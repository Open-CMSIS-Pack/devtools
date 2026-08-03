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

class CtfStreamWriter final {
public:
  class Record final {
  public:
    void writeU8(std::uint8_t value);
    void writeU16(std::uint16_t value);
    void writeU32(std::uint32_t value);
    void writeU64(std::uint64_t value);

  private:
    friend class CtfStreamWriter;

    Record(std::vector<std::uint8_t>& buffer, std::size_t offset, std::size_t endOffset);

    void requireSpace(std::size_t size) const;

    std::vector<std::uint8_t>& buffer_;
    std::size_t offset_;
    std::size_t endOffset_;
  };

  using RecordCallback = std::function<void(Record&)>;

  CtfStreamWriter() = default;
  ~CtfStreamWriter();

  CtfStreamWriter(const CtfStreamWriter&) = delete;
  CtfStreamWriter& operator=(const CtfStreamWriter&) = delete;

  void open(const std::filesystem::path& filePath, std::uint32_t streamId);
  void close();
  void abort() noexcept;

  void writeRecord(std::uint32_t eventId, std::uint64_t timestamp, std::uint8_t traceBusId, std::size_t payloadSize,
                   const RecordCallback& writePayload);

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
