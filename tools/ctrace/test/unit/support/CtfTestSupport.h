/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_TEST_UNIT_SUPPORT_CTFTESTSUPPORT_H
#define CTRACE_TEST_UNIT_SUPPORT_CTFTESTSUPPORT_H

#include "TestSupport.h"
#include "TraceEvent.h"
#include "ctf/CtfSchema.h"

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

/** @brief Stores one CTF event parsed by a unit test. */
struct CtfRecord {
  std::uint32_t id;
  std::uint64_t timestamp;
  std::uint8_t traceBusId;
  std::vector<unsigned char> payload;
};

/** @brief Stores the decoded fields of one CTF exception event. */
struct CtfExceptionRecord {
  ExceptionNumber number;
  std::uint8_t action;
  std::uint8_t origin;
};

/** @brief Compares decoded CTF exception records. */
inline bool operator==(const CtfExceptionRecord& lhs, const CtfExceptionRecord& rhs)
{
  return lhs.number == rhs.number && lhs.action == rhs.action && lhs.origin == rhs.origin;
}

/** @brief Stores one decoded CTF exception event with its event timestamp. */
struct TimestampedCtfExceptionRecord {
  std::uint64_t timestamp;
  CtfExceptionRecord exception;
};

/** @brief Compares timestamped CTF exception records. */
inline bool operator==(const TimestampedCtfExceptionRecord& lhs, const TimestampedCtfExceptionRecord& rhs)
{
  return lhs.timestamp == rhs.timestamp && lhs.exception == rhs.exception;
}

/** @brief Reads an unsigned little-endian integer from test stream bytes. */
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

/** @brief Reads a 16-bit little-endian test value. */
inline std::uint16_t readLe16(const std::vector<unsigned char>& bytes, std::size_t offset)
{
  return readLittleEndian<std::uint16_t>(bytes, offset);
}

/** @brief Reads a 32-bit little-endian test value. */
inline std::uint32_t readLe32(const std::vector<unsigned char>& bytes, std::size_t offset)
{
  return readLittleEndian<std::uint32_t>(bytes, offset);
}

/** @brief Reads a 64-bit little-endian test value. */
inline std::uint64_t readLe64(const std::vector<unsigned char>& bytes, std::size_t offset)
{
  return readLittleEndian<std::uint64_t>(bytes, offset);
}

/** @brief Returns the encoded size of a test CTF value variant. */
inline std::size_t ctfValueSize(std::uint8_t tag)
{
  static constexpr std::array<std::size_t, 7U> sizes{{1U, 1U, 2U, 2U, 4U, 4U, 4U}};
  require(tag < sizes.size(), "CTF test parser encountered an invalid value tag");
  return sizes[tag];
}

/** @brief Returns the encoded payload size of a width-tagged DWT offset. */
inline std::size_t ctfDwtOffsetSize(std::uint8_t tag)
{
  if (tag == CtfSchema::value(CtfSchema::DwtOffsetTag::None) ||
      tag == CtfSchema::value(CtfSchema::DwtOffsetTag::U8)) {
    return 1U;
  }
  if (tag == CtfSchema::value(CtfSchema::DwtOffsetTag::U16)) {
    return 2U;
  }
  if (tag == CtfSchema::value(CtfSchema::DwtOffsetTag::U32)) {
    return 4U;
  }
  require(false, "CTF test parser encountered an invalid DWT offset tag");
  return 0U;
}

