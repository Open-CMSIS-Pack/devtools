/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfEncoder.hpp"

#include "CtfExceptionLaneTracker.hpp"
#include "CtfMetadataWriter.hpp"
#include "CtfSchema.hpp"
#include "CtfStreamWriter.hpp"
#include "DiagnosticSink.hpp"
#include "TraceEvent.hpp"
#include "TraceOutputConfig.hpp"
#include "TraceSelection.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

std::uint32_t ctfOverflowCount(std::uint64_t count)
{
  return static_cast<std::uint32_t>(std::min<std::uint64_t>(count, std::numeric_limits<std::uint32_t>::max()));
}
std::uint64_t holdMonotonicTimestamp(std::uint64_t timestamp, std::optional<std::uint64_t>& lastTimestamp)
{
  if (lastTimestamp.has_value() && timestamp < *lastTimestamp) {
    timestamp = *lastTimestamp;
  }
  lastTimestamp = timestamp;
  return timestamp;
}

const CtfSchema::ValueVariant& dwtValueVariant(const ResolvedTraceSource* source, std::uint32_t comparator)
{
  static const ResolvedTraceSource defaults;
  const auto& resolved = source != nullptr ? *source : defaults;
  const auto* variant = CtfSchema::valueVariantForTraceRunType(resolved.valueType, resolved.valueSize);
  if (variant == nullptr) {
    throw std::runtime_error("CTF DWT value for comparator " + std::to_string(comparator) +
                             " has invalid ctrace-run data.symbol-type/data.symbol-size metadata");
  }
  return *variant;
}

const ResolvedTraceSource* resolvedTraceSource(const CtfEncoderConfig& config, const char* type,
                                               std::uint8_t traceBusId, std::uint32_t source)
{
  const auto exact =
      std::find_if(config.sources.begin(), config.sources.end(), [&](const ResolvedTraceSource& candidate) {
        return candidate.type == type && candidate.source == source && candidate.traceBusId == traceBusId;
      });
  if (exact != config.sources.end() || traceBusId != 0U) {
    return exact == config.sources.end() ? nullptr : &*exact;
  }

  const ResolvedTraceSource* unique = nullptr;
  for (const auto& candidate : config.sources) {
    if (candidate.type != type || candidate.source != source) {
      continue;
    }
    if (unique != nullptr) {
      return nullptr;
    }
    unique = &candidate;
  }
  return unique;
}

std::uint32_t signExtendSample(std::uint32_t value, std::uint8_t sourceBytes)
{
  if (sourceBytes >= 4U) {
    return value;
  }
  const auto bits = sourceBytes * 8U;
  const auto signBit = 1U << (bits - 1U);
  const auto mask = (signBit << 1U) - 1U;
  const auto v = value & mask;
  return (v ^ signBit) - signBit;
}

void writeVariantValue(CtfStreamWriter::Record& record, std::uint32_t data, std::uint8_t sourceSize,
                       const CtfSchema::ValueVariant& info)
{
  if (info.floatingPoint) {
    record.writeU32(data);
    return;
  }
  const auto value = info.signedInteger ? signExtendSample(data, sourceSize) : data;
  if (info.byteSize == 1U) {
    record.writeU8(static_cast<std::uint8_t>(value & 0xffU));
  } else if (info.byteSize == 2U) {
    record.writeU16(static_cast<std::uint16_t>(value & 0xffffU));
  } else {
    record.writeU32(value);
  }
}

} // namespace

CtfEncoder::CtfEncoder(CtfEncoderConfig config) : config_(std::move(config))
{
  if (config_.coreClockHz == 0U) {
    throw std::invalid_argument("CTF output requires a non-zero timestamps.clock");
  }
}

CtfEncoder::~CtfEncoder()
{
  abort();
}

