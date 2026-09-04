/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtraceMain.h"
#include "CtfTestSupport.h"

#include <gtest/gtest.h>

#include "ctf/CtfSchema.h"

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

  void copyFixtureFile(const std::filesystem::path& fixtureDirectory, const std::string& fileName,
                       const std::string& destinationFileName = {}) const
  {
    const auto source = fixtureDirectory / fileName;
    ASSERT_TRUE(std::filesystem::is_regular_file(source)) << source;
    const auto destination = destinationFileName.empty() ? fileName : destinationFileName;
    std::error_code error;
    const auto copied = std::filesystem::copy_file(source, m_workDirectory / destination,
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

std::size_t countOccurrences(std::string_view text, std::string_view value)
{
  std::size_t count = 0U;
  for (auto offset = text.find(value); offset != std::string_view::npos;
       offset = text.find(value, offset + value.size())) {
    ++count;
  }
  return count;
}

TEST_F(CtraceIntegTests, GeneratesAllOutputs)
{
  writeFile(workDirectory() / "Minimal.ctrace-run.yml", R"yml(ctrace-run:
  ctrace-setup:
    - timestamps:
        clock: 400000000
  ctrace-refs: []
)yml");

  const std::string raw{"\0\0\0\0\0\x80\x17\x34\x12\x00\x08\x09\x41", 13U};
  writeFile(workDirectory() / "Minimal.SWO.raw", raw);

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Minimal", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  EXPECT_EQ("cycles,stream,type,source,value,pc,offset,note\n"
            "0,,pcsample,,,0x08001234,,\n"
            "0,,itm,1,0x41,,,\n",
            readTextFile(workDirectory() / "Minimal.SWO.csv"));
  expectNonEmptyFile(workDirectory() / "Minimal.ctf" / "metadata");
  expectNonEmptyFile(workDirectory() / "Minimal.ctf" / "stream_0");
  expectNonEmptyFile(workDirectory() / "Minimal.SWO.traceanalysis.xml");
}

TEST_F(CtraceIntegTests, ExpandsDwtEventCountersAcrossCsvAndCtf)
{
  writeFile(workDirectory() / "Events.ctrace-run.yml", R"yml(ctrace-run:
  ctrace-setup:
    - timestamps:
        clock: 400000000
  ctrace-refs: []
)yml");

  const std::string raw{"\0\0\0\0\0\x80\x05\x21\x09\x41", 10U};
  writeFile(workDirectory() / "Events.SWO.raw", raw);

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Events", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  EXPECT_EQ("cycles,stream,type,source,value,pc,offset,note\n"
            "0,,event,0,0x21,,,\n"
            "0,,itm,1,0x41,,,\n",
            readTextFile(workDirectory() / "Events.SWO.csv"));
  expectContains(readTextFile(workDirectory() / "Events.ctf" / "metadata"), "name = \"DWT_EVENT\"");
  expectNonEmptyFile(workDirectory() / "Events.ctf" / "stream_0");
  expectContains(readTextFile(workDirectory() / "Events.SWO.traceanalysis.xml"),
                 "<label value=\"DWT Event Counters\" />");
}

