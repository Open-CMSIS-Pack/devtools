/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DECODE_SATURATINGARITHMETIC_H
#define CTRACE_SRC_DECODE_SATURATINGARITHMETIC_H

#include <cstdint>
#include <limits>
#include <stdexcept>

/** @brief Provides checked saturating arithmetic for trace timestamps. */
class SaturatingArithmetic final {
public:
  /** @brief Adds two values and clamps overflow to the largest representable value. */
  static constexpr std::uint64_t add(std::uint64_t lhs, std::uint64_t rhs)
  {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    return rhs > maximum - lhs ? maximum : lhs + rhs;
  }

  /** @brief Multiplies by a nonzero factor and clamps overflow to the largest representable value. */
  static constexpr std::uint64_t multiply(std::uint64_t value, std::uint32_t factor)
  {
    if (factor == 0U) {
      throw std::invalid_argument("ITM timestamp prescaler must not be zero");
    }
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    return value > maximum / factor ? maximum : value * factor;
  }

private:
  SaturatingArithmetic() = delete;
};

#endif  // CTRACE_SRC_DECODE_SATURATINGARITHMETIC_H
