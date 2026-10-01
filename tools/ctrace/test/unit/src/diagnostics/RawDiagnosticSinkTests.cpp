/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "DiagnosticSink.h"
#include "RawDiagnosticSink.h"
#include "TestSupport.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/** @brief Finds a technical context field without depending on field order. */
static std::optional<std::string> field(const DiagnosticSink::Event& event, const std::string& name)
{
  const auto found = std::find_if(event.detailedContext.begin(), event.detailedContext.end(),
                                [&](const auto& item) { return item.first == name; });
  return found == event.detailedContext.end() ? std::nullopt : std::optional<std::string>{found->second};
}

/** @brief Creates an independently classified diagnostic at a decoder position. */
static DiagnosticSink::Event located(std::uint64_t offset)
{
  DiagnosticSink::Event event{DiagnosticSink::Severity::Error, "Invalid ITM packet sequence", {{"stream", "1"}}};
  event.rawLocation = RawDiagnosticLocation{};
  event.rawLocation->offset = offset;
  return event;
}

TEST(CtraceUnitTests, testRawDiagnosticContextPreservesEventAndExactPacketLocation)
{
  CollectingDiagnosticSink target;
  RawDiagnosticSink sink(target, false, 200U);
  auto event = located(10U);
  event.detailedMessage = "packet details";
  event.detailedContext = {{"native_code", "19"}};
  event.rawLocation->packetSize = 2U;
  event.rawLocation->previousSyncOffset = 0U;
  sink.report(event);
  ASSERT_EQ(target.events().size(), 1U);
  const auto& actual = target.events().front();
  EXPECT_EQ(actual.message, event.message);
  EXPECT_EQ(actual.context, event.context);
  EXPECT_EQ(actual.detailedMessage, event.detailedMessage);
  EXPECT_EQ(actual.rawLocation->offset, 10U);
  EXPECT_EQ(field(actual, "native_code"), "19");
  EXPECT_EQ(field(actual, "raw_offset"), "10");
  EXPECT_EQ(field(actual, "raw_end"), "12");
  EXPECT_EQ(field(actual, "position_kind"), "exact");
  EXPECT_EQ(field(actual, "packet_bytes_kind"), "file");
  EXPECT_EQ(field(actual, "previous_sync_offset"), "0");
  EXPECT_EQ(field(actual, "next_sync_offset"), "unknown");
  EXPECT_EQ(field(actual, "read_offset"), "0");
  EXPECT_EQ(field(actual, "read_length"), "128");
  EXPECT_EQ(target.failureCount(), 1U);

  DiagnosticSink::Event operational{DiagnosticSink::Severity::Info, "metadata"};
  operational.visibility = DiagnosticSink::Visibility::Verbose;
  sink.report(operational);
  EXPECT_TRUE(target.events().back().detailedContext.empty());
  EXPECT_EQ(target.events().back().visibility, DiagnosticSink::Visibility::Verbose);
  EXPECT_EQ(target.failureCount(), 1U);
}

TEST(CtraceUnitTests, testRawDiagnosticFormattedLocationsRemainHints)
{
  CollectingDiagnosticSink target;
  RawDiagnosticSink sink(target, true, 256U);
  auto event = located(143U);
  event.rawLocation->packetSize = 2U;
  sink.report(event);
  const auto& packet = target.events().back();
  EXPECT_EQ(field(packet, "position_kind"), "formatter_hint");
  EXPECT_EQ(field(packet, "packet_bytes_kind"), "deformatted");
  EXPECT_FALSE(field(packet, "raw_end").has_value());
  EXPECT_EQ(field(packet, "read_offset"), "64");
  EXPECT_EQ(field(packet, "read_length"), "128");

  event.rawLocation->endOffset = 224U;
  event.rawLocation->nextSyncOffset = 224U;
  sink.report(event);
  EXPECT_EQ(field(target.events().back(), "raw_end"), "224");
  EXPECT_EQ(field(target.events().back(), "next_sync_offset"), "224");

  event.rawLocation->kind = RawDiagnosticLocation::Kind::FormatterGroup;
  RawDiagnosticSink unknownFormat(target, false, 256U);
  unknownFormat.report(event);
  EXPECT_EQ(field(target.events().back(), "position_kind"), "formatter_hint");
  EXPECT_FALSE(field(target.events().back(), "previous_sync_offset").has_value());
}

