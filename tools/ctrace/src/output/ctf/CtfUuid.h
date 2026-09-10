/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_CTFUUID_H
#define CTRACE_SRC_OUTPUT_CTF_CTFUUID_H

#include <array>
#include <cstdint>
#include <string>

/** @brief Stores one binary UUID used by a CTF trace or clock domain. */
class CtfUuid final {
public:
  /** @brief Creates an all-zero UUID value. */
  constexpr CtfUuid() = default;

  /** @brief Creates a UUID from its 16 encoded bytes. */
  explicit constexpr CtfUuid(std::array<std::uint8_t, 16U> bytes)
    : m_bytes(bytes)
  {
  }

  /** @brief Generates a random RFC 4122 version-4 UUID. */
  static CtfUuid randomV4();

  /** @brief Returns the encoded UUID bytes. */
  constexpr const std::array<std::uint8_t, 16U>& bytes() const noexcept
  {
    return m_bytes;
  }

  /** @brief Returns the canonical lower-case textual representation. */
  std::string toString() const;

private:
  std::array<std::uint8_t, 16U> m_bytes{};
};

/** @brief Compares complete UUID values. */
inline bool operator==(const CtfUuid& left, const CtfUuid& right) noexcept
{
  return left.bytes() == right.bytes();
}

/** @brief Compares complete UUID values. */
inline bool operator!=(const CtfUuid& left, const CtfUuid& right) noexcept
{
  return !(left == right);
}

/** @brief Orders UUID values lexicographically for associative containers. */
inline bool operator<(const CtfUuid& left, const CtfUuid& right) noexcept
{
  return left.bytes() < right.bytes();
}

#endif // CTRACE_SRC_OUTPUT_CTF_CTFUUID_H
