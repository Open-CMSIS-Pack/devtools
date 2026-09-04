/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_DWTPACKETDECODER_H
#define CTRACE_SRC_DECODE_DWTPACKETDECODER_H

#include "TraceEvent.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

/** @brief Stores a decoded DWT hardware payload and its trace metadata. */
struct DwtPayloadPacket {
  std::uint64_t index = 0;
  std::uint8_t traceBusId = 0U;
  std::uint8_t discriminator = 0;
  std::uint8_t size = 0;
  std::uint32_t value = 0;
  std::uint64_t tcyc = 0;
  TraceQuality quality;
};

/** @brief Number of comparators encodable in a DWT data-trace source ID. */
inline constexpr std::size_t kDwtDataTraceComparatorCount = 4U;

/** @brief Stores configured DWT comparator values used for packet decompression. */
using DwtComparatorValues = std::array<std::optional<std::uint32_t>, kDwtDataTraceComparatorCount>;

/** @brief Reconstructs semantic DWT events from hardware payload packets. */
class DwtPacketDecoder {
public:
  /** @brief Creates a decoder with the comparator values active for this stream. */
  explicit DwtPacketDecoder(DwtComparatorValues comparatorValues = {});
  /** @brief Decodes one hardware payload and returns completed semantic events. */
  std::vector<TraceEvent> decode(const DwtPayloadPacket& payload);
  /** @brief Flushes incomplete data-trace fragments at a boundary. */
  std::vector<TraceEvent> flush(const TraceQuality& quality, std::uint64_t tcyc);
  /** @brief Discards all pending data-trace reconstruction state. */
  void reset();

private:
  /** @brief Accumulates the fragments of one pending DWT data-trace event. */
  struct PendingDataTrace {
    std::uint64_t index = 0;
    std::uint8_t traceBusId = 0U;
    std::uint32_t pc = 0;
    std::uint32_t addressLo16 = 0;
    std::uint32_t value = 0;
    std::uint8_t size = 4;
    bool isRead = false;
    bool hasPc = false;
    bool hasAddressLo16 = false;
    bool hasValue = false;
    TraceQuality quality;
  };

  /** @brief Accumulates one DWT data-trace packet and emits completed events. */
  void decodeDataTrace(const DwtPayloadPacket& payload, std::vector<TraceEvent>& output);
  /** @brief Converts one complete pending comparator state into an event. */
  void sendDataTraceEvent(std::uint32_t comparator, const PendingDataTrace& event, const TraceQuality& quality,
                          std::uint64_t tcyc, std::vector<TraceEvent>& output);
  /** @brief Flushes one comparator state and clears its pending fragments. */
  void flushPending(std::uint32_t comparator, const TraceQuality& quality, std::uint64_t tcyc,
                    std::vector<TraceEvent>& output);
  /** @brief Combines quality captured by a fragment with the current boundary. */
  TraceQuality qualityForPendingFlush(const PendingDataTrace& pending, const TraceQuality& current) const;

  /** @brief Maps an encoded DWT exception action to the semantic action. */
  static ExceptionAction exceptionAction(std::uint32_t value);
  DwtComparatorValues m_comparatorValues;
  std::array<std::optional<PendingDataTrace>, kDwtDataTraceComparatorCount> m_pendingDataTrace;
};

#endif // CTRACE_SRC_DECODE_DWTPACKETDECODER_H
