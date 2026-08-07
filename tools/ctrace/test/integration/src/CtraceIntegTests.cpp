/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtraceMain.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <ios>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

struct RunResult {
  int exitCode;
  std::string stderrText;
};

class CtraceIntegTests : public testing::Test {
protected:
  void SetUp() override
  {
    const auto* testInfo = testing::UnitTest::GetInstance()->current_test_info();
    m_workDirectory = std::filesystem::path{CTRACE_TEST_OUTPUT_DIR} / testInfo->name();
    std::error_code error;
    std::filesystem::remove_all(m_workDirectory, error);
    ASSERT_FALSE(error) << error.message();
    ASSERT_TRUE(std::filesystem::create_directories(m_workDirectory)) << m_workDirectory;
  }

  static const std::filesystem::path& testDataDirectory()
  {
    static const std::filesystem::path path{CTRACE_TEST_DATA_DIR};
    return path;
  }

  const std::filesystem::path& workDirectory() const
  {
    return m_workDirectory;
  }

  void copyFixtureFile(const std::filesystem::path& fixtureDirectory, const std::string& fileName) const
  {
    const auto source = fixtureDirectory / fileName;
    ASSERT_TRUE(std::filesystem::is_regular_file(source)) << source;
    std::error_code error;
    const auto copied = std::filesystem::copy_file(source, m_workDirectory / fileName,
                                                   std::filesystem::copy_options::overwrite_existing, error);
    ASSERT_TRUE(copied) << source << ": " << error.message();
  }

  RunResult run(std::vector<std::string> arguments) const
  {
    testing::internal::CaptureStderr();
    const auto exitCode = CtraceMain(arguments);
    return {exitCode, testing::internal::GetCapturedStderr()};
  }

private:
  std::filesystem::path m_workDirectory;
};

void writeFile(const std::filesystem::path& path, const std::string& contents = {})
{
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(output) << path;
  output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  ASSERT_TRUE(output) << path;
}

std::string readTextFile(const std::filesystem::path& path)
{
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("failed to read test file: " + path.string());
  }
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::vector<unsigned char> readBinaryFile(const std::filesystem::path& path)
{
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("failed to read binary test file: " + path.string());
  }
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void expectNonEmptyFile(const std::filesystem::path& path)
{
  ASSERT_TRUE(std::filesystem::is_regular_file(path)) << path;
  EXPECT_GT(std::filesystem::file_size(path), 0U) << path;
}

void expectContains(std::string_view text, std::string_view expected)
{
  EXPECT_NE(std::string_view::npos, text.find(expected)) << expected << "\n" << text;
}

void expectNotContains(std::string_view text, std::string_view unexpected)
{
  EXPECT_EQ(std::string_view::npos, text.find(unexpected)) << unexpected << "\n" << text;
}

TEST_F(CtraceIntegTests, GeneratesAllOutputs)
{
  writeFile(workDirectory() / "Minimal.ctrace-run.yml", R"yml(ctrace-run:
  ctrace-setup:
    - timestamps:
        clock: 400000000
  ctrace-refs: []
)yml");

  const std::string raw{"\0\0\0\0\0\x80\x09\x41", 8U};
  writeFile(workDirectory() / "Minimal.SWO.raw", raw);

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Minimal", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  EXPECT_EQ("cycles,stream,type,source,value,pc,offset,note\n"
            "0,,itm,1,0x41,,,\n",
            readTextFile(workDirectory() / "Minimal.SWO.csv"));
  expectNonEmptyFile(workDirectory() / "Minimal.ctf" / "metadata");
  expectNonEmptyFile(workDirectory() / "Minimal.ctf" / "stream_0");
  expectNonEmptyFile(workDirectory() / "Minimal.SWO.traceanalysis.xml");
}

TEST_F(CtraceIntegTests, RejectsInvalidOptionCombination)
{
  const auto result = run({"ctrace", "--version", "--type", "DWT"});
  EXPECT_EQ(1, result.exitCode);
  expectContains(result.stderrText, "[error] Invalid --type value: DWT");
}

TEST_F(CtraceIntegTests, ReportsMissingTraceDirectory)
{
  const auto missingDirectory = workDirectory() / "missing";
  const auto result = run({"ctrace", missingDirectory.string()});
  EXPECT_EQ(1, result.exitCode);
  expectContains(result.stderrText, "[error] trace directory not found:");
}

TEST_F(CtraceIntegTests, AppliesTraceRunConfiguration)
{
  const auto fixtureDirectory = testDataDirectory() / "trace-run";
  copyFixtureFile(fixtureDirectory, "Board.ctrace-run.yml");
  writeFile(workDirectory() / "Board.SWO.raw");

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Board"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  expectContains(result.stderrText, "aligned DWT range");
  expectContains(result.stderrText, "configured DWT data trace");
  expectContains(result.stderrText, "applied ctrace-run meta");
  expectNotContains(result.stderrText, "configured ITM channel");
}

