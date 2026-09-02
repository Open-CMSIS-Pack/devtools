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

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

TEST(CtraceUnitTests, testCtfSchemaUsesDenseIdentifiers)
{
  constexpr std::array<std::uint32_t, 6U> eventIds{
      CtfSchema::value(CtfSchema::EventId::Itm),        CtfSchema::value(CtfSchema::EventId::DwtValue),
      CtfSchema::value(CtfSchema::EventId::DwtAddress), CtfSchema::value(CtfSchema::EventId::TraceStatus),
      CtfSchema::value(CtfSchema::EventId::Exception),  CtfSchema::value(CtfSchema::EventId::GlobalTimestamp),
  };
  constexpr std::array<std::uint8_t, 5U> statusReasons{
      CtfSchema::value(CtfSchema::TraceStatusReason::TraceStart),
      CtfSchema::value(CtfSchema::TraceStatusReason::Resync),
      CtfSchema::value(CtfSchema::TraceStatusReason::Overflow),
      CtfSchema::value(CtfSchema::TraceStatusReason::DecodeError),
      CtfSchema::value(CtfSchema::TraceStatusReason::DataLoss),
  };
  constexpr std::array<std::uint8_t, 3U> exceptionActions{
      CtfSchema::value(CtfSchema::ExceptionAction::Entered),
      CtfSchema::value(CtfSchema::ExceptionAction::Exited),
      CtfSchema::value(CtfSchema::ExceptionAction::Returned),
  };
  constexpr std::array<std::uint8_t, 2U> exceptionOrigins{
      CtfSchema::value(CtfSchema::ExceptionOrigin::Trace),
      CtfSchema::value(CtfSchema::ExceptionOrigin::Synthetic),
  };
  constexpr std::array<std::uint8_t, 7U> valueTags{
      CtfSchema::value(CtfSchema::ValueTag::Signed8),  CtfSchema::value(CtfSchema::ValueTag::Unsigned8),
      CtfSchema::value(CtfSchema::ValueTag::Signed16), CtfSchema::value(CtfSchema::ValueTag::Unsigned16),
      CtfSchema::value(CtfSchema::ValueTag::Signed32), CtfSchema::value(CtfSchema::ValueTag::Unsigned32),
      CtfSchema::value(CtfSchema::ValueTag::Float32),
  };

  for (std::size_t index = 0U; index < eventIds.size(); ++index) {
    EXPECT_EQ(eventIds[index], static_cast<std::uint32_t>(index));
  }
  for (std::size_t index = 0U; index < statusReasons.size(); ++index) {
    EXPECT_EQ(statusReasons[index], static_cast<std::uint8_t>(index));
  }
  for (std::size_t index = 0U; index < exceptionActions.size(); ++index) {
    EXPECT_EQ(exceptionActions[index], static_cast<std::uint8_t>(index));
  }
  for (std::size_t index = 0U; index < exceptionOrigins.size(); ++index) {
    EXPECT_EQ(exceptionOrigins[index], static_cast<std::uint8_t>(index));
  }
  for (std::size_t index = 0U; index < valueTags.size(); ++index) {
    EXPECT_EQ(valueTags[index], static_cast<std::uint8_t>(index));
  }
}

TEST(CtraceUnitTests, testCtfValueTypes)
{
  EXPECT_EQ(CtfSchema::eventName(static_cast<CtfSchema::EventId>(255U)), "UNKNOWN");
  /** @brief Describes one supported CTF value representation. */
  struct SupportedType {
    const char* name;
    CtfSchema::ValueTag tag;
    std::uint8_t size;
  };
  const std::vector<SupportedType> supported{
      {"unsigned", CtfSchema::ValueTag::Unsigned8, 1U},  {"unsigned", CtfSchema::ValueTag::Unsigned16, 2U},
      {"unsigned", CtfSchema::ValueTag::Unsigned32, 4U}, {"signed", CtfSchema::ValueTag::Signed8, 1U},
      {"signed", CtfSchema::ValueTag::Signed16, 2U},     {"signed", CtfSchema::ValueTag::Signed32, 4U},
      {"float", CtfSchema::ValueTag::Float32, 4U},
  };
  for (const auto& item : supported) {
    const auto* resolved = CtfSchema::valueVariantForTraceRunType(item.name, item.size);
    ASSERT_TRUE(resolved != nullptr) << std::string("supported CTF data type was rejected: ") + item.name;
    ASSERT_TRUE(resolved->tag == item.tag) << std::string("CTF type tag mismatch: ") + item.name;
    ASSERT_TRUE(resolved->byteSize == item.size) << std::string("CTF type size mismatch: ") + item.name;
  }

  ASSERT_TRUE(CtfSchema::valueVariantForTraceRunType("uint", 4U) == nullptr)
      << "unsupported CTF type alias was accepted";
  ASSERT_TRUE(CtfSchema::valueVariantForTraceRunType("int", 4U) == nullptr)
      << "unsupported signed type alias was accepted";
  ASSERT_TRUE(CtfSchema::valueVariantForTraceRunType("float32", 4U) == nullptr)
      << "unsupported float type alias was accepted";
  ASSERT_TRUE(CtfSchema::valueVariantForTraceRunType("double", 4U) == nullptr) << "unsupported CTF type was accepted";
  ASSERT_TRUE(CtfSchema::valueVariantForTraceRunType("float", 2U) == nullptr)
      << "incompatible CTF float width was accepted";
  ASSERT_TRUE(CtfSchema::valueVariantForTraceRunType("unsigned", 8U) == nullptr)
      << "invalid CTF data size was accepted";
}

