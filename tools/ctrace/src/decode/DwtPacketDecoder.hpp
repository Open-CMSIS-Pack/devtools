/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "TraceEvent.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

struct DwtPayloadPacket {
  std::uint64_t index = 0;
  std::uint8_t traceBusId = 0U;
  std::uint8_t discriminator = 0;
  std::uint8_t size = 0;
  std::uint32_t value = 0;
  std::uint64_t tcyc = 0;
  TraceQuality quality;
};

class DwtPacketDecoder {
public:
  std::vector<TraceEvent> decode(const DwtPayloadPacket& payload);
  std::vector<TraceEvent> flush(const TraceQuality& quality, std::uint64_t tcyc);
  void reset();

private:
  static constexpr std::size_t kDataTraceComparatorCount = 4U;

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

  void decodeDataTrace(const DwtPayloadPacket& payload, std::vector<TraceEvent>& output);
  void sendDataTraceEvent(std::uint32_t comparator, const PendingDataTrace& event, const TraceQuality& quality,
                          std::uint64_t tcyc, std::vector<TraceEvent>& output);
  void flushPending(std::uint32_t comparator, const TraceQuality& quality, std::uint64_t tcyc,
                    std::vector<TraceEvent>& output);
  TraceQuality qualityForPendingFlush(const PendingDataTrace& pending, const TraceQuality& current) const;

  static ExceptionAction exceptionAction(std::uint32_t value);
  std::array<std::optional<PendingDataTrace>, kDataTraceComparatorCount> pendingDataTrace_;
};
