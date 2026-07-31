/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "TestSupport.hpp"

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

inline void requireSingleItmEvent(const std::filesystem::path& streamPath, std::uint8_t expectedChannel,
                                  const std::string& message)
{
  const auto bytes = readTestBinaryFile(streamPath);
  constexpr std::size_t itmPayloadSize = 8U;
  constexpr auto contentSize = kCtfEventOffset + kCtfEventHeaderSize + itmPayloadSize;
  require(bytes.size() >= contentSize, message);
  require(readLe32(bytes, kCtfPacketHeaderSize + 4U) == contentSize * 8U, message);
  require(readLe32(bytes, kCtfEventOffset) == 0U, message);
  require(bytes[kCtfEventOffset + kCtfEventHeaderSize] == expectedChannel, message);
}

} // namespace CtfTestSupport
