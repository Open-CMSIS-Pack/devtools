/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_CTFENCODER_H
#define CTRACE_SRC_OUTPUT_CTF_CTFENCODER_H

#include "CtfExceptionLaneTracker.h"
#include "CtfMetadataModel.h"
#include "CtfStreamWriter.h"
#include "CtfUuid.h"
#include "TraceSelection.h"
#include "TraceEvent.h"
#include "TraceOutputConfig.h"
#include "TraceRoute.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

class DiagnosticSink;

/** @brief Stores metadata topology, selection, route catalogue, and diagnostics for CTF encoding. */
struct CtfEncoderConfig {
  CtfMetadataTopology metadata;
  TraceSelection selection;
  DiagnosticSink* diagnostics = nullptr;
  std::vector<TraceRouteIdentity> routes;
};

/** @brief Encodes semantic trace events into lazy route-specific CTF streams and one metadata set. */
class CtfEncoder final {
public:
  /** @brief Creates an encoder from validated CTF configuration. */
  explicit CtfEncoder(CtfEncoderConfig config);
  /** @brief Aborts active output before destruction. */
  ~CtfEncoder();

  /** @brief Disables copying because the encoder owns stream state. */
  CtfEncoder(const CtfEncoder&) = delete;
  /** @brief Disables copy assignment because the encoder owns stream state. */
  CtfEncoder& operator=(const CtfEncoder&) = delete;

  /** @brief Starts writing into a prepared CTF directory. */
  void start(const std::filesystem::path& outputDirectory, const CtfUuid& traceUuid);
  /** @brief Completes stream data and writes final metadata. */
  void stop();
  /** @brief Aborts stream output without throwing. */
  void abort() noexcept;
  /** @brief Encodes one selected semantic event. */
  void writeEvent(const TraceEvent& event);
  /** @brief Returns completed emitted metadata after a successful stop. */
  const CtfMetadataModel* completedMetadata() const noexcept;

private:
  /** @brief Dispatches one semantic payload without inflating writeEvent's lifecycle logic. */
  struct PayloadVisitor;

  /** @brief Tracks timestamp and trace-quality state for one output stream. */
  struct StreamState {
    std::uint64_t eventTimestamp = 0;
    std::uint64_t overflowCount = 0;
    bool localTimestampObserved = false;
  };

  /** @brief Allocates the next monotonic event timestamp for one stream. */
  std::uint64_t allocateEventTimestamp(const TraceRouteIdentity& route);
  /** @brief Returns route-local CTF state while rejecting identity mismatches. */
  StreamState& streamState(const TraceRouteIdentity& route);
  /** @brief Returns the exact configured stream descriptor for one normalized route. */
  const CtfStreamDescriptor& streamDescriptor(const TraceRouteIdentity& route) const;
  /** @brief Creates or returns the lazy binary writer for one configured stream. */
  CtfStreamWriter& ensureStreamWriter(const CtfStreamDescriptor& stream);
  /** @brief Returns the already-created binary writer for one normalized route. */
  CtfStreamWriter& streamWriter(const TraceRouteIdentity& route);
  /** @brief Tests whether one event can create selected CTF output. */
  bool activatesStream(const TraceEvent& event, bool selected) const;
  /** @brief Emits the legacy stream-local bootstrap exactly once. */
  void bootstrapRoute(const TraceRouteIdentity& route);
  /** @brief Writes metadata that matches the completed binary stream. */
  void writeMetadataFile();
  /** @brief Emits or applies a trace-status transition. */
  void writeTraceStatusEvent(std::uint8_t reason, const TraceRouteIdentity& route, bool emitEvent = true);
  /** @brief Encodes one ITM software event. */
  void writeSoftwareEvent(const TraceEvent& event, const SoftwareTraceEvent& software);
  /** @brief Encodes one DWT data value event. */
  void writeDwtValueEvent(const TraceEvent& event, const DwtDataTraceEvent& data);
  /** @brief Reports configured and decoded DWT width mismatches once per route. */
  void reportDwtSizeMismatch(const TraceEvent& event, const DwtDataTraceEvent& data, const CtfSourceDescriptor* source);
  /** @brief Encodes one DWT address event. */
  void writeDwtAddrEvent(const TraceEvent& event, const DwtAddressTraceEvent& address);
  /** @brief Encodes one comparator-only DWT match event. */
  void writeDwtMatchEvent(const TraceEvent& event, const DwtMatchTraceEvent& match);
  /** @brief Expands one DWT event-counter mask into individual CTF records. */
  void writeDwtEvent(const TraceEvent& event, const DwtEventTraceEvent& counters);
  /** @brief Expands one PMU trace-on-overflow mask into individual CTF records. */
  void writePmuEvent(const TraceEvent& event, const PmuTraceEvent& counters);
  /** @brief Encodes one periodic PC sample, sleep indication, or trace-prohibited marker. */
  void writePcSampleEvent(const TraceEvent& event, const PcSampleTraceEvent& sample);
  /** @brief Encodes one reconstructed global timestamp event. */
  void writeGlobalTimestampEvent(const TraceEvent& event, const GlobalTimestampTraceEvent& timestamp);
  /** @brief Applies one exception transition to its CTF lane state. */
  void writeExceptionEvent(const TraceRouteIdentity& route, const ExceptionTraceEvent& exception);
  /** @brief Emits one concrete exception lane record. */
  void emitExceptionRecord(const TraceRouteIdentity& route, ExceptionNumber number,
                           CtfExceptionLaneTracker::RecordAction action, CtfExceptionLaneTracker::RecordOrigin origin);
  /** @brief Returns the exception tracker for one stream. */
  CtfExceptionLaneTracker& exceptionLane(const TraceRouteIdentity& route);
  /** @brief Computes CTF sample flags and saturated overflow count. */
  std::pair<std::uint8_t, std::uint32_t> computeSampleQuality(const TraceEvent& event);

  CtfEncoderConfig m_config;
  std::filesystem::path m_outputDirectory;
  std::optional<CtfMetadataModel> m_metadata;
  std::optional<CtfMetadataModel> m_completedMetadata;
  std::map<CtfStreamClassId, CtfStreamWriter> m_streams;
  bool m_recording = false;
  std::set<TraceRouteId> m_bootstrappedRoutes;
  std::map<TraceRouteId, StreamState> m_streamStates;
  std::set<std::pair<TraceRouteId, std::uint32_t>> m_reportedDwtSizeMismatches;
  std::map<TraceRouteId, CtfExceptionLaneTracker> m_exceptionLanes;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_CTFENCODER_H
