/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_CTFSTREAMWRITER_H
#define CTRACE_SRC_OUTPUT_CTF_CTFSTREAMWRITER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <vector>

/** @brief Writes packetized binary CTF stream records. */
class CtfStreamWriter final {
public:
  /** @brief Provides bounded little-endian writes into one reserved record payload. */
  class Record final {
  public:
    /** @brief Writes an unsigned 8-bit field. */
    void writeU8(std::uint8_t value);
    /** @brief Writes an unsigned 16-bit field. */
    void writeU16(std::uint16_t value);
    /** @brief Writes an unsigned 32-bit field. */
    void writeU32(std::uint32_t value);
    /** @brief Writes an unsigned 64-bit field. */
    void writeU64(std::uint64_t value);

  private:
    friend class CtfStreamWriter;

    Record(std::vector<std::uint8_t>& buffer, std::size_t offset, std::size_t endOffset);

    void requireSpace(std::size_t size) const;

    std::vector<std::uint8_t>& buffer_;
    std::size_t offset_;
    std::size_t endOffset_;
  };

  /** @brief Writes a caller-defined record payload. */
  using RecordCallback = std::function<void(Record&)>;

  /** @brief Creates an inactive CTF stream writer. */
  CtfStreamWriter() = default;
  /** @brief Aborts an open stream before destruction. */
  ~CtfStreamWriter();

  /** @brief Disables copying because the writer owns an output stream. */
  CtfStreamWriter(const CtfStreamWriter&) = delete;
  /** @brief Disables copy assignment because the writer owns an output stream. */
  CtfStreamWriter& operator=(const CtfStreamWriter&) = delete;

  /** @brief Opens a new CTF stream file with the supplied stream ID. */
  void open(const std::filesystem::path& filePath, std::uint32_t streamId);
  /** @brief Flushes the final packet and closes the stream. */
  void close();
  /** @brief Closes and removes an incomplete stream without throwing. */
  void abort() noexcept;

  /** @brief Appends one timestamped CTF event record. */
  void writeRecord(std::uint32_t eventId, std::uint64_t timestamp, std::uint8_t traceBusId, std::size_t payloadSize,
                   const RecordCallback& writePayload);

  /** @brief Returns the UUID shared by the stream and metadata. */
  const std::string& uuidString() const noexcept;

private:
  void beginPacket();
  void flushPacket();
  std::uint64_t monotonicTimestamp(std::uint64_t timestamp);

  std::filesystem::path filePath_;
  std::ofstream file_;
  std::vector<std::uint8_t> packetBuffer_;
  std::size_t contentOffset_ = 0;
  std::uint32_t eventCount_ = 0;
  std::uint64_t timestampBegin_ = 0;
  std::uint64_t timestampEnd_ = 0;
  std::optional<std::uint64_t> lastTimestamp_;
  std::uint32_t streamId_ = 0;
  std::uint32_t packetSequence_ = 0;
  std::array<std::uint8_t, 16U> uuid_{};
  std::string uuidString_;
  bool open_ = false;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_CTFSTREAMWRITER_H
