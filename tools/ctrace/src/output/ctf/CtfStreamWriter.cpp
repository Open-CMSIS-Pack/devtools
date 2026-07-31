/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfStreamWriter.hpp"

#include "CtfSchema.hpp"

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

namespace {

constexpr std::size_t kPacketSizeBytes = 65536U;
constexpr std::size_t kPacketHeaderSize = 24U;
constexpr std::size_t kPacketContextSize = 32U;
constexpr std::size_t kPacketOverhead = kPacketHeaderSize + kPacketContextSize;
constexpr std::size_t kEventPrefixSize = 13U;
std::string formatUuid(const std::array<std::uint8_t, 16U>& uuid)
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

} // namespace

CtfStreamWriter::Record::Record(std::vector<std::uint8_t>& buffer, std::size_t offset, std::size_t endOffset)
  : buffer_(buffer), offset_(offset), endOffset_(endOffset)
{
}

void CtfStreamWriter::Record::requireSpace(std::size_t size) const
{
  if (size > endOffset_ - offset_) {
    throw std::logic_error("CTF record payload exceeds its declared size");
  }
}

void CtfStreamWriter::Record::writeU8(std::uint8_t value)
{
  requireSpace(1U);
  buffer_[offset_++] = value;
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
  filePath_ = filePath;
  streamId_ = streamId;
  packetSequence_ = 0U;
  lastTimestamp_.reset();
  uuid_.fill(0U);

  std::random_device random;
  for (auto& byte : uuid_) {
    byte = static_cast<std::uint8_t>(random());
  }
  uuid_[6] = static_cast<std::uint8_t>((uuid_[6] & 0x0fU) | 0x40U);
  uuid_[8] = static_cast<std::uint8_t>((uuid_[8] & 0x3fU) | 0x80U);
  uuidString_ = formatUuid(uuid_);

  packetBuffer_.assign(kPacketSizeBytes, 0U);
  beginPacket();
  file_.open(filePath_, std::ios::binary | std::ios::out | std::ios::trunc);
  if (!file_) {
    abort();
    throw std::runtime_error("Failed to open CTF stream " + filePath.string());
  }
  open_ = true;
}

void CtfStreamWriter::close()
{
  if (!open_) {
    return;
  }
  flushPacket();
  file_.close();
  open_ = false;
  packetBuffer_.clear();
  if (!file_) { // LCOV_EXCL_BR_LINE: covered on Linux with /dev/full
    throw std::runtime_error("Failed to write CTF stream in " + filePath_.parent_path().string()); // LCOV_EXCL_LINE
  }
}

void CtfStreamWriter::abort() noexcept
{
  if (file_.is_open()) {
    file_.close();
  }
  open_ = false;
  packetBuffer_.clear();
  filePath_.clear();
}

void CtfStreamWriter::writeRecord(std::uint32_t eventId, std::uint64_t timestamp, std::uint8_t traceBusId,
                                  std::size_t payloadSize, const RecordCallback& writePayload)
{
  if (!open_) {
    return;
  }
  const auto totalSize = kEventPrefixSize + payloadSize;
  if (totalSize > kPacketSizeBytes - kPacketOverhead) {
    throw std::invalid_argument("CTF record does not fit into a packet");
  }
  if (contentOffset_ + totalSize > kPacketSizeBytes) {
    flushPacket();
  }

  timestamp = monotonicTimestamp(timestamp);
  const auto recordEnd = contentOffset_ + totalSize;
  Record record(packetBuffer_, contentOffset_, recordEnd);
  record.writeU32(eventId);
  record.writeU64(timestamp);
  record.writeU8(traceBusId);
  writePayload(record);
  if (record.offset_ != recordEnd) {
    throw std::logic_error("CTF record payload is shorter than its declared size");
  }

  contentOffset_ = recordEnd;
  if (eventCount_ == 0U) {
    timestampBegin_ = timestamp;
    timestampEnd_ = timestamp;
  } else {
    timestampBegin_ = std::min(timestampBegin_, timestamp);
    timestampEnd_ = std::max(timestampEnd_, timestamp);
  }
  ++eventCount_;
}

const std::string& CtfStreamWriter::uuidString() const noexcept
{
  return uuidString_;
}

void CtfStreamWriter::beginPacket()
{
  std::fill(packetBuffer_.begin(), packetBuffer_.end(), std::uint8_t{0});
  contentOffset_ = kPacketOverhead;
  eventCount_ = 0U;
  timestampBegin_ = 0U;
  timestampEnd_ = 0U;
}

void CtfStreamWriter::flushPacket()
{
  if (eventCount_ == 0U) {
    return;
  }

  Record header(packetBuffer_, 0U, kPacketOverhead);
  header.writeU32(CtfSchema::Magic);
  for (const auto byte : uuid_) {
    header.writeU8(byte);
  }
  header.writeU32(streamId_);
  header.writeU32(static_cast<std::uint32_t>(kPacketSizeBytes * 8U));
  header.writeU32(static_cast<std::uint32_t>(contentOffset_ * 8U));
  header.writeU64(timestampBegin_);
  header.writeU64(timestampEnd_);
  header.writeU32(0U);
  header.writeU32(packetSequence_);

  file_.write(reinterpret_cast<const char*>(packetBuffer_.data()), static_cast<std::streamsize>(packetBuffer_.size()));
  ++packetSequence_;
  beginPacket();
}

std::uint64_t CtfStreamWriter::monotonicTimestamp(std::uint64_t timestamp)
{
  if (lastTimestamp_.has_value() && timestamp < *lastTimestamp_) {
    timestamp = *lastTimestamp_;
  }
  lastTimestamp_ = timestamp;
  return timestamp;
}
