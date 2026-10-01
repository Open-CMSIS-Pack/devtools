/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfEncoder.h"

#include "CtfExceptionLaneTracker.h"
#include "CtfMetadataModel.h"
#include "CtfMetadataWriter.h"
#include "CtfSchema.h"
#include "CtfStreamWriter.h"
#include "DiagnosticSink.h"
#include "TraceEvent.h"
#include "TraceOutputConfig.h"
#include "TraceSelection.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

/** @brief Saturates an internal overflow count to the CTF field width. */
static std::uint32_t ctfOverflowCount(std::uint64_t count)
{
  return static_cast<std::uint32_t>(std::min<std::uint64_t>(count, std::numeric_limits<std::uint32_t>::max()));
}

/** @brief Adapts a normalized route to the legacy CTF event-context field. */
static std::uint8_t legacyCtfTraceBusId(const TraceRouteIdentity& route)
{
  return route.traceBusId.value_or(0U);
}

/** @brief Enforces an explicit normalized route catalogue when one was supplied. */
static void validateConfiguredRoute(const CtfEncoderConfig& config, const TraceRouteIdentity& route)
{
  if (config.routes.empty()) {
    return;
  }
  const auto configured = std::find_if(config.routes.begin(), config.routes.end(),
                                       [&](const TraceRouteIdentity& candidate) { return candidate.id == route.id; });
  if (configured == config.routes.end() || *configured != route) {
    throw std::runtime_error("CTF route identity does not match the configured normalized route catalogue");
  }
}

/** @brief Resolves the configured CTF value representation for one DWT comparator. */
static const CtfSchema::ValueVariant& dwtValueVariant(const CtfSourceDescriptor* source)
{
  static const CtfSourceDescriptor defaults;
  const auto& resolved = source != nullptr ? *source : defaults;
  return *CtfSchema::valueVariantForTraceRunType(resolved.dataType, resolved.dataSize);
}