TEST(CtraceUnitTests, testCtfExceptionLaneTracker)
{
  CtfExceptionLaneTracker tracker;
  std::vector<std::string> records;
  const auto emit = [&records](ExceptionNumber number, CtfExceptionLaneTracker::RecordAction action,
                               CtfExceptionLaneTracker::RecordOrigin origin) {
    const auto suffix = action == CtfExceptionLaneTracker::RecordAction::Enter
                            ? ":enter"
                            : action == CtfExceptionLaneTracker::RecordAction::Exit ? ":exit" : ":return";
    const auto originSuffix = origin == CtfExceptionLaneTracker::RecordOrigin::Trace ? ":trace" : ":synthetic";
    records.push_back(std::to_string(number) + suffix + originSuffix);
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
  tracker.consume(ExceptionTraceEvent{0, ExceptionAction::Returned}, emit);

  ASSERT_TRUE(records == std::vector<std::string>({
                             "0:enter:synthetic",
                             "0:exit:synthetic",
                             "3:enter:trace",
                             "3:exit:synthetic",
                             "15:enter:trace",
                             "15:exit:trace",
                             "54:enter:trace",
                             "54:exit:trace",
                             "3:return:trace",
                             "3:exit:trace",
                             "0:return:trace",
                         }))
      << "CtfExceptionLaneTracker nested and tail-chain records mismatch";
  ASSERT_TRUE(tracker.observedExceptionNumbers() == std::vector<ExceptionNumber>({0U, 3U, 15U, 54U}))
      << "CtfExceptionLaneTracker observed lanes mismatch";

  tracker.resetForDiscontinuity(emit);
  ASSERT_TRUE(records.back() == "0:exit:synthetic")
      << "CtfExceptionLaneTracker discontinuity must close the active lane synthetically";
  const auto recordCount = records.size();
  tracker.consume(ExceptionTraceEvent{0, ExceptionAction::Unknown}, emit);
  ASSERT_TRUE(records.size() == recordCount) << "CtfExceptionLaneTracker must ignore unknown exception actions";

  CtfExceptionLaneTracker resumedTracker;
  resumedTracker.startThreadMode(emit);
  ASSERT_TRUE(resumedTracker.observedExceptionNumbers() == std::vector<ExceptionNumber>({0U}))
      << "a new CtfExceptionLaneTracker must start with an empty lane history";

  records.clear();
  resumedTracker.consume(ExceptionTraceEvent{3, ExceptionAction::Entered}, emit);
  resumedTracker.consume(ExceptionTraceEvent{15, ExceptionAction::Entered}, emit);
  resumedTracker.consume(ExceptionTraceEvent{15, ExceptionAction::Exited}, emit);
  const auto preemptedRecordCount = records.size();
  resumedTracker.consume(ExceptionTraceEvent{3, ExceptionAction::Exited}, emit);
  ASSERT_TRUE(records.size() == preemptedRecordCount)
      << "CtfExceptionLaneTracker must not exit a preempted context before its return packet";
  resumedTracker.consume(ExceptionTraceEvent{3, ExceptionAction::Returned}, emit);
  resumedTracker.consume(ExceptionTraceEvent{3, ExceptionAction::Exited}, emit);
  resumedTracker.consume(ExceptionTraceEvent{0, ExceptionAction::Returned}, emit);
  ASSERT_TRUE(records.size() == preemptedRecordCount + 3U && records[records.size() - 3U] == "3:return:trace" &&
              records[records.size() - 2U] == "3:exit:trace" && records.back() == "0:return:trace")
      << "CtfExceptionLaneTracker must preserve return and exit the resumed context";

  CtfExceptionLaneTracker returnTracker;
  records.clear();
  returnTracker.consume(ExceptionTraceEvent{3, ExceptionAction::Entered}, emit);
  returnTracker.consume(ExceptionTraceEvent{15, ExceptionAction::Entered}, emit);
  returnTracker.consume(ExceptionTraceEvent{3, ExceptionAction::Returned}, emit);
  EXPECT_EQ(records.back(), "3:return:trace");
}

TEST(CtraceUnitTests, testCtfExceptionLaneTrackerPreservesReturnForActiveContext)
{
  CtfExceptionLaneTracker tracker;
  std::vector<std::string> records;
  const auto emit = [&records](ExceptionNumber number, CtfExceptionLaneTracker::RecordAction action,
                               CtfExceptionLaneTracker::RecordOrigin origin) {
    const auto actionLabel = action == CtfExceptionLaneTracker::RecordAction::Return ? "return" : "unexpected";
    const auto originLabel = origin == CtfExceptionLaneTracker::RecordOrigin::Trace ? "trace" : "synthetic";
    records.push_back(std::to_string(number) + ":" + actionLabel + ":" + originLabel);
  };

  tracker.startThreadMode(emit);
  records.clear();
  tracker.consume(ExceptionTraceEvent{0U, ExceptionAction::Returned}, emit);

  EXPECT_EQ(records, std::vector<std::string>({"0:return:trace"}));
}
