/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.hpp"
#include <gtest/gtest.h>
#include "TraceEvent.hpp"
#include "TraceSelection.hpp"
#include "csv/CsvRowMapper.hpp"
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

TEST(CtraceUnitTests, testCsvRowMapperAndTraceEventSchema)
{
  const std::array<std::string_view, 6> expectedTypes{{
      "itm",
      "dwt",
      "exception",
      "global_ts",
      "overflow",
      "error",
  }};
  require(kTraceEventTypeNames == expectedTypes, "trace event type names mismatch");
  for (const auto type : kTraceEventTypeNames) {
    require(parseTraceEventType(type).has_value(), "declared trace event type should parse");
  }
  for (const auto* type : {"event", "pmu", "pcsample"}) {
    require(!parseTraceEventType(type).has_value(),
            std::string(type) + " must not be selectable before a semantic output event exists");
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
}
