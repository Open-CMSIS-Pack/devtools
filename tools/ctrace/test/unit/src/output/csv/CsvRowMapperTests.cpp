/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.h"
#include <gtest/gtest.h>
#include "TraceEvent.h"
#include "TraceRoute.h"
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
  ASSERT_TRUE(kTraceEventTypeNames == expectedTypes) << "trace event type names mismatch";
  for (const auto type : kTraceEventTypeNames) {
    ASSERT_TRUE(parseTraceEventType(type).has_value()) << "declared trace event type should parse";
  }
  ASSERT_TRUE(!parseTraceEventType("timestamp").has_value()) << "undeclared trace event type should be rejected";
  EXPECT_FALSE(parseTraceEventType("info").has_value()) << "input annotations do not add a selectable event type";
  const std::vector<std::pair<TraceEvent, std::optional<TraceEventType>>> semanticTypes{
      {TraceEvent{SoftwareTraceEvent{}}, TraceEventType::Itm},
      {TraceEvent{DwtDataTraceEvent{}}, TraceEventType::Dwt},
      {TraceEvent{DwtAddressTraceEvent{0U, DwtPcTraceLocation{{4U, 0U}}}}, TraceEventType::Dwt},
      {TraceEvent{DwtMatchTraceEvent{}}, TraceEventType::Dwt},
      {TraceEvent{ExceptionTraceEvent{}}, TraceEventType::Exception},
      {TraceEvent{DwtEventTraceEvent{}}, TraceEventType::Event},
      {TraceEvent{PmuTraceEvent{}}, TraceEventType::Pmu},
      {TraceEvent{PcSampleTraceEvent{}}, TraceEventType::PcSample},
      {TraceEvent{LocalTimestampTraceEvent{}}, std::nullopt},
      {TraceEvent{GlobalTimestampTraceEvent{}}, TraceEventType::GlobalTimestamp},
      {TraceEvent{OverflowTraceEvent{}}, TraceEventType::Overflow},
      {TraceEvent{SyncTraceEvent{}}, std::nullopt},
      {TraceEvent{TraceIssueEvent{}}, TraceEventType::Error},
  };
  for (const auto& [event, expectedType] : semanticTypes) {
    ASSERT_TRUE(traceEventType(event) == expectedType) << "semantic TraceEvent type mapping mismatch";
  }

  ASSERT_TRUE(CsvRowMapper::header() == "cycles,stream,type,source,value,pc,address,note")
      << "CSV schema header integration mismatch";
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{LocalTimestampTraceEvent{}}), ",,,,,,,")
      << "local timestamp control packets must not populate payload-specific CSV columns";
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{SyncTraceEvent{}}), ",,,,,,,")
      << "synchronization control packets must not populate payload-specific CSV columns";
  ASSERT_TRUE(
      (CsvRowMapper::row(TraceEvent{ExceptionTraceEvent{11U, ExceptionAction::Entered}}) == ",,exception,11,0x1,,,"))
      << "CSV exception value mismatch";
  ASSERT_TRUE(CsvRowMapper::row(TraceEvent{DwtDataTraceEvent{0U, 1U, 0x0aU, AccessType::Write}}) == ",,dwt,0,0x0a,,,")
      << "CSV must render the raw hexadecimal DWT value with the one-byte SWO width";
  ASSERT_TRUE(
      (CsvRowMapper::row(TraceEvent{DwtDataTraceEvent{0U, 2U, 0x0aU, AccessType::Write}}) == ",,dwt,0,0x000a,,,"))
      << "CSV must render the raw hexadecimal DWT value with the two-byte SWO width";
  ASSERT_TRUE(CsvRowMapper::row(softwarePacket(1U)) == ",,itm,1,0x00,,,")
      << "CSV must leave the stream field empty for unformatted input";
  EXPECT_EQ(CsvRowMapper::row(softwarePacket(1U, 1U, 0U)), ",,itm,1,0x00,,,");
  EXPECT_EQ(CsvRowMapper::row(softwarePacket(1U, 2U, 0U)), ",,itm,1,0x0000,,,");
  EXPECT_EQ(CsvRowMapper::row(softwarePacket(1U, 4U, 0U)), ",,itm,1,0x00000000,,,");
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{DwtDataTraceEvent{0U, 1U, 0U, AccessType::Write}}), ",,dwt,0,0x00,,,");
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{DwtDataTraceEvent{0U, 2U, 0U, AccessType::Write}}), ",,dwt,0,0x0000,,,");
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{DwtDataTraceEvent{0U, 4U, 0U, AccessType::Write}}), ",,dwt,0,0x00000000,,,");
  ASSERT_TRUE(CsvRowMapper::row(TraceEvent{DwtMatchTraceEvent{2U}}) == ",,dwt,2,,,,")
      << "CSV must expose a match only through its DWT comparator source";
  ASSERT_TRUE(CsvRowMapper::row(atCycle(TraceEvent{PcSampleTraceEvent{0x08001234U}}, 949339000U)) ==
              "949339000,,pcsample,,,0x08001234,,")
      << "CSV PC-sample row mismatch";
  ASSERT_TRUE(CsvRowMapper::row(atCycle(TraceEvent{PcSampleTraceEvent{0U, PcSampleKind::Sleep}}, 949339100U)) ==
              "949339100,,pcsample,,,,,CPU Sleeping")
      << "CSV PC-sample sleep row mismatch";
  EXPECT_EQ(CsvRowMapper::row(onStream(
                atCycle(TraceEvent{PcSampleTraceEvent{0U, PcSampleKind::TraceProhibited}}, 949339200U), 4U)),
            "949339200,4,pcsample,,,,,Trace prohibited");
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{PcSampleTraceEvent{0xffU}}), ",,pcsample,,,0x000000ff,,")
      << "a four-byte PC value must not be rendered as a status marker";
}

