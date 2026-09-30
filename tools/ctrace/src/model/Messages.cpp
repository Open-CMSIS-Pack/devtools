/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "Messages.h"

#include <array>
#include <limits>
#include <stdexcept>

namespace {

struct TemplateInfo {
  bool valid = true;
  std::size_t argumentCount = 0;
  bool containsArgument = false;
};

/** @brief Checks syntax and locates a parameter without allocating during constant evaluation. */
constexpr TemplateInfo inspectTemplate(std::string_view text, std::size_t requestedArgument = 0U)
{
  TemplateInfo info;
  for (std::size_t cursor = 0U; cursor < text.size(); ++cursor) {
    const auto character = text[cursor];
    if (character != '{' && character != '}') {
      continue;
    }
    if (character == '}') {
      return {false, 0U, false};
    }
    ++cursor;
    const auto start = cursor;
    std::size_t argument = 0U;
    while (cursor < text.size() && text[cursor] >= '0' && text[cursor] <= '9') {
      const auto digit = static_cast<std::size_t>(text[cursor] - '0');
      if (argument > (std::numeric_limits<std::size_t>::max() - digit) / 10U) {
        return {false, 0U, false};
      }
      argument = argument * 10U + digit;
      ++cursor;
    }
    if (cursor == start || cursor == text.size() || text[cursor] != '}' ||
        argument == std::numeric_limits<std::size_t>::max()) {
      return {false, 0U, false};
    }
    if (argument >= info.argumentCount) {
      info.argumentCount = argument + 1U;
    }
    info.containsArgument = info.containsArgument || argument == requestedArgument;
  }
  return info;
}

constexpr std::size_t catalogArgumentCount(const char* detailed, const char* compact)
{
  const auto detailedCount = inspectTemplate(detailed).argumentCount;
  const auto compactCount = compact == nullptr ? 0U : inspectTemplate(compact).argumentCount;
  return detailedCount > compactCount ? detailedCount : compactCount;
}

/** @brief Requires valid templates and a contiguous union of argument positions in each catalog row. */
constexpr bool validCatalogEntry(const char* detailed, const char* compact)
{
  if (detailed == nullptr || !inspectTemplate(detailed).valid ||
      (compact != nullptr && !inspectTemplate(compact).valid)) {
    return false;
  }
  const auto count = catalogArgumentCount(detailed, compact);
  const auto length = std::string_view(detailed).size() +
                       (compact == nullptr ? 0U : std::string_view(compact).size());
  // Each argument needs at least three characters; reject implausible indices before the bounded search.
  if (count > length / 3U) {
    return false;
  }
  for (std::size_t argument = 0U; argument < count; ++argument) {
    if (!inspectTemplate(detailed, argument).containsArgument &&
        (compact == nullptr || !inspectTemplate(compact, argument).containsArgument)) {
      return false;
    }
  }
  return true;
}

// Evaluate each row separately so growing the catalog does not exhaust a compiler's per-expression step budget.
#define CTRACE_MESSAGE(id, detailed, compact) \
  static_assert(validCatalogEntry(detailed, compact), "Invalid message templates: " #id); \
  constexpr std::size_t kArgumentCount##id = catalogArgumentCount(detailed, compact);
#include "MessageCatalog.inc"
#undef CTRACE_MESSAGE

struct CatalogEntry {
  const char* detailed;
  const char* compact;
  std::size_t argumentCount;
};

constexpr std::array<CatalogEntry, static_cast<std::size_t>(MessageId::Count)> catalog{{
#define CTRACE_MESSAGE(id, detailed, compact) {detailed, compact, kArgumentCount##id},
#include "MessageCatalog.inc"
#undef CTRACE_MESSAGE
}};

/** @brief Retrieves fixed internal failure text without recursively invoking the formatter. */
const char* contractErrorText(MessageId id) noexcept
{
  return catalog[static_cast<std::size_t>(id)].detailed;
}

const CatalogEntry& catalogEntry(MessageId id)
{
  const auto index = static_cast<std::size_t>(id);
  if (index >= catalog.size()) {
    throw std::invalid_argument(contractErrorText(MessageId::MessageInvalidId));
  }
  return catalog[index];
}

std::string_view selectTemplate(const CatalogEntry& entry, MessageStyle style)
{
  switch (style) {
  case MessageStyle::Detailed:
    return entry.detailed;
  case MessageStyle::Compact:
    return entry.compact == nullptr ? entry.detailed : entry.compact;
  }
  throw std::invalid_argument(contractErrorText(MessageId::MessageInvalidStyle));
}

} // namespace

MessageArgument::MessageArgument(const char* value)
{
  if (value == nullptr) {
    throw std::invalid_argument(contractErrorText(MessageId::MessageNullArgument));
  }
  m_text = value;
}

std::string formatMessage(MessageId id, MessageStyle style, std::initializer_list<MessageArgument> arguments)
{
  const auto& entry = catalogEntry(id);
  if (arguments.size() != entry.argumentCount) {
    throw std::invalid_argument(contractErrorText(MessageId::MessageArgumentCountMismatch));
  }
  const auto pattern = selectTemplate(entry, style);
  std::string text;
  text.reserve(pattern.size());
  for (std::size_t cursor = 0U; cursor < pattern.size(); ++cursor) {
    const auto character = pattern[cursor];
    if (character != '{') {
      text += character;
    } else {
      // Catalog validation guarantees a closed decimal placeholder and a supplied argument at this position.
      std::size_t argument = 0U;
      ++cursor;
      while (pattern[cursor] != '}') {
        argument = argument * 10U + static_cast<std::size_t>(pattern[cursor] - '0');
        ++cursor;
      }
      text += arguments.begin()[argument].text();
    }
  }
  return text;
}
