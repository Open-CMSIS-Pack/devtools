/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_TEST_UNIT_SUPPORT_FORMATTEDTRACETESTSUPPORT_H
#define CTRACE_TEST_UNIT_SUPPORT_FORMATTEDTRACETESTSUPPORT_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace FormattedTraceTestSupport {

/** @brief Groups contiguous trace bytes under one CoreSight formatter ID. */
struct Segment {
  std::uint8_t traceId = 0U;
  std::vector<std::uint8_t> bytes;
};

/** @brief Returns one architectural ITM hardware synchronization packet. */
inline std::vector<std::uint8_t> itmHardwareSync()
{
  return {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U};
}

/** @brief Returns one ITM source packet with a little-endian payload. */
inline std::vector<std::uint8_t> itmSourcePacket(std::uint8_t source, bool hardware, std::uint8_t width,
                                                 std::uint32_t value)
{
  if (source > 31U) {
    throw std::invalid_argument("ITM source must be between 0 and 31");
  }
  const auto sizeCode = width == 1U ? 1U : width == 2U ? 2U : width == 4U ? 3U : 0U;
  if (sizeCode == 0U) {
    throw std::invalid_argument("ITM source packet width must be 1, 2, or 4 bytes");
  }

  std::vector<std::uint8_t> packet{static_cast<std::uint8_t>((source << 3U) | (hardware ? 0x04U : 0x00U) | sizeCode)};
  for (std::uint8_t byte = 0U; byte < width; ++byte) {
    packet.push_back(static_cast<std::uint8_t>(value >> (byte * 8U)));
  }
  return packet;
}

/** @brief Returns one one-byte ITM software packet. */
inline std::vector<std::uint8_t> itmSoftwarePacket(std::uint8_t channel, std::uint8_t value)
{
  return itmSourcePacket(channel, false, 1U, value);
}

/** @brief Returns one width-preserving ITM software packet. */
inline std::vector<std::uint8_t> itmSoftwarePacket(std::uint8_t channel, std::uint8_t width, std::uint32_t value)
{
  return itmSourcePacket(channel, false, width, value);
}

/** @brief Returns one width-preserving DWT hardware-source packet. */
inline std::vector<std::uint8_t> itmHardwarePacket(std::uint8_t discriminator, std::uint8_t width, std::uint32_t value)
{
  return itmSourcePacket(discriminator, true, width, value);
}

/** @brief Returns one synchronous format-1 ITM local-timestamp packet. */
inline std::vector<std::uint8_t> itmLocalTimestampPacket(std::uint32_t increment)
{
  if (increment > 0x0fffffffU) {
    throw std::invalid_argument("ITM local timestamp must fit in 28 bits");
  }

  std::vector<std::uint8_t> packet{0xc0U};
  do {
    auto byte = static_cast<std::uint8_t>(increment & 0x7fU);
    increment >>= 7U;
    if (increment != 0U) {
      byte |= 0x80U;
    }
    packet.push_back(byte);
  } while (increment != 0U);
  return packet;
}

/** @brief Returns one paired GTS1/GTS2 packet carrying a complete global timestamp. */
inline std::vector<std::uint8_t> itmGlobalTimestampPacket(std::uint64_t value)
{
  std::vector<std::uint8_t> packet{
      0x94U,
      static_cast<std::uint8_t>(0x80U | ((value >> 0U) & 0x7fU)),
      static_cast<std::uint8_t>(0x80U | ((value >> 7U) & 0x7fU)),
      static_cast<std::uint8_t>(0x80U | ((value >> 14U) & 0x7fU)),
      static_cast<std::uint8_t>((value >> 21U) & 0x1fU),
      0xb4U,
      static_cast<std::uint8_t>(0x80U | ((value >> 26U) & 0x7fU)),
      static_cast<std::uint8_t>(0x80U | ((value >> 33U) & 0x7fU)),
      static_cast<std::uint8_t>(0x80U | ((value >> 40U) & 0x7fU)),
      static_cast<std::uint8_t>(0x80U | ((value >> 47U) & 0x7fU)),
      static_cast<std::uint8_t>(0x80U | ((value >> 54U) & 0x7fU)),
      static_cast<std::uint8_t>((value >> 61U) & 0x07U),
  };
  return packet;
}

/** @brief Returns one ITM overflow packet. */
inline std::vector<std::uint8_t> itmOverflowPacket()
{
  return {0x70U};
}