TEST(CtraceUnitTests, testCsvRowMapperCoversAddressAndExceptionVariants)
{
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{DwtAddressTraceEvent{2U, DwtDataAddressTraceLocation{{2U, 0xabcdU}}}}),
            ",,dwt,2,,,0xabcd,");
  EXPECT_EQ(
      CsvRowMapper::row(
          TraceEvent{DwtAddressTraceEvent{3U, DwtPcAndDataAddressTraceLocation{{2U, 0x1234U}, {1U, 0x56U}}}}),
      ",,dwt,3,,0x1234,0x56,");
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{DwtAddressTraceEvent{1U, DwtDataAddressTraceLocation{{4U, 0x20007858U}}}}),
            ",,dwt,1,,,0x20007858,");
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{ExceptionTraceEvent{1U, ExceptionAction::Exited}}), ",,exception,1,0x2,,,");
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{ExceptionTraceEvent{1U, ExceptionAction::Returned}}), ",,exception,1,0x3,,,");
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{ExceptionTraceEvent{1U, ExceptionAction::Unknown}}), ",,exception,1,,,,");
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{ExceptionTraceEvent{1U, static_cast<ExceptionAction>(99)}}),
            ",,exception,1,0x0,,,");
}

TEST(CtraceUnitTests, testCsvRowMapperEscapesDiagnosticText)
{
  const auto issue = onStream(issuePacket(TraceIssueCode::DecodeError, "comma, quote \" and\nnewline"), 7U);
  EXPECT_EQ(CsvRowMapper::row(issue), ",7,error,,,,,\"comma, quote \"\" and\nnewline\"");

  const auto missingSync =
      onStream(issuePacket(TraceIssueCode::OpenCsdMissingSync, "no sync, \"stream\"\r\nended"), 1U);
  EXPECT_EQ(CsvRowMapper::row(missingSync), ",1,error,,,,,\"no sync, \"\"stream\"\"\r\nended\"");

  EXPECT_EQ(CsvRowMapper::row(atCycle(TraceEvent{GlobalTimestampTraceEvent{123U, false}}, 99U)), "123,,global_ts,,,,,");
}

TEST(CtraceUnitTests, testCsvRowMapperMapsByteSkipReasonsWithoutInventingTimeOrRoute)
{
  EXPECT_EQ(CsvRowMapper::byteSkipRow({0U, 1U}),
            ",,info,,,,,1 bytes skipped due to missing source ID; first formatter group at raw offset 0");
  EXPECT_EQ(CsvRowMapper::byteSkipRow({16U, 5U, TraceByteSkipReason::NullSourceId, 0U}),
            ",0,info,,,,,5 bytes skipped for null source ID 0; first formatter group at raw offset 16");
  EXPECT_EQ(CsvRowMapper::byteSkipRow({32U, 3U, TraceByteSkipReason::ReservedSourceId, 127U}),
            ",127,info,,,,,3 bytes skipped for reserved source ID 127; first formatter group at raw offset 32");
  EXPECT_EQ(CsvRowMapper::byteSkipRow({48U, 2U, TraceByteSkipReason::UnconfiguredSourceId, 42U}),
            ",42,info,,,,,2 bytes skipped for unconfigured source ID 42; first formatter group at raw offset 48");
  EXPECT_EQ(CsvRowMapper::byteSkipRow({64U, 8U, TraceByteSkipReason::MissingSync, 1U}),
            ",1,info,,,,,8 bytes skipped due to missing SYNC; first formatter group at raw offset 64");
  EXPECT_EQ(CsvRowMapper::byteSkipRow({4294967296ULL, 8589934592ULL}),
            ",,info,,,,,8589934592 bytes skipped due to missing source ID; "
            "first formatter group at raw offset 4294967296");
}

TEST(CtraceUnitTests, testCsvRowMapperSerializesOnlyArchitecturalTraceBusId)
{
  const TraceRouteIdentity noBusRoute{TraceRouteId{97U}, std::nullopt};
  const TraceRouteIdentity formattedRoute{TraceRouteId{97U}, 7U};

  EXPECT_EQ(CsvRowMapper::row(onRoute(softwarePacket(1U, 1U, 0x2aU), noBusRoute)), ",,itm,1,0x2a,,,")
      << "an internal route ordinal must never appear in the CSV stream column";
  EXPECT_EQ(CsvRowMapper::row(onRoute(softwarePacket(1U, 1U, 0x2aU), formattedRoute)), ",7,itm,1,0x2a,,,")
      << "CSV must serialize the architectural Trace Bus ID rather than the internal route ordinal";

  TraceEvent overflow{OverflowTraceEvent{"route overflow"}};
  EXPECT_EQ(CsvRowMapper::row(onRoute(std::move(overflow), formattedRoute)), ",7,overflow,,,,,route overflow");
}

TEST(CtraceUnitTests, testCsvRowMapperHandlesInternalAndCustomOverflowEvents)
{
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{DwtEventTraceEvent{0x21U}}), ",,event,0,0x21,,,");
  EXPECT_EQ(CsvRowMapper::row(TraceEvent{PmuTraceEvent{0x81U}}), ",,pmu,3,0x81,,,");
  TraceEvent overflow{OverflowTraceEvent{"custom overflow"}};
  EXPECT_EQ(CsvRowMapper::row(overflow), ",,overflow,,,,,custom overflow");
}