/** @brief Finds configured source metadata for one exact event route. */
static const CtfSourceDescriptor* resolvedTraceSource(const CtfMetadataModel& metadata, const char* type,
                                                      const TraceRouteIdentity& route, std::uint32_t source)
{
  return metadata.source(route, type, source);
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

/** @brief Resolves the CTF representation of an optional raw DWT address fragment. */
static const CtfSchema::DwtAddressVariant& dwtAddressVariant(const std::optional<DwtAddressFragment>& fragment)
{
  if (!fragment.has_value()) {
    return CtfSchema::DwtAddressVariants.front();
  }
  const auto* variant = CtfSchema::dwtAddressVariantForSize(fragment->size);
  if (variant == nullptr) {
    throw std::runtime_error("CTF DWT address fragment has an invalid SWO payload size");
  }
  return *variant;
}

/** @brief Writes an optional raw DWT address fragment using its exact SWO width. */
static void writeDwtAddress(CtfStreamWriter::Record& record, const std::optional<DwtAddressFragment>& fragment,
                            const CtfSchema::DwtAddressVariant& variant)
{
  record.writeU8(CtfSchema::value(variant.tag));
  const auto value = fragment.has_value() ? fragment->value : 0U;
  if (variant.byteSize == 1U) {
    record.writeU8(static_cast<std::uint8_t>(value & 0xffU));
  } else if (variant.byteSize == 2U) {
    record.writeU16(static_cast<std::uint16_t>(value & 0xffffU));
  } else {
    record.writeU32(value);
  }
}

/** @brief Dispatches semantic payloads to their type-specific CTF encoders. */
struct CtfEncoder::PayloadVisitor {
  CtfEncoder& encoder;
  const TraceEvent& event;
  bool selected;

  void operator()(const SoftwareTraceEvent& software) const
  {
    if (selected) {
      encoder.writeSoftwareEvent(event, software);
    }
  }

  void operator()(const DwtDataTraceEvent& data) const
  {
    if (selected) {
      encoder.writeDwtValueEvent(event, data);
    }
  }

  void operator()(const DwtAddressTraceEvent& address) const
  {
    if (selected) {
      encoder.writeDwtAddrEvent(event, address);
    }
  }

  void operator()(const DwtMatchTraceEvent& match) const
  {
    if (selected) {
      encoder.writeDwtMatchEvent(event, match);
    }
  }

  void operator()(const ExceptionTraceEvent& exception) const
  {
    if (exception.action != ExceptionAction::Unknown) {
      encoder.writeExceptionEvent(event.route, exception);
    }
  }

  void operator()(const DwtEventTraceEvent& counters) const
  {
    if (selected) {
      encoder.writeDwtEvent(event, counters);
    }
  }

  void operator()(const PmuTraceEvent& counters) const
  {
    if (selected) {
      encoder.writePmuEvent(event, counters);
    }
  }

  void operator()(const PcSampleTraceEvent& sample) const
  {
    if (selected) {
      encoder.writePcSampleEvent(event, sample);
    }
  }

  void operator()(const LocalTimestampTraceEvent&) const
  {
    encoder.streamState(event.route).localTimestampObserved = true;
  }

  void operator()(const GlobalTimestampTraceEvent& timestamp) const
  {
    if (selected) {
      encoder.writeGlobalTimestampEvent(event, timestamp);
    }
  }

  void operator()(const OverflowTraceEvent&) const
  {
    auto& routeState = encoder.streamState(event.route);
    if (event.quality.has_value()) {
      routeState.overflowCount = std::max(routeState.overflowCount, event.quality->overflowCount);
    } else {
      ++routeState.overflowCount;
    }
    encoder.writeTraceStatusEvent(CtfSchema::value(CtfSchema::TraceStatusReason::Overflow), event.route, selected);
  }

  void operator()(const SyncTraceEvent&) const
  {
    encoder.writeTraceStatusEvent(CtfSchema::value(CtfSchema::TraceStatusReason::Resync), event.route,
                                  encoder.m_config.selection.types.empty());
  }

  void operator()(const TraceIssueEvent& issue) const
  {
    if (issue.code == TraceIssueCode::DataLoss) {
      encoder.writeTraceStatusEvent(CtfSchema::value(CtfSchema::TraceStatusReason::DataLoss), event.route, selected);
      return;
    }
    if (event.quality.has_value() && event.quality->overflow) {
      encoder.writeTraceStatusEvent(CtfSchema::value(CtfSchema::TraceStatusReason::DataLoss), event.route, selected);
    }
    if (selected) {
      encoder.writeTraceStatusEvent(CtfSchema::value(CtfSchema::TraceStatusReason::DecodeError), event.route, true);
    }
  }
};

CtfEncoder::CtfEncoder(CtfEncoderConfig config)
  : m_config(std::move(config))
{
}

CtfEncoder::~CtfEncoder()
{
  abort();
}

void CtfEncoder::start(const std::filesystem::path& outputDirectory, const CtfUuid& traceUuid)
{
  abort();
  m_outputDirectory = outputDirectory;
  try {
    m_metadata.emplace(traceUuid, m_config.metadata);
    m_completedMetadata.reset();
    m_streams.clear();
    m_bootstrappedRoutes.clear();
    m_streamStates.clear();
    m_reportedDwtSizeMismatches.clear();
    m_exceptionLanes.clear();
    std::map<TraceRouteId, TraceRouteIdentity> initialRoutes;
    const auto addInitialRoute = [&](const TraceRouteIdentity& route) {
      const auto [found, inserted] = initialRoutes.emplace(route.id, route);
      if (!inserted && found->second != route) {
        throw std::runtime_error("CTF configuration contains inconsistent normalized route identities");
      }
    };
    for (const auto& route : m_config.routes) {
      addInitialRoute(route);
    }
    for (const auto& stream : m_metadata->topology().streams) {
      validateConfiguredRoute(m_config, stream.route);
    }
    for (const auto& source : m_metadata->topology().sources) {
      validateConfiguredRoute(m_config, source.route);
    }
    m_recording = true;
    if (m_metadata->isLegacySingleStreamLayout()) {
      const auto& stream = m_metadata->topology().streams.front();
      (void)ensureStreamWriter(stream);
      if (m_config.selection.includesRoute(stream.route)) {
        bootstrapRoute(stream.route);
      }
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
  for (auto& [streamClassId, stream] : m_streams) {
    (void)streamClassId;
    stream.close();
  }
  writeMetadataFile();
}

void CtfEncoder::abort() noexcept
{
  m_recording = false;
  for (auto& [streamClassId, stream] : m_streams) {
    (void)streamClassId;
    stream.abort();
  }
  m_streams.clear();
  m_metadata.reset();
  m_completedMetadata.reset();
  m_bootstrappedRoutes.clear();
  m_streamStates.clear();
  m_reportedDwtSizeMismatches.clear();
  m_exceptionLanes.clear();
  m_outputDirectory.clear();
}

void CtfEncoder::writeEvent(const TraceEvent& event)
{
  if (!m_recording) {
    return;
  }
  if (!m_config.selection.includesRoute(event.route)) {
    return;
  }
  validateConfiguredRoute(m_config, event.route);
  const auto* stream = m_metadata->streamForRoute(event.route);
  if (stream == nullptr) {
    throw std::runtime_error(
        "CTF binary output cannot encode an event route without an exact runtime stream descriptor");
  }
  const auto selected = traceEventSelectedForOutput(event, m_config.selection);
  if (activatesStream(event, selected)) {
    bootstrapRoute(event.route);
  }
  if (!isTraceEvent<GlobalTimestampTraceEvent>(event) && event.tcyc.has_value()) {
    auto& eventTimestamp = streamState(event.route).eventTimestamp;
    eventTimestamp = std::max(eventTimestamp, *event.tcyc);
    if (event.quality.has_value() && event.quality->timestampReliable) {
      streamState(event.route).localTimestampObserved = true;
    }
  }

  std::visit(PayloadVisitor{*this, event, selected}, event.payload);
}

const CtfMetadataModel* CtfEncoder::completedMetadata() const noexcept
{
  return m_completedMetadata ? &*m_completedMetadata : nullptr;
}

void CtfEncoder::writePcSampleEvent(const TraceEvent& event, const PcSampleTraceEvent& sample)
{
  const auto isPc = sample.kind == PcSampleKind::Pc;
  const auto isSleeping = sample.kind == PcSampleKind::Sleep;
  const auto isProhibited = sample.kind == PcSampleKind::TraceProhibited;
  // A separate event preserves PC_SAMPLE's zero-or-one PC sequence length.
  const auto pcSize = isPc ? 4U : 0U;
  const auto payloadSize = (isProhibited ? 0U : 1U) + pcSize + 1U + 4U;
  const auto eventTimestamp = allocateEventTimestamp(event.route);
  const auto traceBusId = legacyCtfTraceBusId(event.route);
  const auto quality = computeSampleQuality(event);
  const auto eventId = isProhibited ? CtfSchema::EventId::PcSampleProhibited : CtfSchema::EventId::PcSample;
  streamWriter(event.route)
      .writeRecord(CtfSchema::value(eventId), eventTimestamp, traceBusId, payloadSize,
                   [&](CtfStreamWriter::Record& record) {
                     if (!isProhibited) {
                       record.writeU8(CtfSchema::value(isSleeping ? CtfSchema::PcSampleState::Sleep
                                                                 : CtfSchema::PcSampleState::Pc));
                     }
                     if (isPc) {
                       record.writeU32(sample.pc);
                     }
                     record.writeU8(quality.first);
                     record.writeU32(quality.second);
                   });
  const auto streamClassId = streamDescriptor(event.route).streamClassId;
  if (isSleeping) {
    m_metadata->observeGraphicalTopic(streamClassId, CtfGraphicalTopic::ProcessorState);
  }
}

std::uint64_t CtfEncoder::allocateEventTimestamp(const TraceRouteIdentity& route)
{
  // Each route-specific CtfStreamWriter applies its final monotonic clamp.
  return streamState(route).eventTimestamp;
}

CtfEncoder::StreamState& CtfEncoder::streamState(const TraceRouteIdentity& route)
{
  return m_streamStates[route.id];
}

const CtfStreamDescriptor& CtfEncoder::streamDescriptor(const TraceRouteIdentity& route) const
{
  const auto* stream = m_metadata->streamForRoute(route);
  // Public event handling validates the route before any private emission path reaches this lookup.
  assert(stream != nullptr);
  return *stream;
}

CtfStreamWriter& CtfEncoder::ensureStreamWriter(const CtfStreamDescriptor& stream)
{
  const auto [writer, inserted] = m_streams.try_emplace(stream.streamClassId);
  if (inserted) {
    try {
      writer->second.open(m_outputDirectory / ("stream_" + std::to_string(stream.streamClassId.value())),
                          stream.streamClassId, m_metadata->traceUuid(),
                          m_metadata->isLegacySingleStreamLayout() ? CtfStreamWriter::EventContextLayout::Legacy
                                                                   : CtfStreamWriter::EventContextLayout::RouteLabeled);
    } catch (...) {
      m_streams.erase(writer);
      throw;
    }
  }
  return writer->second;
}

CtfStreamWriter& CtfEncoder::streamWriter(const TraceRouteIdentity& route)
{
  const auto& stream = streamDescriptor(route);
  return m_streams.at(stream.streamClassId);
}

bool CtfEncoder::activatesStream(const TraceEvent& event, bool selected) const
{
  if (const auto* exception = traceEventPayload<ExceptionTraceEvent>(event)) {
    return selected && exception->action != ExceptionAction::Unknown;
  }
  if (const auto* counters = traceEventPayload<DwtEventTraceEvent>(event)) {
    return selected && std::any_of(kDwtEventCounters.begin(), kDwtEventCounters.end(), [&](const auto counter) {
             return (counters->counterMask & dwtEventCounterBit(counter)) != 0U;
           });
  }
  if (const auto* counters = traceEventPayload<PmuTraceEvent>(event)) {
    return selected && std::any_of(kPmuEventCounters.begin(), kPmuEventCounters.end(), [&](const auto counter) {
             return (counters->overflowMask & pmuEventCounterBit(counter)) != 0U;
           });
  }
  return selected || (isTraceEvent<SyncTraceEvent>(event) && m_config.selection.types.empty());
}

void CtfEncoder::bootstrapRoute(const TraceRouteIdentity& route)
{
  (void)ensureStreamWriter(streamDescriptor(route));
  (void)streamState(route);
  if (!m_bootstrappedRoutes.insert(route.id).second) {
    return;
  }
  writeTraceStatusEvent(CtfSchema::value(CtfSchema::TraceStatusReason::TraceStart), route,
                        m_config.selection.types.empty());
  (void)exceptionLane(route);
}

void CtfEncoder::writeSoftwareEvent(const TraceEvent& event, const SoftwareTraceEvent& software)
{
  const auto* variant = CtfSchema::valueVariantForTraceRunType("unsigned", software.size);
  if (variant == nullptr) {
    throw std::runtime_error("CTF ITM value has an invalid SWO payload size");
  }
  const auto quality = computeSampleQuality(event);
  const auto eventTimestamp = allocateEventTimestamp(event.route);
  const auto traceBusId = legacyCtfTraceBusId(event.route);
  const auto payloadSize = 1U + 1U + variant->byteSize + 1U + 4U;
  streamWriter(event.route)
      .writeRecord(CtfSchema::value(CtfSchema::EventId::Itm), eventTimestamp, traceBusId, payloadSize,
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
  const auto* source = resolvedTraceSource(*m_metadata, "dwt", event.route, data.comparator);
  reportDwtSizeMismatch(event, data, source);
  const auto& variant = dwtValueVariant(source);
  const auto& pcVariant = dwtAddressVariant(data.pc);
  const auto& addressVariant = dwtAddressVariant(data.address);
  const auto payloadSize =
      1U + 1U + 1U + variant.byteSize + 1U + pcVariant.byteSize + 1U + addressVariant.byteSize + 1U + 4U;
  const auto eventTimestamp = allocateEventTimestamp(event.route);
  const auto traceBusId = legacyCtfTraceBusId(event.route);
  const auto quality = computeSampleQuality(event);
  streamWriter(event.route)
      .writeRecord(CtfSchema::value(CtfSchema::EventId::DwtValue), eventTimestamp, traceBusId, payloadSize,
                   [&](CtfStreamWriter::Record& record) {
                     record.writeU8(static_cast<std::uint8_t>(data.comparator & 0xffU));
                     record.writeU8(CtfSchema::value(data.access == AccessType::Read ? CtfSchema::DwtAccess::Read
                                                                                     : CtfSchema::DwtAccess::Write));
                     record.writeU8(CtfSchema::value(variant.tag));
                     writeVariantValue(record, data.value, data.size, variant);
                     writeDwtAddress(record, data.pc, pcVariant);
                     writeDwtAddress(record, data.address, addressVariant);
                     record.writeU8(quality.first);
                     record.writeU32(quality.second);
                   });
  m_metadata->observeGraphicalTopic(streamDescriptor(event.route).streamClassId, CtfGraphicalTopic::DwtValue);
}

void CtfEncoder::reportDwtSizeMismatch(const TraceEvent& event, const DwtDataTraceEvent& data,
                                       const CtfSourceDescriptor* source)
{
  const auto configuredSize = source != nullptr ? source->dataSize : CtfSourceDescriptor{}.dataSize;
  if (configuredSize == data.size || m_config.diagnostics == nullptr ||
      !m_reportedDwtSizeMismatches.insert({event.route.id, data.comparator}).second) {
    return;
  }

  std::vector<std::pair<std::string, std::string>> context{
      {"backend", "ctf"},
      {"channel", "DWT" + std::to_string(data.comparator)},
      {"configuredSize", std::to_string(configuredSize)},
      {"swoSize", std::to_string(data.size)},
  };
  m_config.diagnostics->report({
      DiagnosticSink::Severity::Warning,
      "configured ctrace-run size does not match the decoded SWO payload size",
      std::move(context),
  });
}

void CtfEncoder::writeDwtAddrEvent(const TraceEvent& event, const DwtAddressTraceEvent& data)
{
  const auto eventTimestamp = allocateEventTimestamp(event.route);
  const auto traceBusId = legacyCtfTraceBusId(event.route);
  const auto quality = computeSampleQuality(event);
  const auto pc = dwtAddressPc(data);
  const auto address = dwtDataAddress(data);
  const auto& pcVariant = dwtAddressVariant(pc);
  const auto& addressVariant = dwtAddressVariant(address);
  const auto payloadSize = 1U + 1U + pcVariant.byteSize + 1U + addressVariant.byteSize + 1U + 4U;
  streamWriter(event.route)
      .writeRecord(CtfSchema::value(CtfSchema::EventId::DwtAddress), eventTimestamp, traceBusId, payloadSize,
                   [&](CtfStreamWriter::Record& record) {
                     record.writeU8(static_cast<std::uint8_t>(data.comparator & 0xffU));
                     writeDwtAddress(record, pc, pcVariant);
                     writeDwtAddress(record, address, addressVariant);
                     record.writeU8(quality.first);
                     record.writeU32(quality.second);
                   });
  if (address.has_value()) {
    m_metadata->observeGraphicalTopic(streamDescriptor(event.route).streamClassId, CtfGraphicalTopic::DwtAddress);
  }
}

void CtfEncoder::writeDwtMatchEvent(const TraceEvent& event, const DwtMatchTraceEvent& match)
{
  constexpr auto payloadSize = 1U + 1U + 4U;
  const auto eventTimestamp = allocateEventTimestamp(event.route);
  const auto traceBusId = legacyCtfTraceBusId(event.route);
  const auto quality = computeSampleQuality(event);
  streamWriter(event.route)
      .writeRecord(CtfSchema::value(CtfSchema::EventId::DwtMatch), eventTimestamp, traceBusId, payloadSize,
                   [&](CtfStreamWriter::Record& record) {
                     record.writeU8(static_cast<std::uint8_t>(match.comparator & 0xffU));
                     record.writeU8(quality.first);
                     record.writeU32(quality.second);
                   });
  m_metadata->observeGraphicalTopic(streamDescriptor(event.route).streamClassId, CtfGraphicalTopic::DwtMatch);
}

void CtfEncoder::writeDwtEvent(const TraceEvent& event, const DwtEventTraceEvent& counters)
{
  constexpr auto payloadSize = 1U + 1U + 4U;
  const auto eventTimestamp = allocateEventTimestamp(event.route);
  const auto traceBusId = legacyCtfTraceBusId(event.route);
  const auto quality = computeSampleQuality(event);
  auto emitted = false;
  for (const auto counter : kDwtEventCounters) {
    const auto counterBit = dwtEventCounterBit(counter);
    if ((counters.counterMask & counterBit) == 0U) {
      continue;
    }
    streamWriter(event.route)
        .writeRecord(CtfSchema::value(CtfSchema::EventId::DwtEvent), eventTimestamp, traceBusId, payloadSize,
                     [&](CtfStreamWriter::Record& record) {
                       record.writeU8(CtfSchema::value(counter));
                       record.writeU8(quality.first);
                       record.writeU32(quality.second);
                     });
    emitted = true;
  }
  if (emitted) {
    m_metadata->observeGraphicalTopic(streamDescriptor(event.route).streamClassId, CtfGraphicalTopic::DwtEvent);
  }
}

void CtfEncoder::writePmuEvent(const TraceEvent& event, const PmuTraceEvent& counters)
{
  constexpr auto payloadSize = 1U + 1U + 4U;
  const auto eventTimestamp = allocateEventTimestamp(event.route);
  const auto traceBusId = legacyCtfTraceBusId(event.route);
  const auto quality = computeSampleQuality(event);
  auto emitted = false;
  for (const auto counter : kPmuEventCounters) {
    if ((counters.overflowMask & pmuEventCounterBit(counter)) == 0U) {
      continue;
    }
    streamWriter(event.route)
        .writeRecord(CtfSchema::value(CtfSchema::EventId::PmuEvent), eventTimestamp, traceBusId, payloadSize,
                     [&](CtfStreamWriter::Record& record) {
                       record.writeU8(CtfSchema::value(counter));
                       record.writeU8(quality.first);
                       record.writeU32(quality.second);
                     });
    emitted = true;
  }
  if (emitted) {
    m_metadata->observeGraphicalTopic(streamDescriptor(event.route).streamClassId, CtfGraphicalTopic::PmuEvent);
  }
}

void CtfEncoder::writeGlobalTimestampEvent(const TraceEvent& event, const GlobalTimestampTraceEvent& timestamp)
{
  constexpr auto payloadSize = 8U + 1U;
  const auto eventTimestamp = allocateEventTimestamp(event.route);
  const auto traceBusId = legacyCtfTraceBusId(event.route);
  streamWriter(event.route)
      .writeRecord(CtfSchema::value(CtfSchema::EventId::GlobalTimestamp), eventTimestamp, traceBusId, payloadSize,
                   [&](CtfStreamWriter::Record& record) {
                     record.writeU64(timestamp.value);
                     record.writeU8(timestamp.clockChange ? 1U : 0U);
                   });
}

void CtfEncoder::writeTraceStatusEvent(std::uint8_t reason, const TraceRouteIdentity& route, bool emitEvent)
{
  if (reason == CtfSchema::value(CtfSchema::TraceStatusReason::Overflow) ||
      reason == CtfSchema::value(CtfSchema::TraceStatusReason::DataLoss)) {
    const auto lane = m_exceptionLanes.find(route.id);
    if (lane != m_exceptionLanes.end()) {
      lane->second.resetForDiscontinuity([this, route](ExceptionNumber number,
                                                       CtfExceptionLaneTracker::RecordAction action,
                                                       CtfExceptionLaneTracker::RecordOrigin origin) {
        emitExceptionRecord(route, number, action, origin);
      });
    }
  }

  if (emitEvent) {
    constexpr auto payloadSize = 1U + 4U;
    const auto eventTimestamp = allocateEventTimestamp(route);
    const auto traceBusId = legacyCtfTraceBusId(route);
    streamWriter(route).writeRecord(CtfSchema::value(CtfSchema::EventId::TraceStatus), eventTimestamp, traceBusId,
                                    payloadSize, [&](CtfStreamWriter::Record& record) {
                                      record.writeU8(reason);
                                      record.writeU32(ctfOverflowCount(streamState(route).overflowCount));
                                    });
  }
}

void CtfEncoder::writeExceptionEvent(const TraceRouteIdentity& route, const ExceptionTraceEvent& exception)
{
  exceptionLane(route).consume(exception,
                               [this, route](ExceptionNumber number, CtfExceptionLaneTracker::RecordAction action,
                                             CtfExceptionLaneTracker::RecordOrigin origin) {
                                 emitExceptionRecord(route, number, action, origin);
                               });
}

void CtfEncoder::emitExceptionRecord(const TraceRouteIdentity& route, ExceptionNumber number,
                                     CtfExceptionLaneTracker::RecordAction action,
                                     CtfExceptionLaneTracker::RecordOrigin origin)
{
  const auto semanticAction =
      action == CtfExceptionLaneTracker::RecordAction::Enter
          ? ExceptionAction::Entered
          : action == CtfExceptionLaneTracker::RecordAction::Exit ? ExceptionAction::Exited : ExceptionAction::Returned;
  TraceEvent selectionEvent{ExceptionTraceEvent{
      number,
      semanticAction,
  }};
  selectionEvent.route = route;
  if (!traceEventSelectedForOutput(selectionEvent, m_config.selection)) {
    return;
  }
  constexpr auto payloadSize = 2U + 1U + 2U + 1U;
  const auto eventTimestamp = allocateEventTimestamp(route);
  const auto traceBusId = legacyCtfTraceBusId(route);
  const auto encodedAction = CtfSchema::value(
      action == CtfExceptionLaneTracker::RecordAction::Enter
          ? CtfSchema::ExceptionAction::Entered
          : action == CtfExceptionLaneTracker::RecordAction::Exit ? CtfSchema::ExceptionAction::Exited
                                                                  : CtfSchema::ExceptionAction::Returned);
  const auto encodedOrigin = CtfSchema::value(origin == CtfExceptionLaneTracker::RecordOrigin::Trace
                                                  ? CtfSchema::ExceptionOrigin::Trace
                                                  : CtfSchema::ExceptionOrigin::Synthetic);
  streamWriter(route).writeRecord(CtfSchema::value(CtfSchema::EventId::Exception), eventTimestamp, traceBusId,
                                  payloadSize, [&](CtfStreamWriter::Record& record) {
                                    record.writeU16(number);
                                    record.writeU8(encodedAction);
                                    record.writeU16(number);
                                    record.writeU8(encodedOrigin);
                                  });
  const auto streamClassId = streamDescriptor(route).streamClassId;
  m_metadata->observeException(streamClassId, number);
  if (origin == CtfExceptionLaneTracker::RecordOrigin::Trace) {
    m_metadata->observeGraphicalTopic(streamClassId, CtfGraphicalTopic::Exception);
  }
}

CtfExceptionLaneTracker& CtfEncoder::exceptionLane(const TraceRouteIdentity& route)
{
  (void)streamState(route);
  const auto [lane, inserted] = m_exceptionLanes.try_emplace(route.id);
  if (inserted) {
    lane->second.startThreadMode([this, route](ExceptionNumber number, CtfExceptionLaneTracker::RecordAction action,
                                               CtfExceptionLaneTracker::RecordOrigin origin) {
      emitExceptionRecord(route, number, action, origin);
    });
  }
  return lane->second;
}

std::pair<std::uint8_t, std::uint32_t> CtfEncoder::computeSampleQuality(const TraceEvent& event)
{
  auto& routeState = streamState(event.route);
  const auto previousOverflowCount = routeState.overflowCount;
  const auto overflowCount = event.quality.has_value() ? event.quality->overflowCount : routeState.overflowCount;
  const auto timestampReliable = event.quality.has_value() ? event.quality->timestampReliable : true;
  const auto overflow = event.quality.has_value() ? event.quality->overflow : overflowCount > previousOverflowCount;
  const auto flags =
      static_cast<std::uint8_t>((overflow ? CtfSchema::SampleFlagOverflow : 0U) |
                                (timestampReliable ? CtfSchema::SampleFlagTimestampReliable : 0U) |
                                (routeState.localTimestampObserved ? 0U : CtfSchema::SampleFlagBeforeFirstTimestamp));
  routeState.overflowCount = std::max(routeState.overflowCount, overflowCount);
  return {flags, ctfOverflowCount(overflowCount)};
}

void CtfEncoder::writeMetadataFile()
{
  std::set<CtfStreamClassId> emittedStreamClassIds;
  for (const auto& [streamClassId, stream] : m_streams) {
    (void)stream;
    emittedStreamClassIds.insert(streamClassId);
  }
  CtfMetadataModel completed = m_metadata->projectToEmittedStreams(emittedStreamClassIds);
  CtfMetadataWriter::write(m_outputDirectory, completed);
  m_completedMetadata.emplace(std::move(completed));
}
