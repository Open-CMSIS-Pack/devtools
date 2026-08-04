/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CortexMPostDecoder.h"

#include "DwtPacketDecoder.h"
#include "OpenCsdTraceElement.h"
#include "SaturatingArithmetic.h"
#include "TraceEvent.h"

#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

CortexMPostDecoder::CortexMPostDecoder(TraceEventSink& eventSink) : m_eventSink(eventSink) {}

void CortexMPostDecoder::append(OpenCsdTraceElement element)
{
  mapTimestampSegment(element);
  switch (element.kind) {
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

  switch (element.kind) {
  case OpenCsdTraceElement::Kind::LocalTimestamp:
    if (element.tcyc.has_value()) {
      const auto mapped = SaturatingArithmetic::add(m_timestampSegmentBase, *element.tcyc);
      element.tcyc = mapped;
      m_mappedTimeline = mapped;
      m_timelineKnown = true;
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
  m_timestampSegmentBase = m_timelineKnown ? m_mappedTimeline : 0U;
}

void CortexMPostDecoder::finish()
{
  finalizePendingDiscontinuityIssues(std::nullopt);
  flushPendingDataTrace(currentTraceStatus());
  flushPendingEvents(std::nullopt, currentTraceStatus());
}

std::uint64_t CortexMPostDecoder::eventCount() const
{
  return m_eventCount;
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
  m_dwtDecoder.reset();

  TraceEvent event{OverflowTraceEvent{
      "overflow: new timestamp segment; time across boundary may be unreliable",
  }};
  event.index = element.sourceIndex;
  event.traceBusId = element.traceBusId;
  event.tcyc = m_timelineKnown ? std::optional<std::uint64_t>(m_currentTcyc) : std::nullopt;
  event.quality = TraceQuality{true, false, m_overflowCount};
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
  m_pendingEvents.push_back(std::move(event));
}

void CortexMPostDecoder::appendDiscontinuity(const OpenCsdTraceElement& element)
{
  const auto status = markDiscontinuity();

  queueDiscontinuityIssue(
      element.sourceIndex, element.traceBusId, status, element.issueCode.empty() ? "data-loss" : element.issueCode,
      element.errorMessage.empty() ? "data loss/resync boundary; timestamps across this point may not match"
                                   : element.errorMessage,
      element.rawBytesConsumed);
}

void CortexMPostDecoder::appendError(const OpenCsdTraceElement& element)
{
  const auto status = element.discontinuity ? markDiscontinuity() : currentTraceStatus();

  TraceEvent event{TraceIssueEvent{
      element.issueCode.empty() ? "opencsd-decode-error" : element.issueCode,
      element.issueSeverity,
      element.errorMessage,
      element.rawBytesConsumed,
      std::nullopt,
  }};
  event.index = element.sourceIndex;
  event.traceBusId = element.traceBusId;
  event.tcyc = m_currentTcyc;
  event.quality = status;
  if (element.awaitingResumeTimestamp) {
    std::get<TraceIssueEvent>(event.payload).lastValidTcyc = m_currentTcyc;
    m_pendingEvents.push_back(std::move(event));
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
  event.tcyc = m_currentTcyc;
  event.quality = currentTraceStatus(element.overflow);
  m_pendingEvents.push_back(std::move(event));
}

void CortexMPostDecoder::appendDwt(const OpenCsdTraceElement& element)
{
  auto events = m_dwtDecoder.decode({
      element.sourceIndex,
      element.traceBusId,
      static_cast<std::uint8_t>(element.discriminator),
      element.size,
      element.value,
      m_currentTcyc,
      currentTraceStatus(element.overflow),
  });
  appendPendingEvents(std::move(events));
}

void CortexMPostDecoder::appendTimestamp(const OpenCsdTraceElement& element)
{
  m_currentTcyc = element.tcyc.value_or(0);
  m_timestampReliable = true;

  const auto status = statusResolvedByTimestamp(element.timestampRelation);
  finalizePendingDiscontinuityIssues(m_currentTcyc);
  flushPendingDataTrace(status);
  flushPendingEvents(m_currentTcyc, status);

  TraceEvent event{LocalTimestampTraceEvent{}};
  event.index = element.sourceIndex;
  event.traceBusId = element.traceBusId;
  event.tcyc = m_currentTcyc;
  emitEvent(event);

  m_timestampReliable = true;
  m_dataLossSinceLastTimestamp = false;
}

void CortexMPostDecoder::flushPendingDataTrace(const TraceQuality& quality)
{
  appendPendingEvents(m_dwtDecoder.flush(quality, m_currentTcyc));
}

void CortexMPostDecoder::flushPendingEvents(std::optional<std::uint64_t> tcyc, const TraceQuality& quality)
{
  for (auto& event : m_pendingEvents) {
    const auto* issue = traceEventPayload<TraceIssueEvent>(event);
    const auto hasLastValidTcyc = issue != nullptr && issue->lastValidTcyc.has_value();
    const auto isControlEvent = isTraceEvent<SyncTraceEvent>(event) || isTraceEvent<GlobalTimestampTraceEvent>(event);
    if (tcyc.has_value() && !isControlEvent && !hasLastValidTcyc) {
      event.tcyc = tcyc;
    }
    if (!isControlEvent && !hasLastValidTcyc) {
      event.quality = quality;
    }
    emitEvent(event);
  }
  m_pendingEvents.clear();
}

void CortexMPostDecoder::appendPendingEvents(std::vector<TraceEvent> events)
{
  m_pendingEvents.insert(m_pendingEvents.end(), std::make_move_iterator(events.begin()),
                        std::make_move_iterator(events.end()));
}

void CortexMPostDecoder::queueDiscontinuityIssue(std::uint64_t sourceIndex, std::uint8_t traceBusId,
                                                 const TraceQuality& quality, const std::string& issueCode,
                                                 const std::string& message,
                                                 std::optional<std::uint64_t> rawBytesConsumed)
{
  TraceEvent event{TraceIssueEvent{
      issueCode,
      TraceIssueSeverity::Error,
      message,
      rawBytesConsumed,
      m_currentTcyc,
  }};
  event.index = sourceIndex;
  event.traceBusId = traceBusId;
  event.tcyc = m_currentTcyc;
  event.quality = quality;
  m_pendingEvents.push_back(std::move(event));
}

void CortexMPostDecoder::finalizePendingDiscontinuityIssues(std::optional<std::uint64_t> firstResumedTcyc)
{
  for (auto& event : m_pendingEvents) {
    auto* issue = traceEventPayload<TraceIssueEvent>(event);
    if (issue == nullptr || !issue->lastValidTcyc.has_value()) {
      continue;
    }
    issue->message += "; timestamp " + std::to_string(*issue->lastValidTcyc) + " .. " +
                      (firstResumedTcyc.has_value() ? std::to_string(*firstResumedTcyc) : "unknown") + ".";
  }
}

void CortexMPostDecoder::queueOrEmitWhileAwaitingTimestamp(TraceEvent event)
{
  if (!m_pendingEvents.empty()) {
    m_pendingEvents.push_back(std::move(event));
    return;
  }
  emitEvent(event);
}

TraceQuality CortexMPostDecoder::markDiscontinuity()
{
  noteOverflow();
  const auto status = currentTraceStatus();
  finalizePendingDiscontinuityIssues(std::nullopt);
  flushPendingDataTrace(status);
  flushPendingEvents(std::nullopt, status);
  m_dwtDecoder.reset();
  m_pendingEvents.clear();
  m_timestampReliable = false;
  m_dataLossSinceLastTimestamp = true;
  return status;
}

void CortexMPostDecoder::emitEvent(const TraceEvent& event)
{
  m_eventSink.append(event);
  ++m_eventCount;
}

TraceQuality CortexMPostDecoder::currentTraceStatus(bool packetOverflow) const
{
  return {
      m_dataLossSinceLastTimestamp || !m_timestampReliable || packetOverflow,
      m_timestampReliable && !m_dataLossSinceLastTimestamp && !packetOverflow,
      m_overflowCount,
  };
}

TraceQuality CortexMPostDecoder::statusResolvedByTimestamp(LocalTimestampRelation relation) const
{
  const auto dataIntact = !m_dataLossSinceLastTimestamp;
  const auto payloadDelayed = relation == LocalTimestampRelation::PayloadDelayed ||
                              relation == LocalTimestampRelation::TimestampAndPayloadDelayed;
  return {!dataIntact, dataIntact && !payloadDelayed, m_overflowCount};
}

void CortexMPostDecoder::noteOverflow()
{
  ++m_overflowCount;
  m_dataLossSinceLastTimestamp = true;
  m_timestampReliable = false;
}
