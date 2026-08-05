/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.h"
#include <gtest/gtest.h>
#include "TraceEvent.h"
#include "TraceSelection.h"
#include "csv/CsvRowMapper.h"
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

TEST(CtraceUnitTests, testCsvRowMapperAndTraceEventSchema)
{
  const std::array<std::string_view, 9> expectedTypes{{
      "itm",
      "dwt",
      "event",
      "pmu",
      "exception",
      "pcsample",
      "global_ts",
      "overflow",
      "error",
  }};
  require(kTraceEventTypeNames == expectedTypes, "trace event type names mismatch");
  for (const auto type : kTraceEventTypeNames) {
    require(parseTraceEventType(type).has_value(), "declared trace event type should parse");
  }
  require(!parseTraceEventType("timestamp").has_value(), "undeclared trace event type should be rejected");
  const std::vector<std::pair<TraceEvent, std::optional<TraceEventType>>> semanticTypes{
      {TraceEvent{SoftwareTraceEvent{}}, TraceEventType::Itm},
      {TraceEvent{DwtDataTraceEvent{}}, TraceEventType::Dwt},
      {TraceEvent{DwtAddressTraceEvent{0U, DwtPcTraceLocation{0U}}}, TraceEventType::Dwt},
      {TraceEvent{ExceptionTraceEvent{}}, TraceEventType::Exception},
      {TraceEvent{DwtEventTraceEvent{}}, std::nullopt},
      {TraceEvent{PmuTraceEvent{}}, std::nullopt},
      {TraceEvent{LocalTimestampTraceEvent{}}, std::nullopt},
      {TraceEvent{GlobalTimestampTraceEvent{}}, TraceEventType::GlobalTimestamp},
      {TraceEvent{OverflowTraceEvent{}}, TraceEventType::Overflow},
      {TraceEvent{SyncTraceEvent{}}, std::nullopt},
      {TraceEvent{TraceIssueEvent{}}, TraceEventType::Error},
  };
  for (const auto& [event, expectedType] : semanticTypes) {
    require(traceEventType(event) == expectedType, "semantic TraceEvent type mapping mismatch");
  }

  require(CsvRowMapper::header() == "cycles,stream,type,source,value,pc,offset,note",
          "CSV schema header integration mismatch");
  require(CsvRowMapper::row(TraceEvent{ExceptionTraceEvent{11U, ExceptionAction::Entered}}) == ",0,exception,11,0x1,,,",
          "CSV exception value mismatch");
  require(CsvRowMapper::row(TraceEvent{DwtDataTraceEvent{0U, 1U, 0x0aU, AccessType::Write}}) == ",0,dwt,0,0x0a,,,",
          "CSV must render the raw hexadecimal DWT value with the one-byte SWO width");
  require(CsvRowMapper::row(TraceEvent{DwtDataTraceEvent{0U, 2U, 0x0aU, AccessType::Write}}) == ",0,dwt,0,0x000a,,,",
          "CSV must render the raw hexadecimal DWT value with the two-byte SWO width");
  require(CsvRowMapper::row(softwarePacket(1U)) == ",0,itm,1,0x00,,,",
          "CSV must write Trace Bus ID 0 for unformatted input");
}

TEST(CtraceUnitTests, testCsvRowMapperCoversAddressAndExceptionVariants)
{
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{DwtAddressTraceEvent{2U, DwtOffsetTraceLocation{0xabcdU}}}),
            ",0,dwt,2,,,0xabcd,");
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{DwtAddressTraceEvent{3U, DwtPcAndOffsetTraceLocation{0x1234U, 0x56U}}}),
            ",0,dwt,3,,0x00001234,0x0056,");
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{ExceptionTraceEvent{1U, ExceptionAction::Exited}}), ",0,exception,1,0x2,,,");
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{ExceptionTraceEvent{1U, ExceptionAction::Returned}}), ",0,exception,1,0x3,,,");
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{ExceptionTraceEvent{1U, ExceptionAction::Unknown}}), ",0,exception,1,,,,");
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{ExceptionTraceEvent{1U, static_cast<ExceptionAction>(99)}}),
            ",0,exception,1,0x0,,,");
}

TEST(CtraceUnitTests, testCsvRowMapperEscapesDiagnosticText)
{
  const auto issue = onStream(issuePacket(TraceIssueCode::DecodeError, "comma, quote \" and\nnewline"), 7U);
  EXPECT_EQ(CsvRowMapper::row(issue), ",7,error,,,,,\"comma, quote \"\" and\nnewline\"");

  EXPECT_EQ(CsvRowMapper::row(atCycle(TraceEvent{GlobalTimestampTraceEvent{123U, false}}, 99U)),
            "123,0,global_ts,,,,,");
}

TEST(CtraceUnitTests, testCsvRowMapperHandlesInternalAndCustomOverflowEvents)
{
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{DwtEventTraceEvent{0U, 1U, 1U}}), ",0,,,,,,");
  TraceEvent overflow{OverflowTraceEvent{"custom overflow"}};
  EXPECT_EQ(CsvRowMapper::row(overflow), ",0,overflow,,,,,custom overflow");
}