TEST(CtraceUnitTests, testRawDiagnosticInspectionWindowsStayBoundedAtFileEdges)
{
  constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
  for (const bool formatted : {false, true}) {
    for (const std::uint64_t size : {std::uint64_t{0U}, std::uint64_t{16U}, std::uint64_t{256U}, maximum}) {
      CollectingDiagnosticSink target;
      RawDiagnosticSink sink(target, formatted, size);
      for (const std::uint64_t offset : {std::uint64_t{0U}, size / 2U, size}) {
        auto event = located(offset);
        event.rawLocation->kind = RawDiagnosticLocation::Kind::InputProgress;
        sink.report(event);
        const auto& actual = target.events().back();
        ASSERT_TRUE(field(actual, "read_offset").has_value());
        ASSERT_TRUE(field(actual, "read_length").has_value());
        const auto start = std::stoull(*field(actual, "read_offset"));
        const auto length = std::stoull(*field(actual, "read_length"));
        EXPECT_LE(start, offset);
        EXPECT_LE(length, 128U);
        EXPECT_LE(length, size - start);
        EXPECT_LE(offset - start, length);
        EXPECT_EQ(field(actual, "position_kind"), "input_progress");
        EXPECT_FALSE(field(actual, "next_sync_offset").has_value());
        if (formatted) {
          EXPECT_EQ(start % 16U, 0U);
        }
      }
    }
  }
}

TEST(CtraceUnitTests, testRawDiagnosticDoesNotInventUnavailableRanges)
{
  CollectingDiagnosticSink target;
  RawDiagnosticSink unknownSize(target, false, std::nullopt);
  auto event = located(std::numeric_limits<std::uint64_t>::max());
  event.rawLocation->packetSize = 2U;
  unknownSize.report(event);
  EXPECT_FALSE(field(target.events().back(), "raw_end").has_value());
  EXPECT_FALSE(field(target.events().back(), "read_offset").has_value());

  RawDiagnosticSink smallFile(target, false, 8U);
  smallFile.report(located(9U));
  EXPECT_EQ(field(target.events().back(), "raw_offset"), "9");
  EXPECT_FALSE(field(target.events().back(), "read_length").has_value());
}

TEST(CtraceUnitTests, testRawDiagnosticFieldsAreVerboseOnlyAndQuotedUnambiguously)
{
  StderrDiagnosticSink target;
  RawDiagnosticSink sink(target, false, 64U);
  auto event = located(10U);
  event.context = {{"input", "folder, with spaces/trace.raw"}};
  event.detailedContext = {{"quoted", "a\"b\\c"}, {"empty", ""}, {"control", std::string("\n\t\r\0\x7f", 5U)},
                           {"list", "[10,20]"}};
  testing::internal::CaptureStderr();
  sink.report(event);
  EXPECT_EQ(testing::internal::GetCapturedStderr(),
            "[error] Invalid ITM packet sequence: input=folder, with spaces/trace.raw\n");

  target.setVerbose(true);
  testing::internal::CaptureStderr();
  sink.report(event);
  const auto text = testing::internal::GetCapturedStderr();
  EXPECT_NE(text.find("input=\"folder, with spaces/trace.raw\""), std::string::npos);
  EXPECT_NE(text.find("quoted=\"a\\\"b\\\\c\""), std::string::npos);
  EXPECT_NE(text.find("empty=\"\""), std::string::npos);
  EXPECT_NE(text.find("control=\"\\u000a\\u0009\\u000d\\u0000\\u007f\""), std::string::npos);
  EXPECT_NE(text.find("list=\"[10,20]\""), std::string::npos);
  EXPECT_NE(text.find("raw_offset=10, position_kind=exact"), std::string::npos);
  EXPECT_NE(text.find("read_offset=0, read_length=64"), std::string::npos);
  EXPECT_EQ(std::count(text.begin(), text.end(), '\n'), 1);
  EXPECT_EQ(target.failureCount(), 2U);
}