namespace Detail {

/** @brief Associates one payload byte with its formatter source ID. */
struct RoutedByte {
  std::uint8_t traceId = 0U;
  std::uint8_t value = 0U;
};

/** @brief Counts contiguous formatter-ID runs in routed test data. */
inline std::size_t runCount(const std::vector<RoutedByte>& records)
{
  if (records.empty()) {
    return 0U;
  }
  std::size_t count = 1U;
  for (std::size_t index = 1U; index < records.size(); ++index) {
    if (records[index - 1U].traceId != records[index].traceId) {
      ++count;
    }
  }
  return count;
}

/** @brief Encodes an ID marker for one memory-aligned formatter frame. */
inline std::uint8_t sourceMarker(std::uint8_t traceId)
{
  if (traceId > 0x7fU) {
    throw std::invalid_argument("CoreSight formatter ID must be between 0 and 127");
  }
  return static_cast<std::uint8_t>((traceId << 1U) | 0x01U);
}

} // namespace Detail

/**
 * @brief Packs routed payload into 16-byte memory-aligned CoreSight frames.
 *
 * Empty input represents an empty capture. Non-empty input is padded with
 * formatter ID 0 data so that every returned frame is complete.
 */
inline std::vector<std::uint8_t> memoryAlignedFrames(const std::vector<Segment>& segments)
{
  std::vector<Detail::RoutedByte> records;
  for (const auto& segment : segments) {
    if (segment.traceId > 0x7fU) {
      throw std::invalid_argument("CoreSight formatter ID must be between 0 and 127");
    }
    for (const auto value : segment.bytes) {
      records.push_back({segment.traceId, value});
    }
  }
  if (records.empty()) {
    return {};
  }

  // Each frame contains 15 payload-or-ID slots plus its flag byte. Add a
  // visible NULL-source run and extend it until all slots are occupied.
  if (records.back().traceId != 0U) {
    records.push_back({0U, 0U});
  }
  while ((records.size() + Detail::runCount(records)) % 15U != 0U) {
    records.push_back({0U, 0U});
  }

  std::vector<std::uint8_t> output;
  output.reserve(((records.size() + Detail::runCount(records)) / 15U) * 16U);
  std::optional<std::uint8_t> currentId;
  std::size_t position = 0U;
  while (position < records.size()) {
    std::array<std::uint8_t, 16U> frame{};
    std::uint8_t flags = 0U;

    for (std::size_t byte = 0U; byte < 14U; byte += 2U) {
      if (position >= records.size()) {
        throw std::logic_error("formatted test payload ended before a complete frame pair");
      }
      const auto& record = records[position];
      if (!currentId.has_value() || record.traceId != *currentId) {
        frame[byte] = Detail::sourceMarker(record.traceId);
        currentId = record.traceId;
        frame[byte + 1U] = record.value;
        ++position;
        continue;
      }

      if (position + 1U < records.size() && records[position + 1U].traceId != *currentId) {
        const auto nextId = records[position + 1U].traceId;
        frame[byte] = Detail::sourceMarker(nextId);
        frame[byte + 1U] = record.value;
        flags |= static_cast<std::uint8_t>(1U << (byte / 2U));
        currentId = nextId;
        ++position;
        continue;
      }

      frame[byte] = static_cast<std::uint8_t>(record.value & 0xfeU);
      flags |= static_cast<std::uint8_t>((record.value & 0x01U) << (byte / 2U));
      ++position;
      if (position >= records.size() || records[position].traceId != *currentId) {
        throw std::logic_error("formatted test payload cannot fill a complete frame pair");
      }
      frame[byte + 1U] = records[position].value;
      ++position;
    }

    if (position >= records.size()) {
      throw std::logic_error("formatted test payload ended before the final frame slot");
    }
    const auto& last = records[position];
    if (!currentId.has_value() || last.traceId != *currentId) {
      frame[14U] = Detail::sourceMarker(last.traceId);
      currentId = last.traceId;
    } else {
      frame[14U] = static_cast<std::uint8_t>(last.value & 0xfeU);
      flags |= static_cast<std::uint8_t>((last.value & 0x01U) << 7U);
      ++position;
    }
    frame[15U] = flags;
    output.insert(output.end(), frame.begin(), frame.end());
  }
  return output;
}

} // namespace FormattedTraceTestSupport

#endif // CTRACE_TEST_UNIT_SUPPORT_FORMATTEDTRACETESTSUPPORT_H
