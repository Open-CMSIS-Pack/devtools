/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_CTFENCODER_H
#define CTRACE_SRC_OUTPUT_CTF_CTFENCODER_H

#include "CtfExceptionLaneTracker.h"
#include "CtfStreamWriter.h"
#include "TraceSelection.h"
#include "TraceEvent.h"
#include "TraceOutputConfig.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <utility>
#include <vector>

class DiagnosticSink;

/** @brief Stores the clock, selection, sources, and diagnostics for CTF encoding. */
struct CtfEncoderConfig {
  std::uint64_t coreClockHz = 0;
  TraceSelection selection;
  std::vector<ResolvedTraceSource> sources;
  DiagnosticSink* diagnostics = nullptr;
};

/** @brief Encodes semantic trace events into one CTF stream and metadata set. */
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
  void start(const std::filesystem::path& outputDirectory);
  /** @brief Completes stream data and writes final metadata. */
  void stop();
  /** @brief Aborts stream output without throwing. */
  void abort() noexcept;
  /** @brief Encodes one selected semantic event. */
  void writeEvent(const TraceEvent& event);

private:
  /** @brief Tracks timestamp and trace-quality state for one output stream. */
  struct StreamState {
    std::uint64_t eventTimestamp = 0;
    std::uint64_t overflowCount = 0;
    bool localTimestampObserved = false;
  };

  /** @brief Allocates the next monotonic event timestamp for one stream. */
  std::uint64_t allocateEventTimestamp(std::uint8_t traceBusId);
  /** @brief Writes metadata that matches the completed binary stream. */
  void writeMetadataFile();
  /** @brief Emits or applies a trace-status transition. */
  void writeTraceStatusEvent(std::uint8_t reason, std::uint8_t traceBusId, bool emitEvent = true);
  /** @brief Encodes one ITM software event. */
  void writeSoftwareEvent(const TraceEvent& event, const SoftwareTraceEvent& software);
  /** @brief Encodes one DWT data value event. */
  void writeDwtValueEvent(const TraceEvent& event, const DwtDataTraceEvent& data);
  /** @brief Reports configured and decoded DWT width mismatches once per route. */
  void reportDwtSizeMismatch(const TraceEvent& event, const DwtDataTraceEvent& data, const ResolvedTraceSource* source);
  /** @brief Encodes one DWT address event. */
  void writeDwtAddrEvent(const TraceEvent& event, const DwtAddressTraceEvent& address);
  /** @brief Encodes one periodic PC-sample or processor-sleep event. */
  void writePcSampleEvent(const TraceEvent& event, const PcSampleTraceEvent& sample);
  /** @brief Encodes one reconstructed global timestamp event. */
  void writeGlobalTimestampEvent(const TraceEvent& event, const GlobalTimestampTraceEvent& timestamp);
  /** @brief Applies one exception transition to its CTF lane state. */
  void writeExceptionEvent(std::uint8_t traceBusId, const ExceptionTraceEvent& exception);
  /** @brief Emits one concrete exception lane record. */
  void emitExceptionRecord(std::uint8_t traceBusId, ExceptionNumber number,
                           CtfExceptionLaneTracker::RecordAction action,
                           CtfExceptionLaneTracker::RecordOrigin origin);
  /** @brief Returns the exception tracker for one stream. */
  CtfExceptionLaneTracker& exceptionLane(std::uint8_t traceBusId);
  /** @brief Computes CTF sample flags and saturated overflow count. */
  std::pair<std::uint8_t, std::uint32_t> computeSampleQuality(const TraceEvent& event);

  CtfEncoderConfig m_config;
  std::filesystem::path m_outputDirectory;
  CtfStreamWriter m_stream;
  bool m_recording = false;
  std::map<std::uint8_t, StreamState> m_streamStates;
  std::set<std::pair<std::uint8_t, std::uint32_t>> m_reportedDwtSizeMismatches;
  std::map<std::uint8_t, CtfExceptionLaneTracker> m_exceptionLanes;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_CTFENCODER_H
