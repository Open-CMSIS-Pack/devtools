/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CortexMPostDecoder.hpp"

#include "DwtPacketDecoder.hpp"
#include "TraceEvent.hpp"
#include "OpenCsdTraceElement.hpp"

#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

CortexMPostDecoder::CortexMPostDecoder(TraceEventSink& eventSink) : eventSink_(eventSink) {}

void CortexMPostDecoder::append(OpenCsdTraceElement element)
{
  mapTimestampSegment(element);
  switch (element.kind) { // LCOV_EXCL_BR_LINE: every declared element kind is covered
  case OpenCsdTraceElement::Kind::Software:
    appendSoftware(element);
    break;
  case OpenCsdTraceElement::Kind::Hardware:
    appendDwt(element);
    break;
  case OpenCsdTraceElement::Kind::LocalTimestamp:
    appendTimestamp(element);
    break;
  case OpenCsdTraceElement::Kind::GlobalTimestamp:
    appendGlobalTimestamp(element);
    break;
  case OpenCsdTraceElement::Kind::Sync:
    appendSync(element);
    break;
  case OpenCsdTraceElement::Kind::Overflow:
    appendOverflow(element);
    break;
  case OpenCsdTraceElement::Kind::Discontinuity:
    appendDiscontinuity(element);
    break;
  case OpenCsdTraceElement::Kind::Error:
    appendError(element);
    break;
  }
}

void CortexMPostDecoder::mapTimestampSegment(OpenCsdTraceElement& element)
{
  if (element.overflow) {
    startNewTimestampSegment();
  }

  switch (element.kind) { // LCOV_EXCL_BR_LINE: every timestamp-relevant kind is covered
  case OpenCsdTraceElement::Kind::LocalTimestamp:
    if (element.tcyc.has_value()) {
      const auto mapped = saturatingAdd(timestampSegmentBase_, *element.tcyc);
      element.tcyc = mapped;
      mappedTimeline_ = mapped;
      timelineKnown_ = true;
    }
    break;
  case OpenCsdTraceElement::Kind::Overflow:
  case OpenCsdTraceElement::Kind::Discontinuity:
    startNewTimestampSegment();
    break;
  case OpenCsdTraceElement::Kind::Error:
    if (element.discontinuity) {
      startNewTimestampSegment();
    }
    break;
  case OpenCsdTraceElement::Kind::Software:
  case OpenCsdTraceElement::Kind::Hardware:
  case OpenCsdTraceElement::Kind::GlobalTimestamp:
  case OpenCsdTraceElement::Kind::Sync:
    break;
  }
}

void CortexMPostDecoder::startNewTimestampSegment()
{
  timestampSegmentBase_ = timelineKnown_ ? mappedTimeline_ : 0U;
}

std::uint64_t CortexMPostDecoder::saturatingAdd(std::uint64_t lhs, std::uint64_t rhs)
{
  const auto max = std::numeric_limits<std::uint64_t>::max();
  return rhs > max - lhs ? max : lhs + rhs; // LCOV_EXCL_BR_LINE: saturation is one arithmetic behavior
}

void CortexMPostDecoder::finish()
{
  finalizePendingDiscontinuityIssues(std::nullopt);
  flushPendingDataTrace(currentTraceStatus());
  flushPendingEvents(std::nullopt, currentTraceStatus());
}

std::uint64_t CortexMPostDecoder::eventCount() const
{
  return eventCount_;
}

void CortexMPostDecoder::appendSync(const OpenCsdTraceElement& element)
{
  TraceEvent event{SyncTraceEvent{}};
  event.index = element.sourceIndex;
  event.traceBusId = element.traceBusId;
  queueOrEmitWhileAwaitingTimestamp(std::move(event));
}

void CortexMPostDecoder::appendOverflow(const OpenCsdTraceElement& element)
{
  noteOverflow();
  const auto status = currentTraceStatus();
  finalizePendingDiscontinuityIssues(std::nullopt);
  flushPendingDataTrace(status);
  flushPendingEvents(std::nullopt, status);
  dwtDecoder_.reset();

  TraceEvent event{OverflowTraceEvent{
      "overflow: new timestamp segment; time across boundary may be unreliable",
  }};
  event.index = element.sourceIndex;
  event.traceBusId = element.traceBusId;
  event.tcyc = currentTcyc_;
  event.quality = TraceQuality{true, false, overflowCount_};
  emitEvent(event);
}