TEST_F(CtraceIntegTests, GeneratesRequestedOutputsAfterDecoderError)
{
  const auto fixtureDirectory = testDataDirectory() / "trace-run";
  copyFixtureFile(fixtureDirectory, "Minimal.ctrace-run.yml");
  writeFile(workDirectory() / "Minimal.SWO.raw", std::string{"\x01\x41", 2U});

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Minimal", "--all"});
  EXPECT_EQ(1, result.exitCode);
  expectContains(result.stderrText, "[info] decoded 1 events from 2 bytes");

  const auto csvPath = workDirectory() / "Minimal.SWO.csv";
  expectNonEmptyFile(csvPath);
  expectContains(readTextFile(csvPath), ",error,");
  expectNonEmptyFile(workDirectory() / "Minimal.ctf" / "metadata");
  expectNonEmptyFile(workDirectory() / "Minimal.ctf" / "stream_0");
  expectNonEmptyFile(workDirectory() / "Minimal.SWO.traceanalysis.xml");
}

TEST_F(CtraceIntegTests, ConvertsBlinkyFixtureAndSkipsUnsupportedTraceBusInput)
{
  const auto fixtureDirectory = testDataDirectory() / "Blinky+Arm";
  copyFixtureFile(fixtureDirectory, "Blinky+Arm.SWO.raw");
  copyFixtureFile(fixtureDirectory, "Blinky+Arm.TB.raw");
  copyFixtureFile(fixtureDirectory, "Blinky+Arm.ctrace-run.yml");

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Blinky+Arm", "--csv"});
  EXPECT_EQ(1, result.exitCode) << result.stderrText;
  expectContains(result.stderrText, "skipping raw trace channel that is not implemented yet:");
  expectContains(result.stderrText, "channel=TB");
  EXPECT_EQ(readTextFile(fixtureDirectory / "Blinky+Arm.SWO.csv"),
            readTextFile(workDirectory() / "Blinky+Arm.SWO.csv"));
  EXPECT_FALSE(std::filesystem::exists(workDirectory() / "Blinky+Arm.TB.csv"));
  EXPECT_FALSE(std::filesystem::exists(workDirectory() / "Blinky+Arm.TB.traceanalysis.xml"));
  EXPECT_FALSE(std::filesystem::exists(workDirectory() / "Blinky+Arm.TB.ctf"));
}

TEST_F(CtraceIntegTests, RecoversAtHardwareSyncAfterResetDiscontinuity)
{
  const auto fixtureDirectory = testDataDirectory() / "Arm-reset";
  copyFixtureFile(fixtureDirectory, "Arm.SWO.raw");
  copyFixtureFile(fixtureDirectory, "Arm.ctrace-run.yml");

  const auto raw = readBinaryFile(workDirectory() / "Arm.SWO.raw");
  ASSERT_EQ(131071U, raw.size());
  constexpr std::array<unsigned char, 6U> hardwareSync{0U, 0U, 0U, 0U, 0U, 0x80U};
  for (const auto offset : {0U, 128U}) {
    ASSERT_LE(offset + hardwareSync.size(), raw.size());
    EXPECT_TRUE(std::equal(hardwareSync.begin(), hardwareSync.end(), raw.begin() + offset)) << offset;
  }

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Arm", "--csv"});
  EXPECT_EQ(1, result.exitCode) << result.stderrText;
  expectContains(result.stderrText, "[error] invalid ITM packet sequence at raw offset 10");
  expectContains(result.stderrText,
                 "[error] 116 raw bytes from raw offset 12 could not be decoded before the next hardware ITM sync");
  expectContains(result.stderrText, "[info] decoded 52374 events from 131071 bytes");
  expectNotContains(result.stderrText, "OpenCSD made no progress");
  expectNotContains(result.stderrText, "OpenCSD made no decode progress");
  expectNotContains(result.stderrText, "decode aborted");

  const auto csv = readTextFile(workDirectory() / "Arm.SWO.csv");
  expectNotContains(csv, ",itm,");
  const auto recoveryError = csv.find("0,,error,,,,,OpenCSD detected an invalid ITM packet sequence at raw offset 10.");
  const auto dataLoss =
      csv.find("0,,error,,,,,OpenCSD consumed 116 raw bytes while waiting for usable ITM trace packets");
  const auto firstResumedEvent = csv.find("271773258,,dwt,0,0x00,,,");
  const auto lateResumedEvent = csv.find("425766493,,dwt,0,0x2a,,,");
  ASSERT_NE(std::string::npos, recoveryError);
  ASSERT_NE(std::string::npos, dataLoss);
  ASSERT_NE(std::string::npos, firstResumedEvent);
  ASSERT_NE(std::string::npos, lateResumedEvent);
  EXPECT_LT(recoveryError, dataLoss);
  EXPECT_LT(dataLoss, firstResumedEvent);
  EXPECT_LT(firstResumedEvent, lateResumedEvent);
}

} // namespace
