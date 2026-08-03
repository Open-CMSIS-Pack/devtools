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

  std::uint64_t allocateEventTimestamp(std::uint8_t traceBusId);
  void writeMetadataFile();
  void writeTraceStatusEvent(std::uint8_t reason, std::uint8_t traceBusId, bool emitEvent = true);
  void writeSoftwareEvent(const TraceEvent& event, const SoftwareTraceEvent& software);
  void writeDwtValueEvent(const TraceEvent& event, const DwtDataTraceEvent& data);
  void reportDwtSizeMismatch(const TraceEvent& event, const DwtDataTraceEvent& data, const ResolvedTraceSource* source);
  void writeDwtAddrEvent(const TraceEvent& event, const DwtAddressTraceEvent& address);
  void writeGlobalTimestampEvent(const TraceEvent& event, const GlobalTimestampTraceEvent& timestamp);
  void writeExceptionEvent(std::uint8_t traceBusId, const ExceptionTraceEvent& exception);
  void emitExceptionRecord(std::uint8_t traceBusId, std::uint32_t number, CtfExceptionLaneTracker::RecordAction action);
  CtfExceptionLaneTracker& exceptionLane(std::uint8_t traceBusId);
  std::pair<std::uint8_t, std::uint32_t> computeSampleQuality(const TraceEvent& event);

  CtfEncoderConfig config_;
  std::filesystem::path outputDirectory_;
  CtfStreamWriter stream_;
  bool recording_ = false;
  std::map<std::uint8_t, StreamState> streamStates_;
  std::set<std::pair<std::uint8_t, std::uint32_t>> reportedDwtSizeMismatches_;
  std::map<std::uint8_t, CtfExceptionLaneTracker> exceptionLanes_;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_CTFENCODER_H
