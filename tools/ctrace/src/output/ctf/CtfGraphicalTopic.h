/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_CTFGRAPHICALTOPIC_H
#define CTRACE_SRC_OUTPUT_CTF_CTFGRAPHICALTOPIC_H

#include <array>
#include <cstddef>
#include <cstdint>

/** @brief Identifies one graphical Trace Compass topic backed by emitted CTF records. */
enum class CtfGraphicalTopic : std::uint8_t {
  DwtValue,
  DwtAddress,
  DwtMatch,
  DwtEvent,
  PmuEvent,
  Exception,
  ProcessorState,
  Count,
};

/** @brief Lists every graphical topic in declaration order. */
inline constexpr std::array<CtfGraphicalTopic, static_cast<std::size_t>(CtfGraphicalTopic::Count)> kCtfGraphicalTopics{{
    CtfGraphicalTopic::DwtValue,
    CtfGraphicalTopic::DwtAddress,
    CtfGraphicalTopic::DwtMatch,
    CtfGraphicalTopic::DwtEvent,
    CtfGraphicalTopic::PmuEvent,
    CtfGraphicalTopic::Exception,
    CtfGraphicalTopic::ProcessorState,
}};

/** @brief Checks that the topic list contains every declared topic exactly once. */
constexpr bool hasCompleteCtfGraphicalTopicList() noexcept
{
  std::array<bool, kCtfGraphicalTopics.size()> found{};
  for (const auto topic : kCtfGraphicalTopics) {
    const auto index = static_cast<std::size_t>(topic);
    if (index >= found.size() || found[index]) {
      return false;
    }
    found[index] = true;
  }
  for (const auto present : found) {
    if (!present) {
      return false;
    }
  }
  return true;
}

static_assert(hasCompleteCtfGraphicalTopicList(), "CTF graphical topic list must be complete and unique");

#endif // CTRACE_SRC_OUTPUT_CTF_CTFGRAPHICALTOPIC_H
