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
#include "TraceStreamId.h"
#include "TraceRunConfig.h"

TEST(CtraceUnitTests, testTraceSelection)
{
  EXPECT_EQ(traceEventTypeName(TraceEventType::Count), "unknown");
  EXPECT_EQ(traceEventTypeName(static_cast<TraceEventType>(1000U)), "unknown");
  EXPECT_FALSE(CoreSight::isAtbTraceId(0U));
  EXPECT_TRUE(CoreSight::isAtbTraceId(1U));
  EXPECT_FALSE(CoreSight::isAtbTraceId(112U));
  EXPECT_TRUE(CoreSight::isItmStimulusPort(0U));
  EXPECT_TRUE(CoreSight::isItmStimulusPort(31U));
  EXPECT_FALSE(CoreSight::isItmStimulusPort(32U));
  EXPECT_TRUE(TraceRunSchema::isDwtDataType("unsigned int"));
  EXPECT_TRUE(TraceRunSchema::isDwtDataType("signed int"));
  EXPECT_TRUE(TraceRunSchema::isDwtDataType("float"));
  TraceRunReference timestampReference;
  timestampReference.ctraceRef = "timestamps";
  EXPECT_TRUE(TraceRunSchema::isTimestampReference(timestampReference));
  TraceRunReference itmReference;
  itmReference.type = "itm";
  itmReference.ctraceRef = "itm";
  EXPECT_TRUE(TraceRunSchema::isProcessorItmReference(itmReference));
  TraceEvent itm = softwarePacket(1U);
  itm.traceBusId = 1;
  require(traceEventSelectedForOutput(itm, TraceSelection{{"itm"}, {}}), "TraceSelection ITM type mismatch");
  require(!traceEventSelectedForOutput(itm, TraceSelection{{"dwt"}, {}}),
          "TraceSelection should reject unrelated DWT type");
  require(traceEventSelectedForOutput(itm, TraceSelection{{}, {1U, 2U}}), "TraceSelection stream-set mismatch");
  require(!traceEventSelectedForOutput(itm, TraceSelection{{}, {2U, 3U}}), "TraceSelection should reject other stream");
  require(traceEventSelectedForOutput(itm, TraceSelection{{"itm"}, {1U}}), "TraceSelection combined criteria mismatch");
  require(!traceEventSelectedForOutput(itm, TraceSelection{{"itm"}, {2U}}), "TraceSelection combined stream mismatch");

  itm.traceBusId = 0U;
  require(traceEventSelectedForOutput(itm, TraceSelection{{"itm"}, {}}),
          "TraceSelection type selector must retain Trace Bus ID 0 input");
  require(!traceEventSelectedForOutput(itm, TraceSelection{{}, {1U}}),
          "a non-zero stream selector must not match Trace Bus ID 0 input");
  require(traceEventSelectedForOutput(itm, TraceSelection{{}, {0U}}),
          "stream selector 0 must match unformatted single-source input");
  std::get<SoftwareTraceEvent>(itm.payload).channel = 0;
  require(!traceEventSelectedForOutput(itm, TraceSelection{}),
          "TraceSelection must exclude software channel zero without selectors");
  require(!traceEventSelectedForOutput(itm, TraceSelection{{"itm"}, {1U}}),
          "TraceSelection selectors must not enable software channel zero");

  TraceEvent softwareError = issuePacket(TraceIssueCode::DecodeError);
  require(traceEventSelectedForOutput(softwareError, TraceSelection{}), "TraceSelection must retain decoder errors");

  TraceEvent dwt{DwtDataTraceEvent{2U}};
  require(traceEventSelectedForOutput(dwt, TraceSelection{{"dwt"}, {}}), "TraceSelection DWT type mismatch");
  require(traceEventSelectedForOutput(dwt, TraceSelection{{"itm", "dwt"}, {}}), "TraceSelection type OR mismatch");

  TraceEvent exception{ExceptionTraceEvent{11U, ExceptionAction::Entered}};
  require(traceEventSelectedForOutput(exception, TraceSelection{{"exception"}, {}}),
          "TraceSelection exception type mismatch");
}
