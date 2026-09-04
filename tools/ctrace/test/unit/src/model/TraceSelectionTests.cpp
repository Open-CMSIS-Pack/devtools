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
  EXPECT_TRUE(TraceRunSchema::isDwtDataType("unsigned"));
  EXPECT_TRUE(TraceRunSchema::isDwtDataType("signed"));
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
  ASSERT_TRUE(traceEventSelectedForOutput(itm, TraceSelection{{"itm"}, {}})) << "TraceSelection ITM type mismatch";
  ASSERT_TRUE(!traceEventSelectedForOutput(itm, TraceSelection{{"dwt"}, {}}))
      << "TraceSelection should reject unrelated DWT type";
  ASSERT_TRUE(traceEventSelectedForOutput(itm, TraceSelection{{}, {1U, 2U}})) << "TraceSelection stream-set mismatch";
  ASSERT_TRUE(!traceEventSelectedForOutput(itm, TraceSelection{{}, {2U, 3U}}))
      << "TraceSelection should reject other stream";
  ASSERT_TRUE(traceEventSelectedForOutput(itm, TraceSelection{{"itm"}, {1U}}))
      << "TraceSelection combined criteria mismatch";
  ASSERT_TRUE(!traceEventSelectedForOutput(itm, TraceSelection{{"itm"}, {2U}}))
      << "TraceSelection combined stream mismatch";

  itm.traceBusId = 0U;
  ASSERT_TRUE(traceEventSelectedForOutput(itm, TraceSelection{{"itm"}, {}}))
      << "TraceSelection type selector must retain Trace Bus ID 0 input";
  ASSERT_TRUE(!traceEventSelectedForOutput(itm, TraceSelection{{}, {1U}}))
      << "a non-zero stream selector must not match Trace Bus ID 0 input";
  ASSERT_TRUE(traceEventSelectedForOutput(itm, TraceSelection{{}, {0U}}))
      << "stream selector 0 must match unformatted single-source input";
  std::get<SoftwareTraceEvent>(itm.payload).channel = 0;
  ASSERT_TRUE(!traceEventSelectedForOutput(itm, TraceSelection{}))
      << "TraceSelection must exclude software channel zero without selectors";
  ASSERT_TRUE(!traceEventSelectedForOutput(itm, TraceSelection{{"itm"}, {1U}}))
      << "TraceSelection selectors must not enable software channel zero";

  TraceEvent softwareError = issuePacket(TraceIssueCode::DecodeError);
  ASSERT_TRUE(traceEventSelectedForOutput(softwareError, TraceSelection{}))
      << "TraceSelection must retain decoder errors";

  TraceEvent dwt{DwtDataTraceEvent{2U}};
  ASSERT_TRUE(traceEventSelectedForOutput(dwt, TraceSelection{{"dwt"}, {}})) << "TraceSelection DWT type mismatch";
  ASSERT_TRUE(traceEventSelectedForOutput(dwt, TraceSelection{{"itm", "dwt"}, {}}))
      << "TraceSelection type OR mismatch";

  TraceEvent dwtMatch{DwtMatchTraceEvent{2U}};
  ASSERT_TRUE(traceEventSelectedForOutput(dwtMatch, TraceSelection{{"dwt"}, {}}))
      << "TraceSelection DWT match type mismatch";

  TraceEvent exception{ExceptionTraceEvent{11U, ExceptionAction::Entered}};
  ASSERT_TRUE(traceEventSelectedForOutput(exception, TraceSelection{{"exception"}, {}}))
      << "TraceSelection exception type mismatch";

  TraceEvent pcSample{PcSampleTraceEvent{0x08001234U, false}};
  ASSERT_TRUE(traceEventSelectedForOutput(pcSample, TraceSelection{{"pcsample"}, {}}))
      << "TraceSelection PC-sample type mismatch";
  ASSERT_FALSE(traceEventSelectedForOutput(pcSample, TraceSelection{{"dwt"}, {}}))
      << "TraceSelection must keep PC samples separate from DWT data trace";
}
