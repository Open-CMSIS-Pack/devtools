/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "Messages.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct CatalogRow {
  MessageId id;
  const char* name;
  const char* detailed;
  const char* compact;
};

constexpr std::array<CatalogRow, static_cast<std::size_t>(MessageId::Count)> rows{{
#define CTRACE_MESSAGE(id, detailed, compact) {MessageId::id, #id, detailed, compact},
#include "MessageCatalog.inc"
#undef CTRACE_MESSAGE
}};

/** @brief Supplies the catalog's runtime argument count through the initializer-list API. */
template <std::size_t... Indices>
std::string formatArguments(MessageId id, MessageStyle style, const std::vector<std::string>& values,
                            std::index_sequence<Indices...>)
{
  return formatMessage(id, style, {values[Indices]...});
}

template <std::size_t Count = 0U>
std::string formatArguments(MessageId id, MessageStyle style, const std::vector<std::string>& values)
{
  if (values.size() == Count) {
    return formatArguments(id, style, values, std::make_index_sequence<Count>{});
  }
  if constexpr (Count < 16U) {
    return formatArguments<Count + 1U>(id, style, values);
  }
  throw std::logic_error("extend catalog test argument dispatcher");
}

/** @brief Independently expands catalog tokens with a regex oracle rather than the production scanner. */
std::string expectedText(const std::string& pattern, const std::vector<std::string>& values)
{
  static const std::regex tokens(R"(\{([0-9]+)\})");
  std::string result;
  std::size_t previous = 0U;
  for (std::sregex_iterator token(pattern.begin(), pattern.end(), tokens), end; token != end; ++token) {
    result.append(pattern, previous, static_cast<std::size_t>(token->position()) - previous);
    result += values.at(static_cast<std::size_t>(std::stoul((*token)[1].str())));
    previous = static_cast<std::size_t>(token->position() + token->length());
  }
  result.append(pattern, previous, std::string::npos);
  return result;
}

} // namespace

TEST(MessagesTests, EveryCatalogEntryFormatsBothStylesAndValidatesTheSharedArgumentCount)
{
  for (const auto& row : rows) {
    SCOPED_TRACE(row.name);
    const auto count = messageArgumentCount(row.id);
    ASSERT_LT(count, 16U);
    std::vector<std::string> arguments;
    for (std::size_t index = 0U; index < count; ++index) {
      arguments.push_back("argument[" + std::to_string(index) + "]: {opaque}, \"quoted\"\n");
    }
    for (const auto style : {MessageStyle::Detailed, MessageStyle::Compact}) {
      const auto* pattern = style == MessageStyle::Detailed || row.compact == nullptr ? row.detailed : row.compact;
      EXPECT_EQ(messageTemplate(row.id, style), pattern);
      EXPECT_EQ(formatArguments(row.id, style, arguments), expectedText(pattern, arguments));
      if (count != 0U) {
        auto missing = arguments;
        missing.pop_back();
        EXPECT_THROW(formatArguments(row.id, style, missing), std::invalid_argument);
      }
      auto extra = arguments;
      extra.push_back("extra");
      EXPECT_THROW(formatArguments(row.id, style, extra), std::invalid_argument);
    }
  }
}

TEST(MessagesTests, ArgumentsOwnTheirValuesAndPreserveOpaqueText)
{
  std::string value = "{0}, \"native detail\"";
  const MessageArgument fromString(value);
  const MessageArgument fromView{std::string_view(value)};
  const MessageArgument fromPointer(value.c_str());
  value.assign("changed");
  EXPECT_EQ(fromString.text(), "{0}, \"native detail\"");
  EXPECT_EQ(fromView.text(), fromString.text());
  EXPECT_EQ(fromPointer.text(), fromString.text());
  EXPECT_EQ(MessageArgument(std::string_view{}).text(), "");
  EXPECT_EQ(MessageArgument("").text(), "");
  const auto binary = std::string("a\0b", 3U);
  EXPECT_EQ(MessageArgument(std::string_view(binary)).text(), binary);
  EXPECT_THROW(MessageArgument(static_cast<const char*>(nullptr)), std::invalid_argument);
}

TEST(MessagesTests, IntegralArgumentsUseDecimalWithoutNarrowing)
{
  EXPECT_EQ(MessageArgument(std::numeric_limits<std::uint64_t>::max()).text(), "18446744073709551615");
  EXPECT_EQ(MessageArgument(std::numeric_limits<std::int64_t>::min()).text(), "-9223372036854775808");
  EXPECT_EQ(MessageArgument(std::uint8_t{255U}).text(), "255");
  EXPECT_EQ(MessageArgument(std::int8_t{-128}).text(), "-128");
  EXPECT_EQ(MessageArgument(std::size_t{42U}).text(), "42");
  EXPECT_EQ(MessageArgument(false).text(), "0");
  EXPECT_EQ(MessageArgument(0).text(), "0");
}

TEST(MessagesTests, FormatsWideAndEmptyValuesWithoutRescanningArgumentPlaceholders)
{
  EXPECT_EQ(formatMessage(MessageId::SpecifyTraceDirectory), "Specify <trace-dir>");
  EXPECT_EQ(formatMessage(MessageId::DiagnosticUnknownNormalizedRoute, MessageStyle::Detailed,
                          {std::numeric_limits<std::uint64_t>::max()}),
            "OpenCSD element references unknown normalized route 18446744073709551615");
  EXPECT_EQ(formatMessage(MessageId::DiagnosticLocation, MessageStyle::Compact, {"", "{0}: {{opaque}}"}),
            ": {0}: {{opaque}}");
  const auto binary = std::string("a\0b", 3U);
  EXPECT_EQ(formatMessage(MessageId::DiagnosticLocation, MessageStyle::Detailed, {"location", std::string_view(binary)}),
            std::string("location: ") + binary);
}

TEST(MessagesTests, InvalidIdsAndStylesAreRejected)
{
  for (const auto id : {MessageId::Count, static_cast<MessageId>(std::numeric_limits<std::size_t>::max())}) {
    EXPECT_THROW(formatMessage(id), std::invalid_argument);
    EXPECT_THROW(messageTemplate(id), std::invalid_argument);
    EXPECT_THROW(messageArgumentCount(id), std::invalid_argument);
  }
  const auto invalidStyle = static_cast<MessageStyle>(-1);
  ASSERT_FALSE(rows.empty());
  EXPECT_THROW(messageTemplate(rows.front().id, invalidStyle), std::invalid_argument);
  const std::vector<std::string> arguments(messageArgumentCount(rows.front().id), "argument");
  EXPECT_THROW(formatArguments(rows.front().id, invalidStyle, arguments), std::invalid_argument);
}