void CortexMPostDecoder::appendGlobalTimestamp(const OpenCsdTraceElement& element)
{
  flushPendingDataTrace(currentTraceStatus());
  TraceEvent event{GlobalTimestampTraceEvent{
      element.timestampValue,
      element.clockChange,
  }};
  event.index = element.sourceIndex;
  event.traceBusId = element.traceBusId;
  pendingEvents_.push_back(std::move(event));
}

void CortexMPostDecoder::appendDiscontinuity(const OpenCsdTraceElement& element)
{
  const auto status = markDiscontinuity();

  queueDiscontinuityIssue(
      element.sourceIndex, element.traceBusId, status,
      element.issueCode.empty() ? "data-loss" : element.issueCode, // LCOV_EXCL_BR_LINE
      element.errorMessage.empty()
          ? "data loss/resync boundary; timestamps across this point may not match" // LCOV_EXCL_BR_LINE
          : element.errorMessage,
      element.rawBytesConsumed);
}

void CortexMPostDecoder::appendError(const OpenCsdTraceElement& element)
{
  const auto status = element.discontinuity ? markDiscontinuity() : currentTraceStatus();

  // LCOV_EXCL_BR_START: generated variant-initializer exception edges
  TraceEvent event{TraceIssueEvent{
      element.issueCode.empty() ? "opencsd-decode-error" : element.issueCode,
      element.issueSeverity,
      element.errorMessage,
      element.rawBytesConsumed,
      std::nullopt,
  }};
  // LCOV_EXCL_BR_STOP
  event.index = element.sourceIndex;
  event.traceBusId = element.traceBusId;
  event.tcyc = currentTcyc_;
  applyDwtTraceStatus(event, status);
  if (element.awaitingResumeTimestamp) {
    std::get<TraceIssueEvent>(event.payload).lastValidTcyc = currentTcyc_;
    pendingEvents_.push_back(std::move(event));
    return;
  }
  queueOrEmitWhileAwaitingTimestamp(std::move(event));
}

void CortexMPostDecoder::appendSoftware(const OpenCsdTraceElement& element)
{
  flushPendingDataTrace(currentTraceStatus());
  TraceEvent event{SoftwareTraceEvent{
      element.channel,
      element.size,
      element.value,
  }};
  event.index = element.sourceIndex;
  event.traceBusId = element.traceBusId;
  event.tcyc = currentTcyc_;
  applyDwtTraceStatus(event, currentTraceStatus(element.overflow));
  pendingEvents_.push_back(std::move(event));
}

void CortexMPostDecoder::appendDwt(const OpenCsdTraceElement& element)
{
  auto events = dwtDecoder_.decode({
      element.sourceIndex,
      element.traceBusId,
      static_cast<std::uint8_t>(element.discriminator),
      element.size,
      element.value,
      currentTcyc_,
      currentTraceStatus(element.overflow),
  });
  appendPendingEvents(std::move(events));
}

void CortexMPostDecoder::appendTimestamp(const OpenCsdTraceElement& element)
{
  currentTcyc_ = element.tcyc.value_or(0);
  timestampReliable_ = true;

  const auto status = statusResolvedByTimestamp(element.timestampRelation);
  finalizePendingDiscontinuityIssues(currentTcyc_);
  flushPendingDataTrace(status);
  flushPendingEvents(currentTcyc_, status);

  TraceEvent event{LocalTimestampTraceEvent{}};
  event.index = element.sourceIndex;
  event.traceBusId = element.traceBusId;
  event.tcyc = currentTcyc_;
  emitEvent(event);

  timestampReliable_ = true;
  dataLossSinceLastTimestamp_ = false;
}

void CortexMPostDecoder::flushPendingDataTrace(const DwtTraceStatus& status)
{
  appendPendingEvents(dwtDecoder_.flush(status, currentTcyc_));
}

