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

TEST(CtraceUnitTests, testTraceSelection)
{
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
  require(CsvRowMapper::row(itm) == ",0,itm,1,0x00,,,", "CSV must write Trace Bus ID 0 for unformatted input");

  std::get<SoftwareTraceEvent>(itm.payload).channel = 0;
  require(!traceEventSelectedForOutput(itm, TraceSelection{}),
          "TraceSelection must exclude software channel zero without selectors");
  require(!traceEventSelectedForOutput(itm, TraceSelection{{"itm"}, {1U}}),
          "TraceSelection selectors must not enable software channel zero");

  TraceEvent softwareError = issuePacket("decode-error");
  require(traceEventSelectedForOutput(softwareError, TraceSelection{}), "TraceSelection must retain decoder errors");

  TraceEvent dwt{DwtDataTraceEvent{2U}};
  require(traceEventSelectedForOutput(dwt, TraceSelection{{"dwt"}, {}}), "TraceSelection DWT type mismatch");
  require(traceEventSelectedForOutput(dwt, TraceSelection{{"itm", "dwt"}, {}}), "TraceSelection type OR mismatch");

  TraceEvent exception{ExceptionTraceEvent{11U, ExceptionAction::Entered}};
  require(traceEventSelectedForOutput(exception, TraceSelection{{"exception"}, {}}),
          "TraceSelection exception type mismatch");
}
