/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestPath.hpp"
#include "TestSupport.hpp"
#include <gtest/gtest.h>
#include "TraceEvent.hpp"
#include "csv/CsvFileOutput.hpp"
#include <filesystem>
#include <system_error>
#include <stdexcept>
#include <string>

TEST(CtraceUnitTests, testCsvFileOutputMatchesSpecification)
{
  const TemporaryTestPath temporaryPath("ctrace-csv-test.csv");
  const auto& csvPath = temporaryPath.path();

  CsvFileOutput output(csvPath);
  output.start();

  TraceEvent dwt{DwtDataTraceEvent{
      2U,
      4U,
      0xfffffdf9U,
      AccessType::Read,
      0xfdf9U,
      0x08001234U,
  }};
  dwt.tcyc = 949338400U;
  output.writeEvent(dwt);

  TraceEvent exception{ExceptionTraceEvent{11U, ExceptionAction::Entered}};
  exception.tcyc = 950364820U;
  output.writeEvent(exception);

  output.stop();

  const auto lines = readTestLines(csvPath);

  require(lines.size() == 3U, "CSV specification row count mismatch");
  require(lines[0] == "cycles,stream,type,source,value,pc,offset,note", "CSV specification header mismatch");
  require(lines[1] == "949338400,0,dwt,2,0xfffffdf9,0x08001234,0xfdf9,", "CSV DWT row schema mismatch");
  require(lines[2] == "950364820,0,exception,11,0x1,,,", "CSV exception state schema mismatch");
}

TEST(CtraceUnitTests, testCsvFileOutputWritesTraceIssues)
{
  const TemporaryTestPath temporaryPath("ctrace-csv-issue-test.csv");
  const auto& csvPath = temporaryPath.path();

  CsvFileOutput output(csvPath);
  output.start();
  output.writeEvent(overflowPacket(1234));

  TraceEvent dataLoss = issuePacket("data-loss", "trace data lost before resynchronization");
  dataLoss.tcyc = 1235;
  output.writeEvent(dataLoss);
  output.stop();

  const auto lines = readTestLines(csvPath);

  require(lines.size() == 3U, "CSV issue row count mismatch");
  require(lines[0] == "cycles,stream,type,source,value,pc,offset,note", "CSV issue header mismatch");
  require(lines[1] == "1234,0,overflow,,,,,overflow: new timestamp segment; time across boundary may be unreliable",
          "CSV overflow issue row mismatch");
  require(lines[2] == "1235,0,error,,,,,trace data lost before resynchronization", "CSV data-loss issue row mismatch");
}

TEST(CtraceUnitTests, testCsvFileOutputWritesDirectly)
{
  const TemporaryTestPath temporaryPath("ctrace-csv-direct-test");
  const auto& testRoot = temporaryPath.path();
  const auto csvPath = testRoot / std::filesystem::u8path(u8"Gr\u00f6\u00dfe.csv");
  writeTestFile(csvPath, "old-output\n");

  CsvFileOutput output(csvPath);
  output.start();
  require(std::filesystem::is_regular_file(csvPath),
          "CSV start must replace the existing target with the direct output file");
  TraceEvent packet = softwarePacket(1U, 1U, 'A');
  output.writeEvent(packet);
  output.abort();
  require(!std::filesystem::exists(csvPath), "CSV abort must remove the incomplete direct output");

  writeTestFile(csvPath, "stale-output\n");
  output.start();
  output.writeEvent(packet);
  output.stop();
  const auto contents = readTestTextFile(csvPath);
  require(contents.find("cycles,stream,type") == 0 && contents.find("stale-output") == std::string::npos,
          "completed CSV output must directly replace the previous file");
  std::filesystem::remove(csvPath);

  std::filesystem::create_directory(csvPath);
  bool rejectedDirectory = false;
  try {
    CsvFileOutput directoryOutput(csvPath);
    directoryOutput.start();
  } catch (const std::runtime_error&) {
    rejectedDirectory = true;
  }
  require(rejectedDirectory && std::filesystem::is_directory(csvPath),
          "CSV output must not delete a directory occupying its target path");
  std::filesystem::remove_all(testRoot);

  std::filesystem::create_directories(testRoot / "working");
  std::filesystem::create_directories(testRoot / "captures");
  const auto parentRelativePath = testRoot / "working" / ".." / "captures" / "output.csv";
  CsvFileOutput parentRelativeOutput(parentRelativePath);
  parentRelativeOutput.start();
  parentRelativeOutput.writeEvent(packet);
  parentRelativeOutput.stop();
  require(std::filesystem::is_regular_file(testRoot / "captures" / "output.csv"),
          "CSV output must accept a legitimate parent-relative trace path");
}

TEST(CtraceUnitTests, testCsvFileOutputReportsIdentityAndIgnoresClosedWrites)
{
  const TemporaryTestPath temporaryPath("ctrace-csv-identity-test.csv");
  CsvFileOutput output(temporaryPath.path(), TraceSelection{{"itm"}, {1U}});
  EXPECT_EQ(output.backendName(), "csv");
  EXPECT_EQ(output.targetPath(), temporaryPath.path().string());

  output.writeEvent(softwarePacket(1U));
  EXPECT_FALSE(std::filesystem::exists(temporaryPath.path()));
  output.start();
  output.writeEvent(softwarePacket(2U));
  output.stop();
  EXPECT_EQ(readTestLines(temporaryPath.path()).size(), 1U);
  output.stop();
  output.abort();
}

TEST(CtraceUnitTests, testCsvFileOutputRejectsUnsafeAndInvalidParents)
{
  for (const auto& path : {std::filesystem::path{}, std::filesystem::path("."), std::filesystem::path("..")}) {
    CsvFileOutput output(path);
    EXPECT_THROW(output.start(), std::invalid_argument);
  }

  const TemporaryTestPath regularParent("ctrace-csv-regular-parent-test");
  writeTestFile(regularParent.path(), "not a directory");
  CsvFileOutput childOfFile(regularParent.path() / "output.csv");
  EXPECT_THROW(childOfFile.start(), std::runtime_error);

  const TemporaryTestPath longNameRoot("ctrace-csv-long-name-test");
  std::filesystem::create_directories(longNameRoot.path());
  CsvFileOutput overlyLongName(longNameRoot.path() / std::string(1024U, 'x'));
  EXPECT_THROW(overlyLongName.start(), std::runtime_error);
}

TEST(CtraceUnitTests, testCsvFileOutputSupportsParentlessPath)
{
  const auto path = std::filesystem::path("ctrace-parentless-output-test.csv");
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
  CsvFileOutput output(path);
  output.start();
  output.writeEvent(softwarePacket(1U));
  output.stop();
  EXPECT_TRUE(std::filesystem::is_regular_file(path));
  std::filesystem::remove(path, ignored);
}

#if defined(__linux__)
TEST(CtraceUnitTests, testCsvFileOutputReportsPseudoFilesystemOpenFailure)
{
  CsvFileOutput output("/proc/ctrace-coverage-output.csv");
  EXPECT_THROW(output.start(), std::runtime_error);
}
#endif
