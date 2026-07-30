/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "CtfExceptionLaneTracker.hpp"
#include "CtfStreamWriter.hpp"
#include "TraceSelection.hpp"
#include "TraceEvent.hpp"
#include "TraceOutputConfig.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

class DiagnosticSink;

struct CtfEncoderConfig {
  std::uint64_t coreClockHz = 0;
  TraceSelection selection;
  std::vector<ResolvedTraceSource> sources;
  DiagnosticSink* diagnostics = nullptr;
};

class CtfEncoder final {
public:
  explicit CtfEncoder(CtfEncoderConfig config);
  ~CtfEncoder();

  CtfEncoder(const CtfEncoder&) = delete;
  CtfEncoder& operator=(const CtfEncoder&) = delete;

  void start(const std::filesystem::path& outputDirectory);
  void stop();
  void abort() noexcept;
  void writeEvent(const TraceEvent& event);

private:
  void resetEventTimestamps();
  void observeInputTimestamp(std::uint64_t timestamp);
  std::uint64_t allocateEventTimestamp();
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
  std::uint64_t currentEventTimestamp_ = 0;
  std::optional<std::uint64_t> lastInputTimestamp_;
  std::optional<std::uint64_t> lastEventTimestamp_;
  std::map<std::uint8_t, std::uint64_t> overflowCounts_;
  std::set<std::uint8_t> localTimestampTraceBusIds_;
  std::set<std::pair<std::uint8_t, std::uint32_t>> reportedDwtSizeMismatches_;
  std::map<std::uint8_t, CtfExceptionLaneTracker> exceptionLanes_;
};
