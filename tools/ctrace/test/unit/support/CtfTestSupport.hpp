/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace CtfTestSupport {

inline constexpr std::size_t kCtfPacketHeaderSize = 24U;
inline constexpr std::size_t kCtfPacketContextSize = 32U;
inline constexpr std::size_t kCtfEventHeaderSize = 13U;
inline constexpr std::size_t kCtfEventOffset = kCtfPacketHeaderSize + kCtfPacketContextSize;
inline constexpr std::size_t kCtfPacketSize = 65536U;

inline std::uint16_t readLe16(const std::vector<unsigned char>& bytes, std::size_t offset)
{
  return static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset]) |
                                    (static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U));
}

inline std::uint32_t readLe32(const std::vector<unsigned char>& bytes, std::size_t offset)
{
  return static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
         (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
         (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

inline std::uint64_t readLe64(const std::vector<unsigned char>& bytes, std::size_t offset)
{
  std::uint64_t value = 0U;
  for (std::uint32_t shift = 0U; shift < 64U; shift += 8U) {
    value |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
  }
  return value;
}

} // namespace CtfTestSupport
