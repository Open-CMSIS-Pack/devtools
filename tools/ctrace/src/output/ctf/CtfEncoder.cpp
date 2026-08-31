/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfEncoder.h"

#include "CtfExceptionLaneTracker.h"
#include "CtfMetadataWriter.h"
#include "CtfSchema.h"
#include "CtfStreamWriter.h"
#include "DiagnosticSink.h"
#include "TraceEvent.h"
#include "TraceOutputConfig.h"
#include "TraceSelection.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/** @brief Saturates an internal overflow count to the CTF field width. */
static std::uint32_t ctfOverflowCount(std::uint64_t count)
{
  return static_cast<std::uint32_t>(std::min<std::uint64_t>(count, std::numeric_limits<std::uint32_t>::max()));
}
/** @brief Resolves the configured CTF value representation for one DWT comparator. */
static const CtfSchema::ValueVariant& dwtValueVariant(const ResolvedTraceSource* source, std::uint32_t comparator)
{
  static const ResolvedTraceSource defaults;
  const auto& resolved = source != nullptr ? *source : defaults;
  const auto* variant = CtfSchema::valueVariantForTraceRunType(resolved.dataType, resolved.dataSize);
  if (variant == nullptr) {
    throw std::runtime_error("CTF DWT value for comparator " + std::to_string(comparator) +
                             " has invalid ctrace-run data-type/size metadata");
  }
  return *variant;
}

/** @brief Tests whether two routes describe equivalent CTF source metadata. */
static bool equivalentSourceMetadata(const ResolvedTraceSource& left, const ResolvedTraceSource& right)
{
  // Trace Bus ID identifies the route, not the metadata attached to it.
  return left.type == right.type && left.source == right.source && left.label == right.label &&
         left.address == right.address && left.dataType == right.dataType && left.dataSize == right.dataSize;
}

/** @brief Finds an unambiguous configured source for one event route. */
static const ResolvedTraceSource* resolvedTraceSource(const CtfEncoderConfig& config, const char* type,
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
    if (unique != nullptr && !equivalentSourceMetadata(*unique, candidate)) {
      throw std::runtime_error("CTF cannot resolve conflicting metadata for unformatted " + std::string(type) +
                               " source " + std::to_string(source));
    }
    if (unique == nullptr) {
      unique = &candidate;
    }
  }
  return unique;
}

/** @brief Sign-extends a sample from its configured source width. */
static std::uint32_t signExtendSample(std::uint32_t value, std::uint8_t sourceBytes)
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

