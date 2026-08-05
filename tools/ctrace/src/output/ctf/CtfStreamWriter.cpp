/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfStreamWriter.h"

#include "CtfSchema.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <ios>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

constexpr std::size_t kPacketSizeBytes = 65536U;
constexpr std::size_t kPacketHeaderSize = 24U;
constexpr std::size_t kPacketContextSize = 32U;
constexpr std::size_t kPacketOverhead = kPacketHeaderSize + kPacketContextSize;
constexpr std::size_t kEventPrefixSize = 13U;
/** @brief Formats a binary UUID in canonical textual form. */
static std::string formatUuid(const std::array<std::uint8_t, 16U>& uuid)
{
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (std::size_t index = 0; index < uuid.size(); ++index) {
    if (index == 4U || index == 6U || index == 8U || index == 10U) {
      out << '-';
    }
    out << std::setw(2) << static_cast<unsigned>(uuid[index]);
  }
  return out.str();
}

CtfStreamWriter::Record::Record(std::vector<std::uint8_t>& buffer, std::size_t offset, std::size_t endOffset)
  : m_buffer(buffer),
    m_offset(offset),
    m_endOffset(endOffset)
{
}

void CtfStreamWriter::Record::requireSpace(std::size_t size) const
{
  if (size > m_endOffset - m_offset) {
    throw std::logic_error("CTF record payload exceeds its declared size");
  }
}

void CtfStreamWriter::Record::writeU8(std::uint8_t value)
{
  requireSpace(1U);
  m_buffer[m_offset++] = value;
}

void CtfStreamWriter::Record::writeU16(std::uint16_t value)
{
  requireSpace(2U);
  writeU8(static_cast<std::uint8_t>(value & 0xffU));
  writeU8(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
}

void CtfStreamWriter::Record::writeU32(std::uint32_t value)
{
  requireSpace(4U);
  for (unsigned shift = 0U; shift < 32U; shift += 8U) {
    writeU8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }
}

void CtfStreamWriter::Record::writeU64(std::uint64_t value)
{
  requireSpace(8U);
  for (unsigned shift = 0U; shift < 64U; shift += 8U) {
    writeU8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
  }
}

CtfStreamWriter::~CtfStreamWriter()
{
  abort();
}

void CtfStreamWriter::open(const std::filesystem::path& filePath, std::uint32_t streamId)
{
  abort();
  m_filePath = filePath;
  m_streamId = streamId;
  m_packetSequence = 0U;
  m_lastTimestamp.reset();
  m_uuid.fill(0U);

  std::random_device random;
  for (auto& byte : m_uuid) {
    byte = static_cast<std::uint8_t>(random());
  }
  m_uuid[6] = static_cast<std::uint8_t>((m_uuid[6] & 0x0fU) | 0x40U);
  m_uuid[8] = static_cast<std::uint8_t>((m_uuid[8] & 0x3fU) | 0x80U);
  m_uuidString = formatUuid(m_uuid);

  m_packetBuffer.assign(kPacketSizeBytes, 0U);
  beginPacket();
  m_file.open(m_filePath, std::ios::binary | std::ios::out | std::ios::trunc);
  if (!m_file) {
    abort();
    throw std::runtime_error("Failed to open CTF stream " + filePath.string());
  }
  m_open = true;
}

void CtfStreamWriter::close()
{
  if (!m_open) {
    return;
  }
  flushPacket();
  m_file.close();
  m_open = false;
  m_packetBuffer.clear();
  if (!m_file) {
    throw std::runtime_error("Failed to write CTF stream in " + m_filePath.parent_path().string());
  }
}

void CtfStreamWriter::abort() noexcept
{
  if (m_file.is_open()) {
    m_file.close();
  }
  m_open = false;
  m_packetBuffer.clear();
  m_filePath.clear();
}

void CtfStreamWriter::writeRecord(std::uint32_t eventId, std::uint64_t timestamp, std::uint8_t traceBusId,
                                  std::size_t payloadSize, const RecordCallback& writePayload)
{
  if (!m_open) {
    return;
  }
  const auto totalSize = kEventPrefixSize + payloadSize;
  if (totalSize > kPacketSizeBytes - kPacketOverhead) {
    throw std::invalid_argument("CTF record does not fit into a packet");
  }
  if (m_contentOffset + totalSize > kPacketSizeBytes) {
    flushPacket();
  }

  timestamp = monotonicTimestamp(timestamp);
  const auto recordEnd = m_contentOffset + totalSize;
  Record record(m_packetBuffer, m_contentOffset, recordEnd);
  record.writeU32(eventId);
  record.writeU64(timestamp);
  record.writeU8(traceBusId);
  writePayload(record);
  if (record.m_offset != recordEnd) {
    throw std::logic_error("CTF record payload is shorter than its declared size");
  }

  m_contentOffset = recordEnd;
  if (m_eventCount == 0U) {
    m_timestampBegin = timestamp;
    m_timestampEnd = timestamp;
  } else {
    m_timestampBegin = std::min(m_timestampBegin, timestamp);
    m_timestampEnd = std::max(m_timestampEnd, timestamp);
  }
  ++m_eventCount;
}

const std::string& CtfStreamWriter::uuidString() const noexcept
{
  return m_uuidString;
}

void CtfStreamWriter::beginPacket()
{
  std::fill(m_packetBuffer.begin(), m_packetBuffer.end(), std::uint8_t{0});
  m_contentOffset = kPacketOverhead;
  m_eventCount = 0U;
  m_timestampBegin = 0U;
  m_timestampEnd = 0U;
}

void CtfStreamWriter::flushPacket()
{
  if (m_eventCount == 0U) {
    return;
  }

  Record header(m_packetBuffer, 0U, kPacketOverhead);
  header.writeU32(CtfSchema::Magic);
  for (const auto byte : m_uuid) {
    header.writeU8(byte);
  }
  header.writeU32(m_streamId);
  header.writeU32(static_cast<std::uint32_t>(kPacketSizeBytes * 8U));
  header.writeU32(static_cast<std::uint32_t>(m_contentOffset * 8U));
  header.writeU64(m_timestampBegin);
  header.writeU64(m_timestampEnd);
  header.writeU32(0U);
  header.writeU32(m_packetSequence);

  m_file.write(reinterpret_cast<const char*>(m_packetBuffer.data()), static_cast<std::streamsize>(m_packetBuffer.size()));
  ++m_packetSequence;
  beginPacket();
}

std::uint64_t CtfStreamWriter::monotonicTimestamp(std::uint64_t timestamp)
{
  if (m_lastTimestamp.has_value() && timestamp < *m_lastTimestamp) {
    timestamp = *m_lastTimestamp;
  }
  m_lastTimestamp = timestamp;
  return timestamp;
}
