/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.h"

#include <gtest/gtest.h>

#include "ctf/CtfExceptionLaneTracker.h"
#include "ctf/CtfSchema.h"
#include "TraceEvent.h"

#include <cstdint>
#include <string>
#include <vector>

TEST(CtraceUnitTests, testCtfValueTypes)
{
  EXPECT_EQ(CtfSchema::eventName(static_cast<CtfSchema::EventId>(255U)), "UNKNOWN");
  struct SupportedType {
    const char* name;
    CtfSchema::ValueTag tag;
    std::uint8_t size;
  };
  const std::vector<SupportedType> supported{
      {"unsigned int", CtfSchema::ValueTag::Unsigned8, 1U},  {"unsigned int", CtfSchema::ValueTag::Unsigned16, 2U},
      {"unsigned int", CtfSchema::ValueTag::Unsigned32, 4U}, {"signed int", CtfSchema::ValueTag::Signed8, 1U},
      {"signed int", CtfSchema::ValueTag::Signed16, 2U},     {"signed int", CtfSchema::ValueTag::Signed32, 4U},
      {"float", CtfSchema::ValueTag::Float32, 4U},
  };
  for (const auto& item : supported) {
    const auto* resolved = CtfSchema::valueVariantForTraceRunType(item.name, item.size);
    require(resolved != nullptr, std::string("supported CTF data type was rejected: ") + item.name);
    require(resolved->tag == item.tag, std::string("CTF type tag mismatch: ") + item.name);
    require(resolved->byteSize == item.size, std::string("CTF type size mismatch: ") + item.name);
  }

  require(CtfSchema::valueVariantForTraceRunType("uint", 4U) == nullptr, "unsupported CTF type alias was accepted");
  require(CtfSchema::valueVariantForTraceRunType("int", 4U) == nullptr, "unsupported signed type alias was accepted");
  require(CtfSchema::valueVariantForTraceRunType("float32", 4U) == nullptr,
          "unsupported float type alias was accepted");
  require(CtfSchema::valueVariantForTraceRunType("double", 4U) == nullptr, "unsupported CTF type was accepted");
  require(CtfSchema::valueVariantForTraceRunType("float", 2U) == nullptr, "incompatible CTF float width was accepted");
  require(CtfSchema::valueVariantForTraceRunType("unsigned int", 8U) == nullptr, "invalid CTF data size was accepted");
}

TEST(CtraceUnitTests, testCtfExceptionLaneTracker)
{
  CtfExceptionLaneTracker tracker;
  std::vector<std::string> records;
  const auto emit = [&records](std::uint32_t number, CtfExceptionLaneTracker::RecordAction action) {
    records.push_back(std::to_string(number) +
                      (action == CtfExceptionLaneTracker::RecordAction::Enter ? ":enter" : ":exit"));
  };

  tracker.startThreadMode(emit);
  tracker.consume(ExceptionTraceEvent{3, ExceptionAction::Entered}, emit);
  tracker.consume(ExceptionTraceEvent{15, ExceptionAction::Entered}, emit);
  tracker.consume(ExceptionTraceEvent{15, ExceptionAction::Exited}, emit);
  tracker.consume(ExceptionTraceEvent{54, ExceptionAction::Entered}, emit);
  tracker.consume(ExceptionTraceEvent{54, ExceptionAction::Exited}, emit);
  tracker.consume(ExceptionTraceEvent{3, ExceptionAction::Returned}, emit);
  tracker.consume(ExceptionTraceEvent{15, ExceptionAction::Exited}, emit);
  tracker.consume(ExceptionTraceEvent{3, ExceptionAction::Exited}, emit);

  require(records == std::vector<std::string>({
                         "0:enter",
                         "0:exit",
                         "3:enter",
                         "3:exit",
                         "15:enter",
                         "15:exit",
                         "3:enter",
                         "3:exit",
                         "54:enter",
                         "54:exit",
                         "3:enter",
                         "3:exit",
                         "0:enter",
                     }),
          "CtfExceptionLaneTracker nested and tail-chain records mismatch");
  require(tracker.observedExceptionNumbers() == std::vector<std::uint32_t>({0U, 3U, 15U, 54U}),
          "CtfExceptionLaneTracker observed lanes mismatch");

  tracker.resetForDiscontinuity(emit);
  require(records.back() == "0:exit", "CtfExceptionLaneTracker discontinuity must close the active lane");
  const auto recordCount = records.size();
  tracker.consume(ExceptionTraceEvent{0, ExceptionAction::Unknown}, emit);
  require(records.size() == recordCount, "CtfExceptionLaneTracker must ignore unknown exception actions");

  tracker.reset();
  tracker.startThreadMode(emit);
  require(tracker.observedExceptionNumbers() == std::vector<std::uint32_t>({0U}),
          "CtfExceptionLaneTracker reset must clear observed lanes");

  records.clear();
  tracker.consume(ExceptionTraceEvent{3, ExceptionAction::Entered}, emit);
  tracker.consume(ExceptionTraceEvent{15, ExceptionAction::Entered}, emit);
  tracker.consume(ExceptionTraceEvent{15, ExceptionAction::Exited}, emit);
  const auto preemptedRecordCount = records.size();
  tracker.consume(ExceptionTraceEvent{3, ExceptionAction::Exited}, emit);
  require(records.size() == preemptedRecordCount,
          "CtfExceptionLaneTracker must not exit a preempted context before its return packet");
  tracker.consume(ExceptionTraceEvent{3, ExceptionAction::Returned}, emit);
  tracker.consume(ExceptionTraceEvent{3, ExceptionAction::Exited}, emit);
  require(records.size() == preemptedRecordCount + 2U && records[records.size() - 2U] == "3:exit" &&
              records.back() == "0:enter",
          "CtfExceptionLaneTracker must exit a resumed context");

  tracker.reset();
  records.clear();
  tracker.consume(ExceptionTraceEvent{3, ExceptionAction::Entered}, emit);
  tracker.consume(ExceptionTraceEvent{15, ExceptionAction::Entered}, emit);
  tracker.consume(ExceptionTraceEvent{3, ExceptionAction::Returned}, emit);
  EXPECT_EQ(records.back(), "3:enter");
}