void CtfEncoder::start(const std::filesystem::path& outputDirectory)
{
  abort();
  outputDirectory_ = outputDirectory;
  try {
    resetEventTimestamps();
    overflowCounts_.clear();
    localTimestampTraceBusIds_.clear();
    reportedDwtSizeMismatches_.clear();
    exceptionLanes_.clear();
    stream_.open(outputDirectory_ / "stream_0", CtfSchema::SwoStreamId);
    recording_ = true;
    writeTraceStatusEvent(CtfSchema::value(CtfSchema::TraceStatusReason::TraceStart), 0U, config_.selection.empty());
    (void)exceptionLane(0U);
  } catch (...) {
    abort();
    throw;
  }
}

void CtfEncoder::stop()
{
  if (!recording_) {
    return;
  }
  recording_ = false;
  stream_.close();
  writeMetadataFile();
}

void CtfEncoder::abort() noexcept
{
  recording_ = false;
  stream_.abort();
  outputDirectory_.clear();
}

void CtfEncoder::writeEvent(const TraceEvent& event)
{
  if (!recording_) {
    return;
  }
  if (!isTraceEvent<GlobalTimestampTraceEvent>(event) && event.tcyc.has_value()) {
    observeInputTimestamp(*event.tcyc);
    if (event.quality.has_value() && event.quality->timestampReliable) {
      localTimestampTraceBusIds_.insert(event.traceBusId);
    }
  }

  const auto selected = traceEventSelectedForOutput(event, config_.selection);
  if (const auto* software = traceEventPayload<SoftwareTraceEvent>(event)) {
    if (selected) {
      writeSoftwareEvent(event, *software);
    }
  } else if (const auto* exception = traceEventPayload<ExceptionTraceEvent>(event)) {
    if (exception->action != ExceptionAction::Unknown) {
      writeExceptionEvent(event.traceBusId, *exception);
    }
  } else if (const auto* data = traceEventPayload<DwtDataTraceEvent>(event)) {
    if (selected) {
      writeDwtValueEvent(event, *data);
    }
  } else if (const auto* address = traceEventPayload<DwtAddressTraceEvent>(event)) {
    if (selected) {
      writeDwtAddrEvent(event, *address);
    }
  } else if (isTraceEvent<OverflowTraceEvent>(event)) {
    if (event.quality.has_value()) {
      auto& overflowCount = overflowCounts_[event.traceBusId];
      overflowCount = std::max(overflowCount, event.quality->overflowCount);
    } else {
      ++overflowCounts_[event.traceBusId];
    }
    writeTraceStatusEvent(CtfSchema::value(CtfSchema::TraceStatusReason::Overflow), event.traceBusId, selected);
  } else if (isTraceEvent<LocalTimestampTraceEvent>(event)) {
    localTimestampTraceBusIds_.insert(event.traceBusId);
    if (event.tcyc.has_value()) {
      observeInputTimestamp(*event.tcyc);
    }
  } else if (isTraceEvent<SyncTraceEvent>(event)) {
    writeTraceStatusEvent(CtfSchema::value(CtfSchema::TraceStatusReason::Resync), event.traceBusId,
                          config_.selection.empty());
  } else if (const auto* timestamp = traceEventPayload<GlobalTimestampTraceEvent>(event)) {
    if (selected) {
      writeGlobalTimestampEvent(event, *timestamp);
    }
  } else if (const auto* issue = traceEventPayload<TraceIssueEvent>(event)) {
    if (issue->code == "data-loss") {
      writeTraceStatusEvent(CtfSchema::value(CtfSchema::TraceStatusReason::DataLoss), event.traceBusId, selected);
    } else {
      if (event.quality.has_value() && event.quality->overflow) {
        writeTraceStatusEvent(CtfSchema::value(CtfSchema::TraceStatusReason::DataLoss), event.traceBusId, selected);
      }
      if (selected) {
        writeTraceStatusEvent(CtfSchema::value(CtfSchema::TraceStatusReason::DecodeError), event.traceBusId, true);
      }
    }
  }
}

