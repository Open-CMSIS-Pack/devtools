/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_CORTEXMPOSTDECODER_H
#define CTRACE_SRC_DECODE_CORTEXMPOSTDECODER_H

#include "OpenCsdTraceElement.h"
#include "DwtPacketDecoder.h"
#include "TraceEvent.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class CortexMPostDecoder final : public OpenCsdTraceElementSink {
public:
  explicit CortexMPostDecoder(TraceEventSink& eventSink);

  void append(OpenCsdTraceElement element) override;
  void finish();

  std::uint64_t eventCount() const;

private:
  void appendSync(const OpenCsdTraceElement& element);
  void appendOverflow(const OpenCsdTraceElement& element);
  void appendGlobalTimestamp(const OpenCsdTraceElement& element);
  void appendDiscontinuity(const OpenCsdTraceElement& element);
  void appendError(const OpenCsdTraceElement& element);
  void appendSoftware(const OpenCsdTraceElement& element);
  void appendDwt(const OpenCsdTraceElement& element);
  void appendTimestamp(const OpenCsdTraceElement& element);

  void queueDiscontinuityIssue(std::uint64_t sourceIndex, std::uint8_t traceBusId, const TraceQuality& quality,
                               const std::string& issueCode, const std::string& message,
                               std::optional<std::uint64_t> rawBytesConsumed = std::nullopt);
  void finalizePendingDiscontinuityIssues(std::optional<std::uint64_t> firstResumedTcyc);
  void queueOrEmitWhileAwaitingTimestamp(TraceEvent event);
  TraceQuality markDiscontinuity();
  void flushPendingDataTrace(const TraceQuality& quality);
  void flushPendingEvents(std::optional<std::uint64_t> tcyc, const TraceQuality& quality);
  void appendPendingEvents(std::vector<TraceEvent> events);
  void emitEvent(const TraceEvent& event);
  void mapTimestampSegment(OpenCsdTraceElement& element);
  void startNewTimestampSegment();
  static std::uint64_t saturatingAdd(std::uint64_t lhs, std::uint64_t rhs);

  TraceQuality currentTraceStatus(bool packetOverflow = false) const;
  TraceQuality statusResolvedByTimestamp(LocalTimestampRelation relation) const;
  void noteOverflow();

  TraceEventSink& eventSink_;
  std::uint64_t eventCount_ = 0;
  std::vector<TraceEvent> pendingEvents_;
  DwtPacketDecoder dwtDecoder_;
  bool timestampReliable_ = false;
  bool dataLossSinceLastTimestamp_ = false;
  std::uint64_t overflowCount_ = 0;
  std::uint64_t currentTcyc_ = 0;
  bool timelineKnown_ = false;
  std::uint64_t mappedTimeline_ = 0;
  std::uint64_t timestampSegmentBase_ = 0;
};

#endif  // CTRACE_SRC_DECODE_CORTEXMPOSTDECODER_H
