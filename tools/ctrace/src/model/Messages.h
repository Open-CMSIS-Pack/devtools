/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_MODEL_MESSAGES_H
#define CTRACE_SRC_MODEL_MESSAGES_H

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

/** @brief Identifies a row in the shared ctrace message catalog. */
enum class MessageId : std::size_t {
#define CTRACE_MESSAGE(id, detailed, compact) id,
#include "MessageCatalog.inc"
#undef CTRACE_MESSAGE
  Count,
};

/** @brief Chooses detailed CLI wording or compact CSV wording. */
enum class MessageStyle {
  Detailed,
  Compact,
};

/** @brief Owns one argument; formatting never interprets placeholders inside its value. */
class MessageArgument {
public:
  MessageArgument(std::string value)
    : m_text(std::move(value))
  {
  }
  MessageArgument(std::string_view value)
    : m_text(value)
  {
  }
  /** @brief Copies a C string, rejecting a null pointer. */
  MessageArgument(const char* value);

  /** @brief Formats integral values as decimal; specialized adapters supply other representations. */
  template <typename Integer, std::enable_if_t<std::is_integral_v<Integer>, int> = 0>
  MessageArgument(Integer value)
    : m_text(std::to_string(value))
  {
  }

  const std::string& text() const noexcept { return m_text; }

private:
  std::string m_text;
};

/**
 * @brief Formats catalog placeholders with owned arguments.
 * @details Both styles receive the same argument list, even if one template omits some positions.
 *          A compact null template reuses the detailed text. Template braces must form positional placeholders.
 * @throws std::invalid_argument For an invalid ID, style, or argument count.
 */
std::string formatMessage(MessageId id, MessageStyle style = MessageStyle::Detailed,
                          std::initializer_list<MessageArgument> arguments = {});

#endif // CTRACE_SRC_MODEL_MESSAGES_H