void CtfEncoder::resetEventTimestamps()
{
  currentEventTimestamp_ = 0U;
  lastInputTimestamp_.reset();
  lastEventTimestamp_.reset();
}

void CtfEncoder::observeInputTimestamp(std::uint64_t timestamp)
{
  currentEventTimestamp_ = holdMonotonicTimestamp(timestamp, lastInputTimestamp_);
}

std::uint64_t CtfEncoder::allocateEventTimestamp()
{
  currentEventTimestamp_ = holdMonotonicTimestamp(currentEventTimestamp_, lastEventTimestamp_);
  return currentEventTimestamp_;
}

void CtfEncoder::writeSoftwareEvent(const TraceEvent& event, const SoftwareTraceEvent& software)
{
  const auto* variant = CtfSchema::valueVariantForTraceRunType("unsigned int", software.size);
  if (variant == nullptr) {
    throw std::runtime_error("CTF ITM value has an invalid SWO payload size");
  }
  const auto quality = computeSampleQuality(event);
  const auto eventTimestamp = allocateEventTimestamp();
  const auto payloadSize = 1U + 1U + variant->byteSize + 1U + 4U;
  stream_.writeRecord(CtfSchema::value(CtfSchema::EventId::Itm), eventTimestamp, event.traceBusId, payloadSize,
                      [&](CtfStreamWriter::Record& record) {
                        record.writeU8(static_cast<std::uint8_t>(software.channel & 0xffU));
                        record.writeU8(CtfSchema::value(variant->tag));
                        writeVariantValue(record, software.value, software.size, *variant);
                        record.writeU8(quality.first);
                        record.writeU32(quality.second);
                      });
}

void CtfEncoder::writeDwtValueEvent(const TraceEvent& event, const DwtDataTraceEvent& data)
{
  const auto* source = resolvedTraceSource(config_, "dwt", event.traceBusId, data.comparator);
  reportDwtSizeMismatch(event, data, source);
  const auto& variant = dwtValueVariant(source, data.comparator);
  const auto hasPc = data.pc.has_value() ? 1U : 0U;
  const auto hasAddress = data.addressLo16.has_value() ? 1U : 0U;
  const auto payloadSize = 1U + 1U + 1U + variant.byteSize + 1U + hasPc * 4U + 1U + hasAddress * 2U + 1U + 4U;
  const auto eventTimestamp = allocateEventTimestamp();
  const auto quality = computeSampleQuality(event);
  stream_.writeRecord(CtfSchema::value(CtfSchema::EventId::DwtValue), eventTimestamp, event.traceBusId, payloadSize,
                      [&](CtfStreamWriter::Record& record) {
                        record.writeU8(static_cast<std::uint8_t>(data.comparator & 0xffU));
                        record.writeU8(CtfSchema::value(data.access == AccessType::Read ? CtfSchema::DwtAccess::Read
                                                                                        : CtfSchema::DwtAccess::Write));
                        record.writeU8(CtfSchema::value(variant.tag));
                        writeVariantValue(record, data.value, data.size, variant);
                        record.writeU8(static_cast<std::uint8_t>(hasPc));
                        if (hasPc != 0U) {
                          record.writeU32(*data.pc);
                        }
                        record.writeU8(static_cast<std::uint8_t>(hasAddress));
                        if (hasAddress != 0U) {
                          record.writeU16(static_cast<std::uint16_t>(*data.addressLo16 & 0xffffU));
                        }
                        record.writeU8(quality.first);
                        record.writeU32(quality.second);
                      });
}

