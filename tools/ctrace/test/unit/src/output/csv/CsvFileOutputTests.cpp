/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestPath.h"
#include "TestSupport.h"
#include <gtest/gtest.h>
#include "TraceEvent.h"
#include "TraceSelection.h"
#include "csv/CsvFileOutput.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <ostream>
#include <streambuf>
#include <stdexcept>
#include <string>
#include <system_error>

/** @brief Selects which operation fails in a synthetic CSV stream. */
enum class CsvStreamFailure {
  Write,
  Flush,
};

/** @brief Implements write and flush failures for CSV lifecycle tests. */
class FailingCsvStreamBuffer final : public std::streambuf {
public:
  /** @brief Creates a buffer that fails at the selected operation. */
  explicit FailingCsvStreamBuffer(CsvStreamFailure failure) : failure_(failure) {}

protected:
  /** @brief Accepts or rejects one output character. */
  int_type overflow(int_type character) override
  {
    return failure_ == CsvStreamFailure::Write ? traits_type::eof() : character;
  }

  /** @brief Accepts or rejects a stream flush. */
  int sync() override
  {
    return failure_ == CsvStreamFailure::Flush ? -1 : 0;
  }

private:
  CsvStreamFailure failure_;
};

/** @brief Provides a CSV stream around a synthetic failing stream buffer. */
class FailingCsvStream final : public CsvFileOutput::Stream {
public:
  /** @brief Creates a stream that fails at the selected operation. */
  explicit FailingCsvStream(CsvStreamFailure failure) : buffer_(failure), stream_(&buffer_)
  {
  }

  /** @brief Returns the synthetic output stream. */
  std::ostream& output() override { return stream_; }

  /** @brief Flushes the synthetic output stream. */
  void close() override { stream_.flush(); }

private:
  FailingCsvStreamBuffer buffer_;
  std::ostream stream_;
};

TEST(CtraceUnitTests, testCsvFileOutputCriteria)
{
  const TemporaryTestPath outputPath("ctrace-filtered-output.csv");
  CsvFileOutput output(outputPath.path(), TraceSelection{{"itm"}, {1U, 2U}});
  output.start();

  const auto accepted = onStream(softwarePacket(1U, 1U, 0x41U), 2U);
  output.writeEvent(accepted);

  for (const auto stream : {3U, 0U}) {
    auto excluded = accepted;
    excluded.traceBusId = static_cast<std::uint8_t>(stream);
    output.writeEvent(excluded);
  }

  auto excludedChannel = accepted;
  std::get<SoftwareTraceEvent>(excludedChannel.payload).channel = 0U;
  output.writeEvent(excludedChannel);

  output.writeEvent(onStream(issuePacket("decode-error"), accepted.traceBusId));
  output.stop();

  require(readTestTextFile(outputPath.path()) == "cycles,stream,type,source,value,pc,offset,note\n,2,itm,1,0x41,,,\n",
          "CsvFileOutput criteria mismatch");

  const TemporaryTestPath errorOutputPath("ctrace-filtered-errors.csv");
  CsvFileOutput errorOutput(errorOutputPath.path(), TraceSelection{{"error"}, {}});
  errorOutput.start();
  errorOutput.writeEvent(issuePacket("decode-warning", "decoder warning", TraceIssueSeverity::Warning));
  errorOutput.stop();
  require(readTestTextFile(errorOutputPath.path()).find(",0,error,,,,,decoder warning\n") != std::string::npos,
          "the error selector must include warning-severity decoder issue packets");
}

TEST(CtraceUnitTests, testCsvFileOutputMatchesSpecification)
{
  const TemporaryTestPath temporaryPath("ctrace-csv-test.csv");
  const auto& csvPath = temporaryPath.path();

  CsvFileOutput output(csvPath);
  output.start();

  output.writeEvent(atCycle(TraceEvent{DwtDataTraceEvent{
                                2U,
                                4U,
                                0xfffffdf9U,
                                AccessType::Read,
                                0xfdf9U,
                                0x08001234U,
                            }},
                            949338400U));
  output.writeEvent(atCycle(TraceEvent{ExceptionTraceEvent{11U, ExceptionAction::Entered}}, 950364820U));

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

  output.writeEvent(atCycle(issuePacket("data-loss", "trace data lost before resynchronization"), 1235U));
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
  const auto rejectedDirectory = throwsException([&] {
    CsvFileOutput directoryOutput(csvPath);
    directoryOutput.start();
  });
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
  longNameRoot.createDirectory();
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

TEST(CtraceUnitTests, testCsvFileOutputReportsInjectedStreamFailures)
{
  const TemporaryTestPath temporaryPath("ctrace-csv-stream-failure-test.csv");

  const auto factory = [](CsvStreamFailure failure) {
    return [failure](const std::filesystem::path&) { return std::make_unique<FailingCsvStream>(failure); };
  };

  CsvFileOutput writeFailure(temporaryPath.path(), {}, factory(CsvStreamFailure::Write));
  EXPECT_THROW(writeFailure.start(), std::runtime_error);

  CsvFileOutput flushFailure(temporaryPath.path(), {}, factory(CsvStreamFailure::Flush));
  flushFailure.start();
  EXPECT_THROW(flushFailure.stop(), std::runtime_error);

  CsvFileOutput openFailure(temporaryPath.path(), {}, [](const std::filesystem::path&) {
    return std::unique_ptr<CsvFileOutput::Stream>{};
  });
  EXPECT_THROW(openFailure.start(), std::runtime_error);

  EXPECT_THROW((void)CsvFileOutput(temporaryPath.path(), {}, {}), std::invalid_argument);
}

#if defined(__linux__)
TEST(CtraceUnitTests, testCsvFileOutputReportsPseudoFilesystemOpenFailure)
{
  CsvFileOutput output("/proc/ctrace-coverage-output.csv");
  EXPECT_THROW(output.start(), std::runtime_error);
}
#endif

#if !defined(_WIN32)
TEST(CtraceUnitTests, testCsvFileOutputReportsPermissionFailures)
{
  const TemporaryTestPath temporaryPath("ctrace-csv-permission-failure-test");
  const auto& root = temporaryPath.createDirectory();
  const auto outputPath = root / "output.csv";

  {
    CsvFileOutput output(outputPath);
    output.start();
    std::filesystem::permissions(root, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
  }
  std::filesystem::permissions(root, std::filesystem::perms::owner_all);
  EXPECT_TRUE(std::filesystem::is_regular_file(outputPath));

  std::filesystem::remove(outputPath);
  std::filesystem::permissions(root, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
  CsvFileOutput createFailure(root / "missing" / "output.csv");
  EXPECT_THROW(createFailure.start(), std::runtime_error);
  std::filesystem::permissions(root, std::filesystem::perms::owner_all);
}
#endif