/** @brief Determines one encoded CTF event payload size. */
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
    size += 1U + ctfDwtOffsetSize(bytes[payloadOffset + size]);
    return size + 5U;
  }
  if (eventId == CtfSchema::value(CtfSchema::EventId::TraceStatus)) {
    return 5U;
  }
  if (eventId == CtfSchema::value(CtfSchema::EventId::Exception)) {
    return 6U;
  }
  if (eventId == CtfSchema::value(CtfSchema::EventId::DwtAddress)) {
    requirePayload(2U);
    const auto hasPc = bytes[payloadOffset + 1U];
    require(hasPc <= 1U, "CTF test parser encountered an invalid DWT PC presence flag");
    const auto offsetTagPosition = 2U + (hasPc != 0U ? 4U : 0U);
    requirePayload(offsetTagPosition + 1U);
    return offsetTagPosition + 1U + ctfDwtOffsetSize(bytes[payloadOffset + offsetTagPosition]) + 5U;
  }
  if (eventId == CtfSchema::value(CtfSchema::EventId::GlobalTimestamp)) {
    return 9U;
  }
  if (eventId == CtfSchema::value(CtfSchema::EventId::PcSample)) {
    requirePayload(1U);
    const auto state = bytes[payloadOffset];
    require(state <= CtfSchema::value(CtfSchema::PcSampleState::Pc),
            "CTF test parser encountered an invalid PC-sample state");
    return 1U + (state == CtfSchema::value(CtfSchema::PcSampleState::Pc) ? 4U : 0U) + 5U;
  }
  if (eventId == CtfSchema::value(CtfSchema::EventId::DwtEvent)) {
    return 6U;
  }
  if (eventId == CtfSchema::value(CtfSchema::EventId::PmuEvent)) {
    return 6U;
  }
  if (eventId == CtfSchema::value(CtfSchema::EventId::DwtMatch)) {
    return 6U;
  }
  require(false, "CTF test parser encountered an unknown event ID");
  return 0U;
}

/** @brief Parses all records from an encoded test CTF stream. */
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

/** @brief Reads and parses records from a test CTF stream file. */
inline std::vector<CtfRecord> readCtfRecords(const std::filesystem::path& streamPath)
{
  return parseCtfRecords(readTestBinaryFile(streamPath));
}

/** @brief Decodes and validates one CTF exception record payload. */
inline CtfExceptionRecord decodeCtfExceptionRecord(const CtfRecord& record)
{
  require(record.id == CtfSchema::value(CtfSchema::EventId::Exception), "expected a CTF exception record");
  require(record.payload.size() == 6U, "CTF exception record has an invalid payload size");
  const auto number = readLe16(record.payload, 0U);
  require(number == readLe16(record.payload, 3U), "CTF exception record contains inconsistent exception numbers");
  return {number, record.payload[2U], record.payload[5U]};
}

/** @brief Decodes all exception events in parsed CTF records. */
inline std::vector<CtfExceptionRecord> ctfExceptionRecords(const std::vector<CtfRecord>& records)
{
  std::vector<CtfExceptionRecord> exceptions;
  for (const auto& record : records) {
    if (record.id == CtfSchema::value(CtfSchema::EventId::Exception)) {
      exceptions.push_back(decodeCtfExceptionRecord(record));
    }
  }
  return exceptions;
}

/** @brief Decodes all exception events and timestamps in parsed CTF records. */
inline std::vector<TimestampedCtfExceptionRecord>
timestampedCtfExceptionRecords(const std::vector<CtfRecord>& records)
{
  std::vector<TimestampedCtfExceptionRecord> exceptions;
  for (const auto& record : records) {
    if (record.id == CtfSchema::value(CtfSchema::EventId::Exception)) {
      exceptions.push_back({record.timestamp, decodeCtfExceptionRecord(record)});
    }
  }
  return exceptions;
}

/** @brief Reads and decodes all exception events from a test CTF stream file. */
inline std::vector<CtfExceptionRecord> readCtfExceptionRecords(const std::filesystem::path& streamPath)
{
  return ctfExceptionRecords(readCtfRecords(streamPath));
}

/** @brief Returns the first record with a required CTF event ID. */
inline const CtfRecord& requireFirstCtfRecord(const std::vector<CtfRecord>& records, CtfSchema::EventId id,
                                              const std::string& message)
{
  const auto expected = CtfSchema::value(id);
  const auto found = std::find_if(records.begin(), records.end(),
                                  [expected](const CtfRecord& record) { return record.id == expected; });
  require(found != records.end(), message);
  return *found;
}

/** @brief Requires a stream containing one ITM event on the expected channel. */
inline void requireSingleItmEvent(const std::filesystem::path& streamPath, std::uint8_t expectedChannel,
                                  const std::string& message)
{
  const auto records = readCtfRecords(streamPath);
  require(records.size() == 1U && records.front().id == CtfSchema::value(CtfSchema::EventId::Itm) &&
              !records.front().payload.empty() && records.front().payload.front() == expectedChannel,
          message);
}

} // namespace CtfTestSupport

#endif  // CTRACE_TEST_UNIT_SUPPORT_CTFTESTSUPPORT_H
