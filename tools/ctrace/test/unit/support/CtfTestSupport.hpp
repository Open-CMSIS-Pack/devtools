/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "TestSupport.hpp"
#include "ctf/CtfSchema.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <type_traits>
#include <vector>

namespace CtfTestSupport {

inline constexpr std::size_t kCtfPacketHeaderSize = 24U;
inline constexpr std::size_t kCtfPacketContextSize = 32U;
inline constexpr std::size_t kCtfEventHeaderSize = 13U;
inline constexpr std::size_t kCtfEventOffset = kCtfPacketHeaderSize + kCtfPacketContextSize;
inline constexpr std::size_t kCtfPacketSize = 65536U;

struct CtfRecord {
  std::uint32_t id;
  std::uint64_t timestamp;
  std::uint8_t traceBusId;
  std::vector<unsigned char> payload;
};

template <typename Integer> inline Integer readLittleEndian(const std::vector<unsigned char>& bytes, std::size_t offset)
{
  static_assert(std::is_integral_v<Integer> && std::is_unsigned_v<Integer>);
  require(offset <= bytes.size() && sizeof(Integer) <= bytes.size() - offset, "CTF test read exceeds stream data");
  Integer value = 0U;
  for (std::size_t byte = 0U; byte < sizeof(Integer); ++byte) {
    value |= static_cast<Integer>(bytes[offset + byte]) << (byte * 8U);
  }
  return value;
}

inline std::uint16_t readLe16(const std::vector<unsigned char>& bytes, std::size_t offset)
{
  return readLittleEndian<std::uint16_t>(bytes, offset);
}

inline std::uint32_t readLe32(const std::vector<unsigned char>& bytes, std::size_t offset)
{
  return readLittleEndian<std::uint32_t>(bytes, offset);
}

inline std::uint64_t readLe64(const std::vector<unsigned char>& bytes, std::size_t offset)
{
  return readLittleEndian<std::uint64_t>(bytes, offset);
}

inline std::size_t ctfValueSize(std::uint8_t tag)
{
  static constexpr std::array<std::size_t, 8U> sizes{{1U, 1U, 1U, 2U, 2U, 4U, 4U, 4U}};
  require(tag < sizes.size(), "CTF test parser encountered an invalid value tag");
  return sizes[tag];
}

inline std::size_t ctfPayloadSize(const std::vector<unsigned char>& bytes, std::size_t payloadOffset,
                                  std::size_t contentEnd, std::uint32_t eventId)
{
  const auto requirePayload = [&](std::size_t size) {
    require(payloadOffset <= contentEnd && size <= contentEnd - payloadOffset,
            "CTF test parser encountered a truncated event payload");
  };

  if (eventId == CtfSchema::value(CtfSchema::EventId::Itm)) {
    requirePayload(2U);
    return 7U + ctfValueSize(bytes[payloadOffset + 1U]);
  }
  if (eventId == CtfSchema::value(CtfSchema::EventId::DwtValue)) {
    requirePayload(3U);
    auto size = 3U + ctfValueSize(bytes[payloadOffset + 2U]);
    requirePayload(size + 1U);
    const auto hasPc = bytes[payloadOffset + size];
    require(hasPc <= 1U, "CTF test parser encountered an invalid DWT PC presence flag");
    size += 1U + (hasPc != 0U ? 4U : 0U);
    requirePayload(size + 1U);
    const auto hasAddress = bytes[payloadOffset + size];
    require(hasAddress <= 1U, "CTF test parser encountered an invalid DWT address presence flag");
    size += 1U + (hasAddress != 0U ? 2U : 0U);
    return size + 5U;
  }
  if (eventId == CtfSchema::value(CtfSchema::EventId::TraceStatus) ||
      eventId == CtfSchema::value(CtfSchema::EventId::Exception)) {
    return 5U;
  }
  if (eventId == CtfSchema::value(CtfSchema::EventId::DwtAddress)) {
    return 14U;
  }
  if (eventId == CtfSchema::value(CtfSchema::EventId::GlobalTimestamp)) {
    return 9U;
  }
  require(false, "CTF test parser encountered an unknown event ID");
  return 0U;
}

inline std::vector<CtfRecord> parseCtfRecords(const std::vector<unsigned char>& bytes)
{
  std::vector<CtfRecord> records;
  for (std::size_t packetStart = 0U; packetStart + kCtfEventOffset <= bytes.size(); packetStart += kCtfPacketSize) {
    const auto contentBits = readLe32(bytes, packetStart + kCtfPacketHeaderSize + 4U);
    require(contentBits % 8U == 0U, "CTF packet content size must be byte-aligned");
    const auto contentEnd = packetStart + static_cast<std::size_t>(contentBits / 8U);
    require(contentEnd >= packetStart + kCtfEventOffset && contentEnd <= bytes.size() &&
                contentEnd <= packetStart + kCtfPacketSize,
            "CTF packet content size exceeds the packet");

    auto offset = packetStart + kCtfEventOffset;
    while (offset < contentEnd) {
      require(kCtfEventHeaderSize <= contentEnd - offset, "CTF test parser encountered a truncated event header");
      const auto id = readLe32(bytes, offset);
      const auto timestamp = readLe64(bytes, offset + 4U);
      const auto traceBusId = bytes[offset + 12U];
      const auto payloadOffset = offset + kCtfEventHeaderSize;
      const auto payloadSize = ctfPayloadSize(bytes, payloadOffset, contentEnd, id);
      require(payloadSize <= contentEnd - payloadOffset, "CTF event payload exceeds packet content");
      records.push_back({
          id,
          timestamp,
          traceBusId,
          {bytes.begin() + static_cast<std::ptrdiff_t>(payloadOffset),
           bytes.begin() + static_cast<std::ptrdiff_t>(payloadOffset + payloadSize)},
      });
      offset = payloadOffset + payloadSize;
    }
  }
  return records;
}

inline std::vector<CtfRecord> readCtfRecords(const std::filesystem::path& streamPath)
{
  return parseCtfRecords(readTestBinaryFile(streamPath));
}

inline const CtfRecord& requireFirstCtfRecord(const std::vector<CtfRecord>& records, CtfSchema::EventId id,
                                              const std::string& message)
{
  const auto expected = CtfSchema::value(id);
  const auto found = std::find_if(records.begin(), records.end(),
                                  [expected](const CtfRecord& record) { return record.id == expected; });
  require(found != records.end(), message);
  return *found;
}

inline void requireSingleItmEvent(const std::filesystem::path& streamPath, std::uint8_t expectedChannel,
                                  const std::string& message)
{
  const auto records = readCtfRecords(streamPath);
  require(records.size() == 1U && records.front().id == CtfSchema::value(CtfSchema::EventId::Itm) &&
              !records.front().payload.empty() && records.front().payload.front() == expectedChannel,
          message);
}

} // namespace CtfTestSupport