void CtfEncoder::reportDwtSizeMismatch(const TraceEvent& event, const DwtDataTraceEvent& data,
                                       const ResolvedTraceSource* source)
{
  const auto configuredSize = source != nullptr ? source->valueSize : ResolvedTraceSource{}.valueSize;
  if (configuredSize == data.size || config_.diagnostics == nullptr ||
      !reportedDwtSizeMismatches_.insert({event.traceBusId, data.comparator}).second) {
    return;
  }

  std::vector<std::pair<std::string, std::string>> context{
      {"backend", "ctf"},
      {"channel", "DWT" + std::to_string(data.comparator)},
      {"configuredSize", std::to_string(configuredSize)},
      {"swoSize", std::to_string(data.size)},
  };
  context.emplace_back("stream", std::to_string(event.traceBusId));
  config_.diagnostics->report({
      DiagnosticSink::Severity::Warning,
      DiagnosticSink::Category::Output,
      "dwt-symbol-size-mismatch",
      "configured ctrace-run data.symbol-size does not match the decoded SWO payload size",
      std::move(context),
  });
}

void CtfEncoder::writeDwtAddrEvent(const TraceEvent& event, const DwtAddressTraceEvent& address)
{
  constexpr auto payloadSize = 1U + 1U + 1U + 4U + 2U + 1U + 4U;
  const auto eventTimestamp = allocateEventTimestamp();
  const auto quality = computeSampleQuality(event);
  const auto pc = dwtAddressPc(address);
  const auto addressOffset = dwtAddressOffset(address);
  const auto hasPc = pc.has_value() ? 1U : 0U;
  const auto hasAddress = addressOffset.has_value() ? 1U : 0U;
  stream_.writeRecord(CtfSchema::value(CtfSchema::EventId::DwtAddress), eventTimestamp, event.traceBusId, payloadSize,
                      [&](CtfStreamWriter::Record& record) {
                        record.writeU8(static_cast<std::uint8_t>(address.comparator & 0xffU));
                        record.writeU8(static_cast<std::uint8_t>(hasPc));
                        record.writeU8(static_cast<std::uint8_t>(hasAddress));
                        record.writeU32(pc.value_or(0U));
                        record.writeU16(static_cast<std::uint16_t>(addressOffset.value_or(0U) & 0xffffU));
                        record.writeU8(quality.first);
                        record.writeU32(quality.second);
                      });
}

void CtfEncoder::writeGlobalTimestampEvent(const TraceEvent& event, const GlobalTimestampTraceEvent& timestamp)
{
  constexpr auto payloadSize = 8U + 1U;
  const auto eventTimestamp = allocateEventTimestamp();
  stream_.writeRecord(CtfSchema::value(CtfSchema::EventId::GlobalTimestamp), eventTimestamp, event.traceBusId,
                      payloadSize, [&](CtfStreamWriter::Record& record) {
                        record.writeU64(timestamp.value);
                        record.writeU8(timestamp.clockChange ? 1U : 0U);
                      });
}

void CtfEncoder::writeTraceStatusEvent(std::uint8_t reason, std::uint8_t traceBusId, bool emitEvent)
{
  if (!recording_) {
    return;
  }
  if (emitEvent) {
    constexpr auto payloadSize = 1U + 4U;
    const auto eventTimestamp = allocateEventTimestamp();
    stream_.writeRecord(CtfSchema::value(CtfSchema::EventId::TraceStatus), eventTimestamp, traceBusId, payloadSize,
                        [&](CtfStreamWriter::Record& record) {
                          record.writeU8(reason);
                          record.writeU32(ctfOverflowCount(overflowCounts_[traceBusId]));
                        });
  }

  if (reason == CtfSchema::value(CtfSchema::TraceStatusReason::Overflow) ||
      reason == CtfSchema::value(CtfSchema::TraceStatusReason::Resync) ||
      reason == CtfSchema::value(CtfSchema::TraceStatusReason::DataLoss)) {
    const auto lane = exceptionLanes_.find(traceBusId);
    if (lane != exceptionLanes_.end()) {
      lane->second.resetForDiscontinuity(
          [this, traceBusId](std::uint32_t number, CtfExceptionLaneTracker::RecordAction action) {
            emitExceptionRecord(traceBusId, number, action);
          });
    }
  }
}

