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

/** @brief Converts OpenCSD elements from one Cortex-M stream into semantic events. */
class CortexMPostDecoder final : public OpenCsdTraceElementSink {
public:
  /** @brief Creates a post-decoder that emits to the supplied event sink. */
  explicit CortexMPostDecoder(TraceEventSink& eventSink);

  /** @brief Appends one OpenCSD trace element. */
  void append(OpenCsdTraceElement element) override;
  /** @brief Flushes pending state at the end of input. */
  void finish();

  /** @brief Returns the number of semantic events emitted. */
  std::uint64_t eventCount() const;

private:
  /** @brief Emits a synchronization event and starts a reliable trace segment. */
  void appendSync(const OpenCsdTraceElement& element);
  /** @brief Records a hardware overflow and invalidates timestamp quality. */
  void appendOverflow(const OpenCsdTraceElement& element);
  /** @brief Emits a reconstructed global timestamp event. */
  void appendGlobalTimestamp(const OpenCsdTraceElement& element);
  /** @brief Queues a recoverable stream discontinuity for ordered emission. */
  void appendDiscontinuity(const OpenCsdTraceElement& element);
  /** @brief Converts a decoder issue element into a semantic error event. */
  void appendError(const OpenCsdTraceElement& element);
  /** @brief Emits an ITM software event with current trace quality. */
  void appendSoftware(const OpenCsdTraceElement& element);
  /** @brief Passes a DWT payload through semantic packet reconstruction. */
  void appendDwt(const OpenCsdTraceElement& element);
  /** @brief Applies one local timestamp to pending and subsequent events. */
  void appendTimestamp(const OpenCsdTraceElement& element);

  /** @brief Queues an issue whose final interval ends at the next reliable timestamp. */
  void queueDiscontinuityIssue(std::uint64_t sourceIndex, std::uint8_t traceBusId, const TraceQuality& quality,
                               const std::string& issueCode, const std::string& message,
                               std::optional<std::uint64_t> rawBytesConsumed = std::nullopt);
  /** @brief Finalizes queued discontinuity intervals at the first resumed timestamp. */
  void finalizePendingDiscontinuityIssues(std::optional<std::uint64_t> firstResumedTcyc);
  /** @brief Queues an event until time is known or emits it immediately. */
  void queueOrEmitWhileAwaitingTimestamp(TraceEvent event);
  /** @brief Marks the current stream state as discontinuous and unreliable. */
  TraceQuality markDiscontinuity();
  /** @brief Flushes incomplete DWT reconstruction at a stream boundary. */
  void flushPendingDataTrace(const TraceQuality& quality);
  /** @brief Emits all events waiting for a resolved timestamp. */
  void flushPendingEvents(std::optional<std::uint64_t> tcyc, const TraceQuality& quality);
  /** @brief Appends reconstructed DWT events to the pending sequence. */
  void appendPendingEvents(std::vector<TraceEvent> events);
  /** @brief Sends one finalized event to the downstream sink. */
  void emitEvent(const TraceEvent& event);
  /** @brief Maps a decoder-local timestamp onto the monotonic output timeline. */
  void mapTimestampSegment(OpenCsdTraceElement& element);
  /** @brief Starts a new timestamp mapping segment after discontinuity. */
  void startNewTimestampSegment();

  /** @brief Returns quality derived from current overflow and timestamp state. */
  TraceQuality currentTraceStatus(bool packetOverflow = false) const;
  /** @brief Returns quality after applying a local timestamp relation. */
  TraceQuality statusResolvedByTimestamp(LocalTimestampRelation relation) const;
  /** @brief Increments the saturated overflow counter. */
  void noteOverflow();

  TraceEventSink& m_eventSink;
  std::uint64_t m_eventCount = 0;
  std::vector<TraceEvent> m_pendingEvents;
  DwtPacketDecoder m_dwtDecoder;
  bool m_timestampReliable = false;
  bool m_dataLossSinceLastTimestamp = false;
  std::uint64_t m_overflowCount = 0;
  std::uint64_t m_currentTcyc = 0;
  bool m_timelineKnown = false;
  std::uint64_t m_mappedTimeline = 0;
  std::uint64_t m_timestampSegmentBase = 0;
};

#endif  // CTRACE_SRC_DECODE_CORTEXMPOSTDECODER_H
