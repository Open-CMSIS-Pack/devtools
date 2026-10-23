/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceInputChannel.h"

#include <algorithm>
#include <string_view>

/** @brief Checks the CMSIS RestrictedString character set. */
static bool isRestrictedStringCharacter(const char character)
{
  return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
         (character >= '0' && character <= '9') || character == '_' || character == '-';
}

bool isTraceBufferChannel(std::string_view value)
{
  constexpr std::string_view namedPrefix = "TB_";
  if (value == "TB") {
    return true;
  }
  if (value.size() <= namedPrefix.size() || value.substr(0U, namedPrefix.size()) != namedPrefix) {
    return false;
  }
  const auto name = value.substr(namedPrefix.size());
  return std::all_of(name.begin(), name.end(), isRestrictedStringCharacter);
}

bool isSupportedTraceChannel(std::string_view value)
{
  return value == "SWO" || isTraceBufferChannel(value);
}
