/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfUuid.h"

#include <cstddef>
#include <iomanip>
#include <random>
#include <sstream>

CtfUuid CtfUuid::randomV4()
{
  std::array<std::uint8_t, 16U> bytes{};
  std::random_device random;
  for (auto& byte : bytes) {
    byte = static_cast<std::uint8_t>(random());
  }
  bytes[6U] = static_cast<std::uint8_t>((bytes[6U] & 0x0fU) | 0x40U);
  bytes[8U] = static_cast<std::uint8_t>((bytes[8U] & 0x3fU) | 0x80U);
  return CtfUuid{bytes};
}

std::string CtfUuid::toString() const
{
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (std::size_t index = 0; index < m_bytes.size(); ++index) {
    if (index == 4U || index == 6U || index == 8U || index == 10U) {
      out << '-';
    }
    out << std::setw(2) << static_cast<unsigned>(m_bytes[index]);
  }
  return out.str();
}