/** @brief Writes one sample using its selected CTF variant encoding. */
static void writeVariantValue(CtfStreamWriter::Record& record, std::uint32_t data, std::uint8_t sourceSize,
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

CtfEncoder::CtfEncoder(CtfEncoderConfig config)
  : m_config(std::move(config))
{
  if (m_config.coreClockHz == 0U) {
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
  m_outputDirectory = outputDirectory;
  try {
    m_streamStates.clear();
    m_reportedDwtSizeMismatches.clear();
    m_exceptionLanes.clear();
    m_stream.open(m_outputDirectory / "stream_0", CtfSchema::SwoStreamId);
    m_recording = true;
    auto initialTraceBusIds =
        std::set<std::uint8_t>(m_config.selection.streams.begin(), m_config.selection.streams.end());
    if (initialTraceBusIds.empty()) {
      initialTraceBusIds.insert(0U);
    }
    for (const auto traceBusId : initialTraceBusIds) {
      writeTraceStatusEvent(CtfSchema::value(CtfSchema::TraceStatusReason::TraceStart), traceBusId,
                            m_config.selection.types.empty());
      (void)exceptionLane(traceBusId);
    }
  } catch (...) {
    abort();
    throw;
  }
}

void CtfEncoder::stop()
{
  if (!m_recording) {
    return;
  }
  m_recording = false;
  m_stream.close();
  writeMetadataFile();
}

void CtfEncoder::abort() noexcept
{
  m_recording = false;
  m_stream.abort();
  m_outputDirectory.clear();
}

void CtfEncoder::writeEvent(const TraceEvent& event)
{
  if (!m_recording) {
    return;
  }
  if (!m_config.selection.includesStream(event.traceBusId)) {
    return;
  }
  if (!isTraceEvent<GlobalTimestampTraceEvent>(event) && event.tcyc.has_value()) {
    auto& eventTimestamp = m_streamStates[event.traceBusId].eventTimestamp;
    eventTimestamp = std::max(eventTimestamp, *event.tcyc);
    if (event.quality.has_value() && event.quality->timestampReliable) {
      m_streamStates[event.traceBusId].localTimestampObserved = true;
    }
  }

  const auto selected = traceEventSelectedForOutput(event, m_config.selection);
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
    auto& streamState = m_streamStates[event.traceBusId];
    if (event.quality.has_value()) {
      streamState.overflowCount = std::max(streamState.overflowCount, event.quality->overflowCount);
    } else {
      ++streamState.overflowCount;
    }
    writeTraceStatusEvent(CtfSchema::value(CtfSchema::TraceStatusReason::Overflow), event.traceBusId, selected);
  } else if (isTraceEvent<LocalTimestampTraceEvent>(event)) {
    m_streamStates[event.traceBusId].localTimestampObserved = true;
  } else if (isTraceEvent<SyncTraceEvent>(event)) {
    writeTraceStatusEvent(CtfSchema::value(CtfSchema::TraceStatusReason::Resync), event.traceBusId,
                          m_config.selection.types.empty());
  } else if (const auto* timestamp = traceEventPayload<GlobalTimestampTraceEvent>(event)) {
    if (selected) {
      writeGlobalTimestampEvent(event, *timestamp);
    }
  } else if (const auto* issue = traceEventPayload<TraceIssueEvent>(event)) {
    if (issue->code == TraceIssueCode::DataLoss) {
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

std::uint64_t CtfEncoder::allocateEventTimestamp(std::uint8_t traceBusId)
{
  // CtfStreamWriter applies the final monotonic clamp across the multiplexed
  // CTF stream. This value remains local to the CoreSight Trace Bus ID.
  return m_streamStates[traceBusId].eventTimestamp;
}

void CtfEncoder::writeSoftwareEvent(const TraceEvent& event, const SoftwareTraceEvent& software)
{
  const auto* variant = CtfSchema::valueVariantForTraceRunType("unsigned", software.size);
  if (variant == nullptr) {
    throw std::runtime_error("CTF ITM value has an invalid SWO payload size");
  }
  const auto quality = computeSampleQuality(event);
  const auto eventTimestamp = allocateEventTimestamp(event.traceBusId);
  const auto payloadSize = 1U + 1U + variant->byteSize + 1U + 4U;
  m_stream.writeRecord(CtfSchema::value(CtfSchema::EventId::Itm), eventTimestamp, event.traceBusId, payloadSize,
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
  const auto* source = resolvedTraceSource(m_config, "dwt", event.traceBusId, data.comparator);
  reportDwtSizeMismatch(event, data, source);
  const auto& variant = dwtValueVariant(source, data.comparator);
  const auto hasPc = data.pc.has_value() ? 1U : 0U;
  const auto hasAddress = data.addressLo16.has_value() ? 1U : 0U;
  const auto payloadSize = 1U + 1U + 1U + variant.byteSize + 1U + hasPc * 4U + 1U + hasAddress * 2U + 1U + 4U;
  const auto eventTimestamp = allocateEventTimestamp(event.traceBusId);
  const auto quality = computeSampleQuality(event);
  m_stream.writeRecord(CtfSchema::value(CtfSchema::EventId::DwtValue), eventTimestamp, event.traceBusId, payloadSize,
                       [&](CtfStreamWriter::Record& record) {
                         record.writeU8(static_cast<std::uint8_t>(data.comparator & 0xffU));
                         record.writeU8(CtfSchema::value(data.access == AccessType::Read
                                                             ? CtfSchema::DwtAccess::Read
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
  const auto configuredSize = source != nullptr ? source->dataSize : ResolvedTraceSource{}.dataSize;
  if (configuredSize == data.size || m_config.diagnostics == nullptr ||
      !m_reportedDwtSizeMismatches.insert({event.traceBusId, data.comparator}).second) {
    return;
  }

  std::vector<std::pair<std::string, std::string>> context{
      {"backend", "ctf"},
      {"channel", "DWT" + std::to_string(data.comparator)},
      {"configuredSize", std::to_string(configuredSize)},
      {"swoSize", std::to_string(data.size)},
  };
  context.emplace_back("stream", std::to_string(event.traceBusId));
  m_config.diagnostics->report({
      DiagnosticSink::Severity::Warning,
      "configured ctrace-run size does not match the decoded SWO payload size",
      std::move(context),
  });
}

void CtfEncoder::writeDwtAddrEvent(const TraceEvent& event, const DwtAddressTraceEvent& address)
{
  constexpr auto payloadSize = 1U + 1U + 1U + 4U + 2U + 1U + 4U;
  const auto eventTimestamp = allocateEventTimestamp(event.traceBusId);
  const auto quality = computeSampleQuality(event);
  const auto pc = dwtAddressPc(address);
  const auto addressOffset = dwtAddressOffset(address);
  const auto hasPc = pc.has_value() ? 1U : 0U;
  const auto hasAddress = addressOffset.has_value() ? 1U : 0U;
  m_stream.writeRecord(CtfSchema::value(CtfSchema::EventId::DwtAddress), eventTimestamp, event.traceBusId, payloadSize,
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
  const auto eventTimestamp = allocateEventTimestamp(event.traceBusId);
  m_stream.writeRecord(CtfSchema::value(CtfSchema::EventId::GlobalTimestamp), eventTimestamp, event.traceBusId,
                       payloadSize, [&](CtfStreamWriter::Record& record) {
                         record.writeU64(timestamp.value);
                         record.writeU8(timestamp.clockChange ? 1U : 0U);
                       });
}

void CtfEncoder::writeTraceStatusEvent(std::uint8_t reason, std::uint8_t traceBusId, bool emitEvent)
{
  if (emitEvent) {
    constexpr auto payloadSize = 1U + 4U;
    const auto eventTimestamp = allocateEventTimestamp(traceBusId);
    m_stream.writeRecord(CtfSchema::value(CtfSchema::EventId::TraceStatus), eventTimestamp, traceBusId, payloadSize,
                         [&](CtfStreamWriter::Record& record) {
                           record.writeU8(reason);
                           record.writeU32(ctfOverflowCount(m_streamStates[traceBusId].overflowCount));
                         });
  }

  if (reason == CtfSchema::value(CtfSchema::TraceStatusReason::Overflow) ||
      reason == CtfSchema::value(CtfSchema::TraceStatusReason::DataLoss)) {
    const auto lane = m_exceptionLanes.find(traceBusId);
    if (lane != m_exceptionLanes.end()) {
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
  if (!traceEventSelectedForOutput(selectionEvent, m_config.selection)) {
    return;
  }
  constexpr auto payloadSize = 2U + 1U + 2U;
  const auto eventTimestamp = allocateEventTimestamp(traceBusId);
  const auto encodedAction =
      CtfSchema::value(action == CtfExceptionLaneTracker::RecordAction::Enter ? CtfSchema::ExceptionAction::Entered
                                                                              : CtfSchema::ExceptionAction::Exited);
  m_stream.writeRecord(CtfSchema::value(CtfSchema::EventId::Exception), eventTimestamp, traceBusId, payloadSize,
                       [&](CtfStreamWriter::Record& record) {
                         record.writeU16(static_cast<std::uint16_t>(number & 0xffffU));
                         record.writeU8(encodedAction);
                         record.writeU16(static_cast<std::uint16_t>(number & 0xffffU));
                       });
}

CtfExceptionLaneTracker& CtfEncoder::exceptionLane(std::uint8_t traceBusId)
{
  const auto [lane, inserted] = m_exceptionLanes.try_emplace(traceBusId);
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
  auto& streamState = m_streamStates[event.traceBusId];
  const auto previousOverflowCount = streamState.overflowCount;
  const auto overflowCount = event.quality.has_value() ? event.quality->overflowCount : streamState.overflowCount;
  const auto timestampReliable = event.quality.has_value() ? event.quality->timestampReliable : true;
  const auto overflow = event.quality.has_value() ? event.quality->overflow : overflowCount > previousOverflowCount;
  const auto flags =
      static_cast<std::uint8_t>((overflow ? CtfSchema::SampleFlagOverflow : 0U) |
                                (timestampReliable ? CtfSchema::SampleFlagTimestampReliable : 0U) |
                                (streamState.localTimestampObserved ? 0U : CtfSchema::SampleFlagBeforeFirstTimestamp));
  streamState.overflowCount = std::max(streamState.overflowCount, overflowCount);
  return {flags, ctfOverflowCount(overflowCount)};
}

void CtfEncoder::writeMetadataFile()
{
  std::set<std::uint32_t> observedExceptionNumbers;
  for (const auto& [traceBusId, lane] : m_exceptionLanes) {
    (void)traceBusId;
    observedExceptionNumbers.insert(lane.observedExceptionNumbers().begin(), lane.observedExceptionNumbers().end());
  }
  CtfMetadataWriter::write(m_outputDirectory, m_stream.uuidString(), m_config.coreClockHz, m_config.sources,
                           {
                               observedExceptionNumbers.begin(),
                               observedExceptionNumbers.end(),
                           });
}