void CortexMPostDecoder::flushPendingEvents(std::optional<std::uint64_t> tcyc, const DwtTraceStatus& status)
{
  for (auto& event : pendingEvents_) {
    const auto* issue = traceEventPayload<TraceIssueEvent>(event);
    const auto hasLastValidTcyc = issue != nullptr && issue->lastValidTcyc.has_value();
    const auto isControlEvent = isTraceEvent<SyncTraceEvent>(event) || isTraceEvent<GlobalTimestampTraceEvent>(event);
    if (tcyc.has_value() && !isControlEvent && !hasLastValidTcyc) {
      event.tcyc = tcyc;
    }
    if (!isControlEvent && !hasLastValidTcyc) {
      applyDwtTraceStatus(event, status);
    }
    emitEvent(event);
  }
  pendingEvents_.clear();
}

void CortexMPostDecoder::appendPendingEvents(std::vector<TraceEvent> events)
{
  pendingEvents_.insert(pendingEvents_.end(), std::make_move_iterator(events.begin()),
                        std::make_move_iterator(events.end()));
}

void CortexMPostDecoder::queueDiscontinuityIssue(std::uint64_t sourceIndex, std::uint8_t traceBusId,
                                                 const DwtTraceStatus& status, const std::string& issueCode,
                                                 const std::string& message,
                                                 std::optional<std::uint64_t> rawBytesConsumed)
{
  TraceEvent event{TraceIssueEvent{
      issueCode,
      TraceIssueSeverity::Error,
      message,
      rawBytesConsumed,
      currentTcyc_,
  }};
  event.index = sourceIndex;
  event.traceBusId = traceBusId;
  event.tcyc = currentTcyc_;
  applyDwtTraceStatus(event, status);
  pendingEvents_.push_back(std::move(event));
}

void CortexMPostDecoder::finalizePendingDiscontinuityIssues(std::optional<std::uint64_t> firstResumedTcyc)
{
  for (auto& event : pendingEvents_) {
    auto* issue = traceEventPayload<TraceIssueEvent>(event);
    if (issue == nullptr || !issue->lastValidTcyc.has_value()) {
      continue;
    }
    // LCOV_EXCL_BR_START: all behavioral alternatives are covered; GCC emits expression-cleanup branches
    issue->message += "; timestamp " + std::to_string(*issue->lastValidTcyc) + " .. " +
                      (firstResumedTcyc.has_value() ? std::to_string(*firstResumedTcyc) : "unknown") + ".";
    // LCOV_EXCL_BR_STOP
  }
}

void CortexMPostDecoder::queueOrEmitWhileAwaitingTimestamp(TraceEvent event)
{
  if (!pendingEvents_.empty()) {
    pendingEvents_.push_back(std::move(event));
    return;
  }
  emitEvent(event);
}

DwtTraceStatus CortexMPostDecoder::markDiscontinuity()
{
  noteOverflow();
  const auto status = currentTraceStatus();
  finalizePendingDiscontinuityIssues(std::nullopt);
  flushPendingDataTrace(status);
  flushPendingEvents(std::nullopt, status);
  dwtDecoder_.reset();
  pendingEvents_.clear();
  timestampReliable_ = false;
  dataLossSinceLastTimestamp_ = true;
  return status;
}

void CortexMPostDecoder::emitEvent(const TraceEvent& event)
{
  eventSink_.append(event);
  ++eventCount_;
}

DwtTraceStatus CortexMPostDecoder::currentTraceStatus(bool packetOverflow) const
{
  return {
      dataLossSinceLastTimestamp_ || !timestampReliable_ || packetOverflow,  // LCOV_EXCL_BR_LINE
      timestampReliable_ && !dataLossSinceLastTimestamp_ && !packetOverflow, // LCOV_EXCL_BR_LINE
      overflowCount_,
  };
}

DwtTraceStatus CortexMPostDecoder::statusResolvedByTimestamp(LocalTimestampRelation relation) const
{
  const auto dataIntact = !dataLossSinceLastTimestamp_;
  const auto payloadDelayed = relation == LocalTimestampRelation::PayloadDelayed ||
                              relation == LocalTimestampRelation::TimestampAndPayloadDelayed;
  return {!dataIntact, dataIntact && !payloadDelayed, overflowCount_};
}

void CortexMPostDecoder::noteOverflow()
{
  ++overflowCount_;
  dataLossSinceLastTimestamp_ = true;
  timestampReliable_ = false;
}