void CtfEncoder::writeExceptionEvent(std::uint8_t traceBusId, const ExceptionTraceEvent& exception)
{
  exceptionLane(traceBusId)
      .consume(exception, [this, traceBusId](std::uint32_t number, CtfExceptionLaneTracker::RecordAction action) {
        emitExceptionRecord(traceBusId, number, action);
      });
}

void CtfEncoder::emitExceptionRecord(std::uint8_t traceBusId, std::uint32_t number,
                                     CtfExceptionLaneTracker::RecordAction action)
{
  TraceEvent selectionEvent{ExceptionTraceEvent{
      number,
      action == CtfExceptionLaneTracker::RecordAction::Enter ? ExceptionAction::Entered : ExceptionAction::Exited,
  }};
  selectionEvent.traceBusId = traceBusId;
  if (!traceEventSelectedForOutput(selectionEvent, config_.selection)) {
    return;
  }
  constexpr auto payloadSize = 2U + 1U + 2U;
  const auto eventTimestamp = allocateEventTimestamp();
  const auto encodedAction =
      CtfSchema::value(action == CtfExceptionLaneTracker::RecordAction::Enter ? CtfSchema::ExceptionAction::Entered
                                                                              : CtfSchema::ExceptionAction::Exited);
  stream_.writeRecord(CtfSchema::value(CtfSchema::EventId::Exception), eventTimestamp, traceBusId, payloadSize,
                      [&](CtfStreamWriter::Record& record) {
                        record.writeU16(static_cast<std::uint16_t>(number & 0xffffU));
                        record.writeU8(encodedAction);
                        record.writeU16(static_cast<std::uint16_t>(number & 0xffffU));
                      });
}

CtfExceptionLaneTracker& CtfEncoder::exceptionLane(std::uint8_t traceBusId)
{
  const auto [lane, inserted] = exceptionLanes_.try_emplace(traceBusId);
  if (inserted) {
    lane->second.startThreadMode(
        [this, traceBusId](std::uint32_t number, CtfExceptionLaneTracker::RecordAction action) {
          emitExceptionRecord(traceBusId, number, action);
        });
  }
  return lane->second;
}

std::pair<std::uint8_t, std::uint32_t> CtfEncoder::computeSampleQuality(const TraceEvent& event)
{
  auto& currentOverflowCount = overflowCounts_[event.traceBusId];
  const auto previousOverflowCount = currentOverflowCount;
  const auto overflowCount = event.quality.has_value() ? event.quality->overflowCount : currentOverflowCount;
  const auto timestampReliable = event.quality.has_value() ? event.quality->timestampReliable : true;
  const auto overflow = event.quality.has_value() ? event.quality->overflow : overflowCount > previousOverflowCount;
  const auto flags =
      static_cast<std::uint8_t>((overflow ? CtfSchema::SampleFlagOverflow : 0U) |
                                (timestampReliable ? CtfSchema::SampleFlagTimestampReliable : 0U) |
                                (localTimestampTraceBusIds_.find(event.traceBusId) != localTimestampTraceBusIds_.end()
                                     ? 0U
                                     : CtfSchema::SampleFlagBeforeFirstTimestamp));
  currentOverflowCount = std::max(currentOverflowCount, overflowCount);
  return {flags, ctfOverflowCount(overflowCount)};
}

void CtfEncoder::writeMetadataFile()
{
  std::set<std::uint32_t> observedExceptionNumbers;
  for (const auto& [traceBusId, lane] : exceptionLanes_) {
    (void)traceBusId;
    observedExceptionNumbers.insert(lane.observedExceptionNumbers().begin(), lane.observedExceptionNumbers().end());
  }
  CtfMetadataWriter::write(outputDirectory_, stream_.uuidString(), config_.coreClockHz, config_.sources,
                           {
                               observedExceptionNumbers.begin(),
                               observedExceptionNumbers.end(),
                           });
}