TEST_F(CtraceIntegTests, ConvertsDwtMatchAcrossCsvAndCtf)
{
  const auto fixtureDirectory = testDataDirectory() / "trace-match";
  copyFixtureFile(fixtureDirectory, "trace-match.raw", "trace-match.SWO.raw");
  copyFixtureFile(fixtureDirectory, "trace-match.ctrace-run.yml");

  // This Armv8-M packet stream is completely synthetic and was generated from
  // the architecture specification without a capture from real hardware.
  constexpr std::array<unsigned char, 18U> expectedRaw{{
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x45U, 0x01U, 0x10U,
      0x55U, 0x01U, 0x20U, 0x65U, 0x01U, 0x30U, 0x75U, 0x01U, 0x40U,
  }};
  EXPECT_EQ(readBinaryFile(workDirectory() / "trace-match.SWO.raw"),
            std::vector<unsigned char>(expectedRaw.begin(), expectedRaw.end()));

  const auto result = run({"ctrace", workDirectory().string(), "--target", "trace-match", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  EXPECT_EQ("cycles,stream,type,source,value,pc,offset,note\n"
            "1,,dwt,0,,,,\n"
            "3,,dwt,1,,,,\n"
            "6,,dwt,2,,,,\n"
            "10,,dwt,3,,,,\n",
            readTextFile(workDirectory() / "trace-match.SWO.csv"));

  const auto records = CtfTestSupport::readCtfRecords(workDirectory() / "trace-match.ctf" / "stream_0");
  std::vector<std::uint64_t> matchTimestamps;
  std::vector<std::uint8_t> matchComparators;
  for (const auto& record : records) {
    if (record.id != CtfSchema::value(CtfSchema::EventId::DwtMatch)) {
      continue;
    }
    ASSERT_EQ(record.payload.size(), 6U);
    matchTimestamps.push_back(record.timestamp);
    matchComparators.push_back(record.payload[0U]);
  }
  EXPECT_EQ(matchTimestamps, (std::vector<std::uint64_t>{1U, 3U, 6U, 10U}));
  EXPECT_EQ(matchComparators, (std::vector<std::uint8_t>{0U, 1U, 2U, 3U}));

  const auto metadata = readTextFile(workDirectory() / "trace-match.ctf" / "metadata");
  expectContains(metadata, "name = \"DWT_MATCH\"");
  expectContains(metadata, "\"Match Comparator 3\" = 3");
  const auto xml = readTextFile(workDirectory() / "trace-match.SWO.traceanalysis.xml");
  expectContains(xml, "<label value=\"DWT Match\" />");
  expectContains(xml, "<definedValue name=\"Something happened\" value=\"1\"");
}

TEST_F(CtraceIntegTests, ReconstructsCompressedDwtPacketsAcrossCsvAndCtf)
{
  const auto fixtureDirectory = testDataDirectory() / "trace-compressed-dwt";
  copyFixtureFile(fixtureDirectory, "trace-compressed-dwt.raw", "trace-compressed-dwt.SWO.raw");
  copyFixtureFile(fixtureDirectory, "trace-compressed-dwt.ctrace-run.yml");

  // This Armv8-M stream is completely synthetic. It contains one hardware
  // sync followed by short/medium PC-value packets from an instruction-address
  // range and short/medium data-address packets from a data-address range; no
  // real target capture was used to generate it.
  constexpr std::array<unsigned char, 20U> expectedRaw{{
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x45U, 0x34U, 0x10U, 0x56U,
      0x78U, 0x56U, 0x20U, 0x6dU, 0x56U, 0x30U, 0x7eU, 0x58U, 0x78U, 0x40U,
  }};
  EXPECT_EQ(readBinaryFile(workDirectory() / "trace-compressed-dwt.SWO.raw"),
            std::vector<unsigned char>(expectedRaw.begin(), expectedRaw.end()));

  const auto result = run({"ctrace", workDirectory().string(), "--target", "trace-compressed-dwt", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  EXPECT_EQ("cycles,stream,type,source,value,pc,offset,note\n"
            "1,,dwt,0,,0x08001234,,\n"
            "3,,dwt,1,,0x08015678,,\n"
            "6,,dwt,2,,,0x7856,\n"
            "10,,dwt,3,,,0x7858,\n",
            readTextFile(workDirectory() / "trace-compressed-dwt.SWO.csv"));

  const auto records = CtfTestSupport::readCtfRecords(workDirectory() / "trace-compressed-dwt.ctf" / "stream_0");
  std::vector<CtfTestSupport::CtfRecord> addresses;
  std::copy_if(records.begin(), records.end(), std::back_inserter(addresses),
               [](const auto& record) { return record.id == CtfSchema::value(CtfSchema::EventId::DwtAddress); });
  ASSERT_EQ(addresses.size(), 4U);
  EXPECT_EQ(addresses[0U].timestamp, 1U);
  EXPECT_EQ(addresses[0U].payload[0U], 0U);
  EXPECT_EQ(addresses[0U].payload[1U], 1U);
  EXPECT_EQ(CtfTestSupport::readLe32(addresses[0U].payload, 3U), 0x08001234U);
  EXPECT_EQ(addresses[1U].timestamp, 3U);
  EXPECT_EQ(addresses[1U].payload[0U], 1U);
  EXPECT_EQ(CtfTestSupport::readLe32(addresses[1U].payload, 3U), 0x08015678U);
  EXPECT_EQ(addresses[2U].timestamp, 6U);
  EXPECT_EQ(addresses[2U].payload[0U], 2U);
  EXPECT_EQ(addresses[2U].payload[2U], 1U);
  EXPECT_EQ(CtfTestSupport::readLe16(addresses[2U].payload, 7U), 0x7856U);
  EXPECT_EQ(addresses[3U].timestamp, 10U);
  EXPECT_EQ(addresses[3U].payload[0U], 3U);
  EXPECT_EQ(CtfTestSupport::readLe16(addresses[3U].payload, 7U), 0x7858U);
}

TEST_F(CtraceIntegTests, ConvertsCapturedDwtEventCountersAcrossOverflow)
{
  const auto fixtureDirectory = testDataDirectory() / "trace-event";
  copyFixtureFile(fixtureDirectory, "trace-event.raw", "trace-event.SWO.raw");
  copyFixtureFile(fixtureDirectory, "trace-event.ctrace-run.yml");

  const auto raw = readBinaryFile(workDirectory() / "trace-event.SWO.raw");
  ASSERT_EQ(raw.size(), 19999U);
  constexpr std::array<unsigned char, 6U> hardwareSync{0U, 0U, 0U, 0U, 0U, 0x80U};
  ASSERT_GE(raw.size(), 10005U);
  EXPECT_TRUE(std::equal(hardwareSync.begin(), hardwareSync.end(), raw.begin()));
  EXPECT_EQ(raw[9998U], 0x70U);
  EXPECT_TRUE(std::equal(hardwareSync.begin(), hardwareSync.end(), raw.begin() + 9999U));

  const auto result = run({"ctrace", workDirectory().string(), "--target", "trace-event", "--all"});
  EXPECT_EQ(result.exitCode, 0) << result.stderrText;
  expectContains(result.stderrText, "[warning] first overflow occurred at cycle timestamp 796135");
  expectContains(result.stderrText, "[info] decoded 8599 events from 19999 bytes");

  const auto csv = readTextFile(workDirectory() / "trace-event.SWO.csv");
  EXPECT_EQ(countOccurrences(csv, ",,event,0,"), 5797U);
  EXPECT_EQ(countOccurrences(csv, ",,event,0,0x04,,,"), 3073U);
  EXPECT_EQ(countOccurrences(csv, ",,overflow,"), 1U);
  EXPECT_EQ(countOccurrences(csv, ",,error,"), 0U);
  const auto regularEvent = csv.find("62,,event,0,0x20,,,");
  const auto overflow = csv.find("796135,,overflow,");
  const auto sleepEvent = csv.find("796854,,event,0,0x04,,,");
  ASSERT_NE(regularEvent, std::string::npos);
  ASSERT_NE(overflow, std::string::npos);
  ASSERT_NE(sleepEvent, std::string::npos);
  EXPECT_LT(regularEvent, overflow);
  EXPECT_LT(overflow, sleepEvent);

  const auto ctfRecords = CtfTestSupport::readCtfRecords(workDirectory() / "trace-event.ctf" / "stream_0");
  std::array<std::size_t, 6U> eventCounters{};
  for (const auto& record : ctfRecords) {
    if (record.id != CtfSchema::value(CtfSchema::EventId::DwtEvent)) {
      continue;
    }
    ASSERT_EQ(record.payload.size(), 6U);
    ASSERT_LT(record.payload.front(), eventCounters.size());
    ++eventCounters[record.payload.front()];
  }
  constexpr std::array<std::size_t, 6U> expectedCounters{{1211U, 44U, 3092U, 1011U, 480U, 98U}};
  EXPECT_EQ(eventCounters, expectedCounters);

  expectContains(readTextFile(workDirectory() / "trace-event.ctf" / "metadata"), "name = \"DWT_EVENT\"");
  expectContains(readTextFile(workDirectory() / "trace-event.SWO.traceanalysis.xml"),
                 "<label value=\"DWT Event Counters\" />");
}

TEST_F(CtraceIntegTests, ReportsInvalidDwtEventCounterWithoutPartialDecode)
{
  writeFile(workDirectory() / "InvalidEvent.ctrace-run.yml", R"yml(ctrace-run:
  ctrace-setup:
    - timestamps:
        clock: 400000000
  ctrace-refs: []
)yml");

  const std::string raw{"\0\0\0\0\0\x80\x05\x41\x09\x41", 10U};
  writeFile(workDirectory() / "InvalidEvent.SWO.raw", raw);

  const auto result = run({"ctrace", workDirectory().string(), "--target", "InvalidEvent", "--all"});
  EXPECT_EQ(1, result.exitCode);
  expectContains(result.stderrText, "trace decode error at raw offset 6");
  EXPECT_EQ("cycles,stream,type,source,value,pc,offset,note\n"
            "0,,error,,,,,\"unsupported DWT event-counter payload: size 1, value 0x41; expected a non-zero 1-byte "
            "mask using bits 0..5 only\"\n"
            "0,,itm,1,0x41,,,\n",
            readTextFile(workDirectory() / "InvalidEvent.SWO.csv"));
}

TEST_F(CtraceIntegTests, ExpandsPmuEventCountersAcrossCsvAndCtf)
{
  writeFile(workDirectory() / "Pmu.ctrace-run.yml", R"yml(ctrace-run:
  ctrace-setup:
    - timestamps:
        clock: 400000000
  ctrace-refs: []
)yml");

  const std::string raw{"\0\0\0\0\0\x80\x1d\x81\x09\x41", 10U};
  writeFile(workDirectory() / "Pmu.SWO.raw", raw);

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Pmu", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  EXPECT_EQ("cycles,stream,type,source,value,pc,offset,note\n"
            "0,,pmu,3,0x81,,,\n"
            "0,,itm,1,0x41,,,\n",
            readTextFile(workDirectory() / "Pmu.SWO.csv"));
  expectContains(readTextFile(workDirectory() / "Pmu.ctf" / "metadata"), "name = \"PMU_EVENT\"");
  expectNonEmptyFile(workDirectory() / "Pmu.ctf" / "stream_0");
  expectContains(readTextFile(workDirectory() / "Pmu.SWO.traceanalysis.xml"),
                 "<label value=\"PMU Event Counters\" />");
}

TEST_F(CtraceIntegTests, ReportsInvalidPmuEventCounterWithoutPartialDecode)
{
  writeFile(workDirectory() / "InvalidPmu.ctrace-run.yml", R"yml(ctrace-run:
  ctrace-setup:
    - timestamps:
        clock: 400000000
  ctrace-refs: []
)yml");

  const std::string raw{"\0\0\0\0\0\x80\x1d\x00\x09\x41", 10U};
  writeFile(workDirectory() / "InvalidPmu.SWO.raw", raw);

  const auto result = run({"ctrace", workDirectory().string(), "--target", "InvalidPmu", "--all"});
  EXPECT_EQ(1, result.exitCode);
  expectContains(result.stderrText, "trace decode error at raw offset 6");
  EXPECT_EQ("cycles,stream,type,source,value,pc,offset,note\n"
            "0,,error,,,,,\"unsupported PMU event-counter payload: size 1, value 0x0; expected a non-zero 1-byte "
            "mask using bits 0..7\"\n"
            "0,,itm,1,0x41,,,\n",
            readTextFile(workDirectory() / "InvalidPmu.SWO.csv"));
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
  expectContains(result.stderrText, "configured ITM channel");
  expectContains(result.stderrText, "applied ctrace-run meta");
}

TEST_F(CtraceIntegTests, ReportsDiagnosticsFromConsumedTraceRunReferences)
{
  writeFile(workDirectory() / "Diagnostics.ctrace-run.yml", R"yml(ctrace-run:
  ctrace-setup:
    - pname: core
      timestamps:
        clock: 400000000
  ctrace-refs:
    - ctrace-ref: core/itm
      type: itm
      pname: core
      stream: 1
      source: 0
      info: configured ITM channel zero
      warning: ITM channel zero uses fallback routing
      error: target could not enable ITM channel zero
    - ctrace-ref: core/exceptions
      type: exception
      error: ignored reference diagnostic
)yml");
  writeFile(workDirectory() / "Diagnostics.SWO.raw");

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Diagnostics", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  expectContains(result.stderrText, "[info] configured ITM channel zero:");
  expectContains(result.stderrText, "[warning] ITM channel zero uses fallback routing:");
  expectContains(result.stderrText, "[error] target could not enable ITM channel zero:");
  expectContains(result.stderrText, "ctraceRef=core/itm, type=itm, pname=core");
  expectNotContains(result.stderrText, "ignored reference diagnostic");

  expectNonEmptyFile(workDirectory() / "Diagnostics.SWO.csv");
  expectNonEmptyFile(workDirectory() / "Diagnostics.ctf" / "metadata");
  expectNonEmptyFile(workDirectory() / "Diagnostics.ctf" / "stream_0");
  expectNonEmptyFile(workDirectory() / "Diagnostics.SWO.traceanalysis.xml");
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
