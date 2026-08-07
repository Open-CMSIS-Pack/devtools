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

    /** @brief Creates a bounded view over one reserved payload range. */
    Record(std::vector<std::uint8_t>& buffer, std::size_t offset, std::size_t endOffset);

    /** @brief Rejects writes that exceed the reserved payload range. */
    void requireSpace(std::size_t size) const;

    std::vector<std::uint8_t>& m_buffer;
    std::size_t m_offset;
    std::size_t m_endOffset;
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
  /** @brief Initializes a new packet buffer and writes its fixed context. */
  void beginPacket();
  /** @brief Finalizes and writes the current packet when it contains events. */
  void flushPacket();
  /** @brief Clamps a timestamp to the last emitted stream timestamp. */
  std::uint64_t monotonicTimestamp(std::uint64_t timestamp);

  std::filesystem::path m_filePath;
  std::ofstream m_file;
  std::vector<std::uint8_t> m_packetBuffer;
  std::size_t m_contentOffset = 0;
  std::uint32_t m_eventCount = 0;
  std::uint64_t m_timestampBegin = 0;
  std::uint64_t m_timestampEnd = 0;
  std::optional<std::uint64_t> m_lastTimestamp;
  std::uint32_t m_streamId = 0;
  std::uint32_t m_packetSequence = 0;
  std::array<std::uint8_t, 16U> m_uuid{};
  std::string m_uuidString;
  bool m_open = false;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_CTFSTREAMWRITER_H
