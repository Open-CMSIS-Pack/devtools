/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestPath.hpp"
#include "TestSupport.hpp"
#include <gtest/gtest.h>
#include "csv/CsvFileOutput.hpp"
#include "TraceSelection.hpp"
#include <string>

TEST(CtraceUnitTests, testCsvFileOutputCriteria)
{
  const TemporaryTestPath outputPath("ctrace-filtered-output.csv");
  const auto& path = outputPath.path();
  CsvFileOutput output(path, TraceSelection{{"itm"}, {1U, 2U}});
  output.start();

  TraceEvent accepted = softwarePacket(1U, 1U, 0x41U);
  accepted.traceBusId = 2;
  output.writeEvent(accepted);

  TraceEvent wrongStream = accepted;
  wrongStream.traceBusId = 3;
  output.writeEvent(wrongStream);

  TraceEvent unformatted = accepted;
  unformatted.traceBusId = 0U;
  output.writeEvent(unformatted);

  TraceEvent excludedChannel = accepted;
  std::get<SoftwareTraceEvent>(excludedChannel.payload).channel = 0;
  output.writeEvent(excludedChannel);

  TraceEvent error = issuePacket("decode-error");
  error.traceBusId = accepted.traceBusId;
  output.writeEvent(error);

  output.stop();
  const auto contents = readTestTextFile(path);
  require(contents == "cycles,stream,type,source,value,pc,offset,note\n,2,itm,1,0x41,,,\n",
          "CsvFileOutput criteria mismatch");
  const TemporaryTestPath errorOutputPath("ctrace-filtered-errors.csv");
  const auto& errorPath = errorOutputPath.path();
  CsvFileOutput errorOutput(errorPath, TraceSelection{{"error"}, {}});
  errorOutput.start();
  TraceEvent warning = issuePacket("decode-warning", "decoder warning", TraceIssueSeverity::Warning);
  errorOutput.writeEvent(warning);
  errorOutput.stop();
  const auto errorContents = readTestTextFile(errorPath);
  require(errorContents.find(",0,error,,,,,decoder warning\n") != std::string::npos,
          "the error selector must include warning-severity decoder issue packets");
}
