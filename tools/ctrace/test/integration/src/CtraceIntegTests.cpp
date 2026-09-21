/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtraceMain.h"
#include "CtfTestSupport.h"
#include "FormattedTraceTestSupport.h"
#include "TestSupport.h"

#include <gtest/gtest.h>

#include "ctf/CtfSchema.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <initializer_list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
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

void appendBytes(std::vector<std::uint8_t>& destination, const std::vector<std::uint8_t>& source)
{
  destination.insert(destination.end(), source.begin(), source.end());
}

/** @brief Separates input annotations from semantic rows in the generated single-line CSV fixtures. */
std::string traceCsvRows(std::string_view csv)
{
  std::istringstream input{std::string(csv)};
  std::string result;
  for (std::string line; std::getline(input, line);) {
    const auto firstComma = line.find(',');
    const auto secondComma = firstComma == std::string::npos ? firstComma : line.find(',', firstComma + 1U);
    if (secondComma == std::string::npos || line.compare(secondComma + 1U, 5U, "info,") != 0) {
      result += line + "\n";
    }
  }
  return result;
}

/** @brief Builds one deterministic ITM byte stream covering formatted-output packet families. */
std::vector<std::uint8_t> syntheticFormattedRoute(std::uint32_t timestampIncrement, std::uint64_t globalTimestamp,
                                                  std::uint32_t pc, std::uint16_t address)
{
  using namespace FormattedTraceTestSupport;

  auto bytes = itmHardwareSync();
  std::uint8_t channel = 1U;
  for (const auto width : {1U, 2U, 4U}) {
    appendBytes(bytes, itmSoftwarePacket(channel++, static_cast<std::uint8_t>(width), 0U));
  }
  appendBytes(bytes, itmHardwarePacket(16U, 1U, 0U));
  appendBytes(bytes, itmHardwarePacket(18U, 2U, 0U));
  appendBytes(bytes, itmHardwarePacket(20U, 4U, 0U));
  appendBytes(bytes, itmHardwarePacket(14U, 4U, pc));
  appendBytes(bytes, itmHardwarePacket(15U, 2U, address));
  appendBytes(bytes, itmHardwarePacket(14U, 1U, 1U));
  appendBytes(bytes, itmHardwarePacket(2U, 1U, 0U));
  appendBytes(bytes, itmHardwarePacket(0U, 1U, 0x21U));
  appendBytes(bytes, itmHardwarePacket(3U, 1U, 0x81U));
  appendBytes(bytes, itmGlobalTimestampPacket(globalTimestamp));
  appendBytes(bytes, itmLocalTimestampPacket(timestampIncrement));
  appendBytes(bytes, itmOverflowPacket());
  return bytes;
}

constexpr std::uint64_t kAnchoredGlobalTimestamp = 0x1020304c00f23456ULL;
constexpr std::uint64_t kFallbackGlobalTimestamp = 0x2030405000123456ULL;

std::size_t countCsvStreamRows(std::string_view csv, std::string_view stream);

/** @brief Builds the common two-route CoreSight-formatted integration capture. */
std::vector<std::uint8_t> syntheticFormattedCapture()
{
  const auto anchored = syntheticFormattedRoute(240U, kAnchoredGlobalTimestamp, 0x08001000U, 0x1000U);
  const auto fallback = syntheticFormattedRoute(120U, kFallbackGlobalTimestamp, 0x08002000U, 0x2000U);
  return FormattedTraceTestSupport::memoryAlignedFrames({{1U, anchored}, {2U, fallback}});
}

/** @brief Writes one independent test case using the common synthetic formatted capture. */
void writeSyntheticFormattedFixture(const std::filesystem::path& directory, const std::string& traceRun,
                                    std::string_view rawChannel = "TB")
{
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  ASSERT_FALSE(error) << directory << ": " << error.message();
  writeTestFile(directory / "Synthetic.ctrace-run.yml", traceRun);
  const auto raw = syntheticFormattedCapture();
  ASSERT_FALSE(raw.empty());
  ASSERT_EQ(raw.size() % 16U, 0U);
  writeTestFile(directory / ("Synthetic." + std::string(rawChannel) + ".raw"),
            {reinterpret_cast<const char*>(raw.data()), raw.size()});
}

/** @brief Replaces every required occurrence in deterministic fixture text. */
void replaceFixtureText(std::string& text, std::string_view from, std::string_view to)
{
  std::size_t replacements = 0U;
  for (auto position = text.find(from); position != std::string::npos;
       position = text.find(from, position + to.size())) {
    text.replace(position, from.size(), to);
    ++replacements;
  }
  ASSERT_GT(replacements, 0U) << from;
}

void expectSyntheticCsvRoute(std::string_view csv, std::uint8_t stream, std::uint64_t cycles, std::uint32_t pc,
                             std::uint16_t address, std::uint64_t globalTimestamp)
{
  const auto prefix = std::to_string(cycles) + "," + std::to_string(stream) + ",";
  for (const auto& expected : {
           prefix + "itm,1,0x00,,,\n",
           prefix + "itm,2,0x0000,,,\n",
           prefix + "itm,3,0x00000000,,,\n",
           prefix + "dwt,0,0x00,,,\n",
           prefix + "dwt,1,0x0000,,,\n",
           prefix + "dwt,2,0x00000000,,,\n",
           prefix + "pcsample,,,,,\n",
           prefix + "event,0,0x21,,,\n",
           prefix + "pmu,3,0x81,,,\n",
           std::to_string(globalTimestamp) + "," + std::to_string(stream) + ",global_ts,,,,,\n",
       }) {
    expectContains(csv, expected);
  }

  std::ostringstream addressRow;
  addressRow << prefix << "dwt,3,,0x" << std::hex << std::setfill('0') << std::setw(8) << pc << ",0x" << std::setw(4)
             << address << ",\n";
  expectContains(csv, addressRow.str());
  expectContains(csv, prefix + "dwt,3,,,,\n");
  expectContains(csv,
                 prefix + "overflow,,,,,overflow: new timestamp segment; time across boundary may be unreliable\n");
}

void expectSyntheticCtfRoute(const std::filesystem::path& streamPath, std::uint8_t traceBusId,
                             std::uint64_t payloadTimestamp)
{
  const auto records = CtfTestSupport::readCtfRecords(streamPath, CtfStreamWriter::EventContextLayout::RouteLabeled);
  ASSERT_FALSE(records.empty());

  std::array<bool, 10U> eventIds{};
  std::array<bool, 5U> statusReasons{};
  for (const auto& record : records) {
    ASSERT_EQ(record.traceBusId, traceBusId);
    ASSERT_LT(record.id, eventIds.size());
    eventIds[record.id] = true;
    if (record.id == CtfSchema::value(CtfSchema::EventId::TraceStatus)) {
      ASSERT_FALSE(record.payload.empty());
      ASSERT_LT(record.payload.front(), statusReasons.size());
      statusReasons[record.payload.front()] = true;
    }
    if (record.id == CtfSchema::value(CtfSchema::EventId::PcSample)) {
      ASSERT_EQ(record.payload.size(), 6U);
      EXPECT_EQ(record.payload.front(), CtfSchema::value(CtfSchema::PcSampleState::Sleep));
    }
    if (record.id == CtfSchema::value(CtfSchema::EventId::Itm) ||
        record.id == CtfSchema::value(CtfSchema::EventId::DwtValue) ||
        record.id == CtfSchema::value(CtfSchema::EventId::DwtAddress) ||
        record.id == CtfSchema::value(CtfSchema::EventId::DwtMatch) ||
        record.id == CtfSchema::value(CtfSchema::EventId::PcSample) ||
        record.id == CtfSchema::value(CtfSchema::EventId::DwtEvent) ||
        record.id == CtfSchema::value(CtfSchema::EventId::PmuEvent)) {
      EXPECT_EQ(record.timestamp, payloadTimestamp);
    }
  }

  for (const auto id :
       {CtfSchema::EventId::Itm, CtfSchema::EventId::DwtValue, CtfSchema::EventId::DwtAddress,
        CtfSchema::EventId::TraceStatus, CtfSchema::EventId::GlobalTimestamp, CtfSchema::EventId::PcSample,
        CtfSchema::EventId::DwtEvent, CtfSchema::EventId::PmuEvent, CtfSchema::EventId::DwtMatch}) {
    EXPECT_TRUE(eventIds[CtfSchema::value(id)]) << CtfSchema::eventName(id);
  }
  for (const auto reason : {CtfSchema::TraceStatusReason::TraceStart, CtfSchema::TraceStatusReason::Resync,
                            CtfSchema::TraceStatusReason::Overflow}) {
    EXPECT_TRUE(statusReasons[CtfSchema::value(reason)]);
  }
}

/** @brief Verifies the complete semantic CSV content of the common synthetic capture. */
void expectCompleteSyntheticCsv(const std::filesystem::path& csvPath)
{
  const auto csv = readTestTextFile(csvPath);
  expectSyntheticCsvRoute(csv, 1U, 240U, 0x08001000U, 0x1000U, kAnchoredGlobalTimestamp);
  expectSyntheticCsvRoute(csv, 2U, 480U, 0x08002000U, 0x2000U, kFallbackGlobalTimestamp);
  EXPECT_EQ(countCsvStreamRows(csv, "1"), 13U);
  EXPECT_EQ(countCsvStreamRows(csv, "2"), 13U);
  EXPECT_EQ(countCsvStreamRows(csv, "0"), 0U);
  expectContains(csv, ",0,info,,,,,");
  expectContains(csv, " bytes skipped for null source ID 0;");
}

/** @brief Requires all CTF records to use, and to cover, exactly the listed event families. */
void expectOnlyCtfEventIds(const std::filesystem::path& streamPath,
                           std::initializer_list<CtfSchema::EventId> expectedIds)
{
  const auto records = CtfTestSupport::readCtfRecords(streamPath, CtfStreamWriter::EventContextLayout::RouteLabeled);
  ASSERT_FALSE(records.empty());
  for (const auto& record : records) {
    EXPECT_TRUE(std::any_of(expectedIds.begin(), expectedIds.end(), [&](const auto id) {
      return record.id == CtfSchema::value(id);
    })) << record.id;
  }
  for (const auto id : expectedIds) {
    EXPECT_TRUE(std::any_of(records.begin(), records.end(), [&](const auto& record) {
      return record.id == CtfSchema::value(id);
    })) << CtfSchema::eventName(id);
  }
}

/** @brief Verifies the complete output artifact set for the multi-clock synthetic fixture. */
void expectSyntheticArtifacts(const std::filesystem::path& directory, bool csvExpected, bool ctfExpected)
{
  EXPECT_EQ(std::filesystem::is_regular_file(directory / "Synthetic.TB.csv"), csvExpected);
  EXPECT_EQ(std::filesystem::is_directory(directory / "Synthetic.ctf"), ctfExpected);
  EXPECT_FALSE(std::filesystem::exists(directory / "Synthetic.TB.traceanalysis.xml"));
  if (!ctfExpected) {
    return;
  }

  std::vector<std::string> files;
  for (const auto& entry : std::filesystem::directory_iterator(directory / "Synthetic.ctf")) {
    ASSERT_TRUE(entry.is_regular_file()) << entry.path();
    files.push_back(entry.path().filename().string());
  }
  std::sort(files.begin(), files.end());
  EXPECT_EQ(files, (std::vector<std::string>{"metadata", "stream_1", "stream_2"}));
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

std::size_t countCsvStreamRows(std::string_view csv, std::string_view stream)
{
  std::size_t count = 0U;
  auto lineStart = csv.find('\n');
  while (lineStart != std::string_view::npos && lineStart + 1U < csv.size()) {
    ++lineStart;
    const auto lineEnd = csv.find('\n', lineStart);
    const auto firstComma = csv.find(',', lineStart);
    const auto secondComma = firstComma == std::string_view::npos ? firstComma : csv.find(',', firstComma + 1U);
    if (firstComma != std::string_view::npos && secondComma != std::string_view::npos &&
        csv.substr(firstComma + 1U, secondComma - firstComma - 1U) == stream &&
        csv.substr(secondComma + 1U, 5U) != "info,") {
      ++count;
    }
    lineStart = lineEnd;
  }
  return count;
}

std::string normalizeGeneratedTextLineEndings(std::string text, std::string_view artifact)
{
  std::string normalized;
  normalized.reserve(text.size());
  for (std::size_t offset = 0U; offset < text.size(); ++offset) {
    if (text[offset] != '\r') {
      normalized.push_back(text[offset]);
      continue;
    }
    if (offset + 1U >= text.size() || text[offset + 1U] != '\n') {
      throw std::runtime_error(std::string(artifact) + " contains a bare carriage return");
    }
  }
  return normalized;
}

unsigned uuidHexValue(char value)
{
  if (value >= '0' && value <= '9') {
    return static_cast<unsigned>(value - '0');
  }
  if (value >= 'a' && value <= 'f') {
    return static_cast<unsigned>(value - 'a') + 10U;
  }
  if (value >= 'A' && value <= 'F') {
    return static_cast<unsigned>(value - 'A') + 10U;
  }
  throw std::runtime_error("CTF metadata trace UUID is not hexadecimal");
}

std::array<unsigned char, 16U> normalizeCtfMetadataTraceUuid(std::string& metadata)
{
  constexpr std::string_view traceMarker{"trace {"};
  constexpr std::string_view uuidMarker{"    uuid = \""};
  constexpr std::string_view normalizedUuid{"00000000-0000-0000-0000-000000000000"};
  const auto traceStart = metadata.find(traceMarker);
  const auto traceEnd = metadata.find("\n};", traceStart);
  const auto uuidMarkerPosition = metadata.find(uuidMarker, traceStart);
  if (traceStart == std::string::npos || traceEnd == std::string::npos || uuidMarkerPosition == std::string::npos ||
      uuidMarkerPosition >= traceEnd) {
    throw std::runtime_error("CTF metadata trace declaration has no UUID");
  }

  const auto uuidStart = uuidMarkerPosition + uuidMarker.size();
  if (uuidStart + normalizedUuid.size() + 2U > metadata.size() ||
      metadata.compare(uuidStart + normalizedUuid.size(), 2U, "\";") != 0) {
    throw std::runtime_error("CTF metadata trace UUID is not in canonical form");
  }

  std::array<unsigned char, 16U> uuid{};
  std::size_t textOffset = 0U;
  for (std::size_t byte = 0U; byte < uuid.size(); ++byte) {
    if (byte == 4U || byte == 6U || byte == 8U || byte == 10U) {
      if (metadata[uuidStart + textOffset] != '-') {
        throw std::runtime_error("CTF metadata trace UUID is not in canonical form");
      }
      ++textOffset;
    }
    const auto high = uuidHexValue(metadata[uuidStart + textOffset++]);
    const auto low = uuidHexValue(metadata[uuidStart + textOffset++]);
    uuid[byte] = static_cast<unsigned char>((high << 4U) | low);
  }
  if ((uuid[6U] & 0xf0U) != 0x40U || (uuid[8U] & 0xc0U) != 0x80U) {
    throw std::runtime_error("CTF metadata trace UUID is not an RFC 4122 version-4 UUID");
  }

  metadata.replace(uuidStart, normalizedUuid.size(), normalizedUuid.data(), normalizedUuid.size());
  return uuid;
}

void normalizeCtfStreamTraceUuid(std::vector<unsigned char>& stream, const std::array<unsigned char, 16U>& uuid)
{
  constexpr std::size_t uuidOffset = sizeof(std::uint32_t);
  if (stream.empty()) {
    throw std::runtime_error("CTF stream is empty");
  }

  std::size_t packetStart = 0U;
  while (packetStart < stream.size()) {
    if (CtfTestSupport::kCtfEventOffset > stream.size() - packetStart ||
        CtfTestSupport::readLe32(stream, packetStart) != CtfSchema::Magic) {
      throw std::runtime_error("CTF stream has an invalid packet header");
    }

    const auto streamUuid = stream.begin() + static_cast<std::ptrdiff_t>(packetStart + uuidOffset);
    if (!std::equal(uuid.begin(), uuid.end(), streamUuid)) {
      throw std::runtime_error("CTF packet UUID does not match its metadata trace UUID");
    }
    std::fill(streamUuid, streamUuid + static_cast<std::ptrdiff_t>(uuid.size()), 0U);

    const auto packetBits = CtfTestSupport::readLe32(stream, packetStart + CtfTestSupport::kCtfPacketHeaderSize);
    if (packetBits % 8U != 0U) {
      throw std::runtime_error("CTF packet size is not byte-aligned");
    }
    const auto packetBytes = static_cast<std::size_t>(packetBits / 8U);
    if (packetBytes < CtfTestSupport::kCtfEventOffset || packetBytes > stream.size() - packetStart) {
      throw std::runtime_error("CTF packet size exceeds the stream");
    }
    packetStart += packetBytes;
  }
}

template <typename Container>
void expectMatchesGolden(const Container& expected, const Container& actual, std::string_view artifact)
{
  ASSERT_EQ(expected.size(), actual.size()) << artifact << " size differs from golden file";
  const auto mismatch = std::mismatch(expected.begin(), expected.end(), actual.begin());
  if (mismatch.first == expected.end()) {
    return;
  }
  const auto offset = static_cast<std::size_t>(std::distance(expected.begin(), mismatch.first));
  const auto expectedByte = static_cast<unsigned>(static_cast<unsigned char>(*mismatch.first));
  const auto actualByte = static_cast<unsigned>(static_cast<unsigned char>(*mismatch.second));
  ADD_FAILURE() << artifact << " differs from golden file at byte " << offset << ": expected 0x" << std::hex
                << expectedByte << ", actual 0x" << actualByte;
}

TEST_F(CtraceIntegTests, GeneratesAllOutputs)
{
  writeTestFile(workDirectory() / "Minimal.ctrace-run.yml", R"yml(ctrace-run:
  ctrace-setup:
    - timestamps:
        clock: 400000000
  ctrace-refs: []
)yml");

  const std::string raw{"\0\0\0\0\0\x80\x17\x34\x12\x00\x08\x09\x41", 13U};
  writeTestFile(workDirectory() / "Minimal.SWO.raw", raw);

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Minimal", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  EXPECT_EQ("cycles,stream,type,source,value,pc,address,note\n"
            "0,,pcsample,,,0x08001234,,\n"
            "0,,itm,1,0x41,,,\n",
            readTestTextFile(workDirectory() / "Minimal.SWO.csv"));
  expectNonEmptyFile(workDirectory() / "Minimal.ctf" / "metadata");
  expectNonEmptyFile(workDirectory() / "Minimal.ctf" / "stream_0");
  expectNonEmptyFile(workDirectory() / "Minimal.SWO.traceanalysis.xml");
}

TEST_F(CtraceIntegTests, DecodesExplicitUnformattedNamedTraceBuffer)
{
  writeTestFile(workDirectory() / "Named.ctrace-run.yml", R"yml(ctrace-run:
  trace-format: unformatted
  ctrace-setup:
    - timestamps:
        clock: 400000000
  ctrace-refs: []
)yml");

  const std::string raw{"\0\0\0\0\0\x80\x17\x34\x12\x00\x08\x09\x41", 13U};
  writeTestFile(workDirectory() / "Named.TB_MTB.raw", raw);

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Named", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  EXPECT_EQ("cycles,stream,type,source,value,pc,address,note\n"
            "0,,pcsample,,,0x08001234,,\n"
            "0,,itm,1,0x41,,,\n",
            readTestTextFile(workDirectory() / "Named.TB_MTB.csv"));
  expectNonEmptyFile(workDirectory() / "Named.ctf" / "metadata");
  expectNonEmptyFile(workDirectory() / "Named.ctf" / "stream_0");
  expectNonEmptyFile(workDirectory() / "Named.TB_MTB.traceanalysis.xml");
}

TEST_F(CtraceIntegTests, DefaultsNullTraceFormatToUnformattedForSwo)
{
  writeTestFile(workDirectory() / "Minimal.ctrace-run.yml", R"yml(ctrace-run:
  trace-format: null
  ctrace-setup: []
  ctrace-refs: []
)yml");
  const std::string raw{"\0\0\0\0\0\x80\x09\x41", 8U};
  writeTestFile(workDirectory() / "Minimal.SWO.raw", raw);

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Minimal", "--csv"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  EXPECT_EQ("cycles,stream,type,source,value,pc,address,note\n"
            "0,,itm,1,0x41,,,\n",
            readTestTextFile(workDirectory() / "Minimal.SWO.csv"));
}

TEST_F(CtraceIntegTests, ExcludesUnformattedRawSwoWithoutRequiringClock)
{
  writeTestFile(workDirectory() / "Filtered.ctrace-run.yml", R"yml(ctrace-run:
  ctrace-setup:
    - timestamps:
        itm-prescaler: 1
  ctrace-refs: []
)yml");

  const std::string raw{"\0\0\0\0\0\x80\x17\x34\x12\x00\x08\x09\x41", 13U};
  writeTestFile(workDirectory() / "Filtered.SWO.raw", raw);

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Filtered", "--ctf", "--stream", "1"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  expectNotContains(result.stderrText, "timestamps.clock");

  const auto ctfDirectory = workDirectory() / "Filtered.ctf";
  const auto metadata = readTestTextFile(ctfDirectory / "metadata");
  EXPECT_FALSE(metadata.empty());
  expectNotContains(metadata, "\nclock {");
  expectNotContains(metadata, "\nstream {");
  expectNotContains(metadata, "\nevent {");
  EXPECT_FALSE(std::filesystem::exists(ctfDirectory / "stream_0"));
  EXPECT_FALSE(std::filesystem::exists(workDirectory() / "Filtered.SWO.traceanalysis.xml"));
}

TEST_F(CtraceIntegTests, RejectsPartialFormattedFrameBeforeCreatingArtifacts)
{
  constexpr std::string_view configuration = R"yml(  ctrace-setup:
    - pname: core
  ctrace-refs:
    - ctrace-ref: core/itm
      type: itm
      pname: core
      stream: 1
)yml";
  for (const auto& declaration : {
           std::pair<std::string_view, std::string_view>{"explicit", "  trace-format: formatted\n"},
           {"absent", {}}, {"null", "  trace-format: null\n"}}) {
    SCOPED_TRACE(declaration.first);
    const auto directory = workDirectory() / declaration.first;
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    writeTestFile(directory / "Partial.ctrace-run.yml",
                  "ctrace-run:\n" + std::string(declaration.second) + std::string(configuration));
    writeTestFile(directory / "Partial.TB.raw", std::string(15U, 'f'));

    const auto result = run({"ctrace", directory.string(), "--target", "Partial", "--all"});
    EXPECT_EQ(1, result.exitCode);
    expectContains(result.stderrText, "formatted raw trace input size must be a multiple of 16 bytes");
    EXPECT_FALSE(std::filesystem::exists(directory / "Partial.TB.csv"));
    EXPECT_FALSE(std::filesystem::exists(directory / "Partial.ctf"));
    EXPECT_FALSE(std::filesystem::exists(directory / "Partial.TB.traceanalysis.xml"));
  }
}

TEST_F(CtraceIntegTests, SkipsUnsupportedFormattedSourceOnceAndKeepsConfiguredRoute)
{
  writeTestFile(workDirectory() / "Mixed.ctrace-run.yml", R"yml(ctrace-run:
  trace-format: formatted
  ctrace-setup:
    - pname: core
      timestamps:
        clock: 400000000
  ctrace-refs:
    - ctrace-ref: core/itm
      type: itm
      pname: core
      stream: 1
)yml");
  // Memory-aligned formatter frames contain two ID-42 runs between clean ID-1 ITM packets.
  constexpr std::array<unsigned char, 32U> raw{{
      0x03U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x55U, 0x80U, 0xdeU, 0xadU, 0x03U, 0x09U, 0x55U, 0x41U, 0xbeU, 0x48U,
      0x03U, 0xefU, 0x10U, 0x42U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x03U,
  }};
  writeTestFile(workDirectory() / "Mixed.TB.raw",
            {reinterpret_cast<const char*>(raw.data()), static_cast<std::size_t>(raw.size())});

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Mixed", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  EXPECT_EQ(countOccurrences(result.stderrText, "skipping unsupported formatted CoreSight trace source"), 1U);
  expectContains(result.stderrText, "stream=42");
  EXPECT_EQ("cycles,stream,type,source,value,pc,address,note\n"
            "0,1,itm,1,0x41,,,\n"
            "0,1,itm,2,0x42,,,\n",
            traceCsvRows(readTestTextFile(workDirectory() / "Mixed.TB.csv")));
  expectContains(readTestTextFile(workDirectory() / "Mixed.TB.csv"),
                 ",42,info,,,,,4 bytes skipped for unconfigured source ID 42;");
  expectNonEmptyFile(workDirectory() / "Mixed.ctf" / "stream_1");
  EXPECT_FALSE(std::filesystem::exists(workDirectory() / "Mixed.ctf" / "stream_42"));
  expectNonEmptyFile(workDirectory() / "Mixed.TB.traceanalysis.xml");
}

TEST_F(CtraceIntegTests, PublishesOutputsWithUnresolvedFormattedRouteRecovery)
{
  writeTestFile(workDirectory() / "Invalid.ctrace-run.yml", R"yml(ctrace-run:
  trace-format: formatted
  ctrace-setup:
    - pname: core
      timestamps:
        clock: 400000000
  ctrace-refs:
    - ctrace-ref: core/itm
      type: itm
      pname: core
      stream: 1
)yml");
  // ID 1 carries a hardware sync followed by the reserved ITM header 0x04.
  constexpr std::array<unsigned char, 16U> raw{{
      0x03U,
      0x00U,
      0x00U,
      0x00U,
      0x00U,
      0x00U,
      0x80U,
      0x04U,
      0x01U,
      0x00U,
      0x00U,
      0x00U,
      0x00U,
      0x00U,
      0x00U,
      0x00U,
  }};
  writeTestFile(workDirectory() / "Invalid.TB.raw",
            {reinterpret_cast<const char*>(raw.data()), static_cast<std::size_t>(raw.size())});

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Invalid", "--all"});
  EXPECT_EQ(1, result.exitCode);
  expectContains(result.stderrText, "invalid ITM packet header at raw offset 6");
  expectContains(result.stderrText, "ITM decoding did not resume before end of input at raw offset 16");
  expectContains(result.stderrText, "no later hardware SYNC; affected raw interval [6, 16) spans 10 bytes");
  EXPECT_EQ("cycles,stream,type,source,value,pc,address,note\n"
            "0,1,error,,,,,\"OpenCSD detected an invalid ITM packet header at raw offset 6. "
            "0x0014 (OCSD_ERR_INVALID_PCKT_HDR) [Invalid packet header]; packet=RESERVED, size=1 byte, bytes=[04]\"\n"
            "0,1,error,,,,,\"ITM decoding did not resume before end of input at raw offset 16; "
            "no later hardware SYNC; affected raw interval [6, 16) spans 10 bytes; timestamp 0 .. unknown.\"\n",
            traceCsvRows(readTestTextFile(workDirectory() / "Invalid.TB.csv")));
  expectNonEmptyFile(workDirectory() / "Invalid.ctf" / "metadata");
  expectNonEmptyFile(workDirectory() / "Invalid.ctf" / "stream_1");
  expectNonEmptyFile(workDirectory() / "Invalid.TB.traceanalysis.xml");
}

TEST_F(CtraceIntegTests, RecoversOneFormattedRouteWithoutLosingInterleavedOutput)
{
  writeTestFile(workDirectory() / "Recovery.ctrace-run.yml", R"yml(ctrace-run:
  trace-format: formatted
  ctrace-setup:
    - pname: first
      timestamps:
        clock: 400000000
    - pname: second
      timestamps:
        clock: 400000000
  ctrace-refs:
    - ctrace-ref: first/itm
      type: itm
      pname: first
      stream: 1
    - ctrace-ref: second/itm
      type: itm
      pname: second
      stream: 2
)yml");
  // Route 2 has a reserved ITM header in frame 2. Frame 3 starts with
  // continuation data for the same formatter ID; a tree reset would lose it.
  constexpr std::array<unsigned char, 48U> raw{{
      0x03U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x09U, 0x05U, 0x41U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x10U,
      0x80U, 0x04U, 0x00U, 0x58U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x10U, 0x43U, 0x18U, 0x44U, 0x20U, 0xe2U,
      0x44U, 0x29U, 0x03U, 0x46U, 0x10U, 0x42U, 0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x07U,
  }};
  writeTestFile(workDirectory() / "Recovery.TB.raw",
            {reinterpret_cast<const char*>(raw.data()), static_cast<std::size_t>(raw.size())});

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Recovery", "--all"});
  EXPECT_EQ(1, result.exitCode);
  expectContains(result.stderrText, "invalid ITM packet header at raw offset 17");
  const auto csv = readTestTextFile(workDirectory() / "Recovery.TB.csv");
  for (const auto expected : {",1,itm,1,0x41", ",1,itm,2,0x42", ",2,itm,2,0x43", ",2,itm,3,0x44", ",2,itm,4,0x45",
                              ",2,itm,5,0x46", ",2,error"}) {
    expectContains(csv, expected);
  }
  expectNotContains(csv, ",2,itm,0,0x58");
  expectNotContains(csv, ",1,error");
  expectNonEmptyFile(workDirectory() / "Recovery.ctf" / "metadata");
  expectNonEmptyFile(workDirectory() / "Recovery.ctf" / "stream_1");
  expectNonEmptyFile(workDirectory() / "Recovery.ctf" / "stream_2");
  EXPECT_FALSE(std::filesystem::exists(workDirectory() / "Recovery.TB.traceanalysis.xml"));
}

TEST_F(CtraceIntegTests, ReportsUnassignedFormatterOnlyInputWithoutInventingRouteData)
{
  writeTestFile(workDirectory() / "Unassigned.ctrace-run.yml", R"yml(ctrace-run:
  trace-format: formatted
  ctrace-setup:
    - pname: core
      timestamps:
        clock: 400000000
  ctrace-refs:
    - ctrace-ref: core/itm
      type: itm
      pname: core
      stream: 1
)yml");
  writeTestFile(workDirectory() / "Unassigned.TB.raw", std::string(16U, '\0'));

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Unassigned", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  expectContains(result.stderrText, "[info] processed 16 input bytes in ");
  expectContains(result.stderrText, "); trace/diagnostic records: 1\n");
  expectNotContains(result.stderrText, "[info] decoded ");
  expectContains(result.stderrText,
                 "[info] 15 bytes skipped due to missing source ID; first formatter group at raw offset 0");
  expectNotContains(result.stderrText, "no hardware ITM SYNC");
  expectNotContains(result.stderrText, "[error]");
  EXPECT_EQ("cycles,stream,type,source,value,pc,address,note\n"
            ",,info,,,,,15 bytes skipped due to missing source ID; "
            "first formatter group at raw offset 0\n",
            readTestTextFile(workDirectory() / "Unassigned.TB.csv"));
  EXPECT_FALSE(std::filesystem::exists(workDirectory() / "Unassigned.ctf" / "stream_1"));
  EXPECT_FALSE(std::filesystem::exists(workDirectory() / "Unassigned.TB.traceanalysis.xml"));
}

TEST_F(CtraceIntegTests, DecodesFormattedCaptureAfterUnassignedPrefixAndRetainsInfoUnderFilters)
{
  const auto traceRun = readTestTextFile(testDataDirectory() / "formatted-synthetic" / "Synthetic.ctrace-run.yml");
  for (const auto prefixByte : {0x00U, 0xffU}) {
    for (const auto filtered : {false, true}) {
      const auto directory = workDirectory() / (std::to_string(prefixByte) + (filtered ? "-filtered" : "-all"));
      writeSyntheticFormattedFixture(directory, traceRun);
      std::vector<std::uint8_t> raw(32U, static_cast<std::uint8_t>(prefixByte));
      appendBytes(raw, syntheticFormattedCapture());
      writeTestFile(directory / "Synthetic.TB.raw", {reinterpret_cast<const char*>(raw.data()), raw.size()});
      std::vector<std::string> arguments{"ctrace", directory.string(), "--target", "Synthetic",
                                         filtered ? "--csv" : "--all"};
      if (filtered) {
        arguments.insert(arguments.end(), {"--type", "exception", "--stream", "2"});
      }

      const auto result = run(std::move(arguments));
      EXPECT_EQ(0, result.exitCode) << result.stderrText;
      const auto message = std::to_string(prefixByte == 0U ? 30U : 1U) +
                           " bytes skipped due to missing source ID; first formatter group at raw offset 0";
      EXPECT_EQ(countOccurrences(result.stderrText, message), 1U);
      expectNotContains(result.stderrText, "[error]");
      expectNotContains(result.stderrText, "no hardware ITM SYNC");
      const auto csv = readTestTextFile(directory / "Synthetic.TB.csv");
      const auto infoRow = ",,info,,,,," + message + "\n";
      EXPECT_EQ(countOccurrences(csv, infoRow), 1U);
      EXPECT_EQ(countOccurrences(csv, " bytes skipped for null source ID 0;"), 1U);
      EXPECT_EQ(countOccurrences(csv, " bytes skipped for reserved source ID 127;"), prefixByte == 0xffU ? 1U : 0U);
      if (filtered) {
        EXPECT_EQ(traceCsvRows(csv), "cycles,stream,type,source,value,pc,address,note\n");
        EXPECT_EQ(countOccurrences(csv, ",info,"), prefixByte == 0xffU ? 3U : 2U);
      } else {
        expectCompleteSyntheticCsv(directory / "Synthetic.TB.csv");
        expectSyntheticArtifacts(directory, true, true);
      }
    }
  }
}

TEST_F(CtraceIntegTests, ReportsNeverSynchronizedFormattedRouteAndKeepsHealthyOutput)
{
  const auto traceRun = readTestTextFile(testDataDirectory() / "formatted-synthetic" / "Synthetic.ctrace-run.yml");
  writeSyntheticFormattedFixture(workDirectory(), traceRun);
  std::vector<std::uint8_t> raw(16U, 0U);
  appendBytes(raw, FormattedTraceTestSupport::memoryAlignedFrames({
                       {1U, FormattedTraceTestSupport::itmHardwareSync()},
                       {1U, FormattedTraceTestSupport::itmSoftwarePacket(1U, 'A')},
                       {2U, {0x00U, 0x80U, 0x17U, 0xacU, 0x5eU, 0x00U, 0x08U, 0xc0U}},
                   }));
  writeTestFile(workDirectory() / "Synthetic.TB.raw", {reinterpret_cast<const char*>(raw.data()), raw.size()});

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Synthetic", "--all"});
  EXPECT_EQ(1, result.exitCode) << result.stderrText;
  expectContains(result.stderrText, "15 bytes skipped due to missing source ID");
  EXPECT_EQ(countOccurrences(result.stderrText, "no hardware ITM SYNC before end of input"), 1U);
  expectContains(result.stderrText, "8 bytes skipped due to missing SYNC");
  expectContains(result.stderrText, "stream=2");
  const auto csv = readTestTextFile(workDirectory() / "Synthetic.TB.csv");
  expectContains(csv, ",,info,,,,,15 bytes skipped due to missing source ID");
  expectContains(csv, "0,1,itm,1,0x41,,,\n");
  EXPECT_EQ(countOccurrences(csv, "0,2,error,,,,,no hardware ITM SYNC before end of input; "
                                 "first formatter group at raw offset 24\n"),
            1U);
  EXPECT_EQ(countOccurrences(csv, ",2,info,,,,,8 bytes skipped due to missing SYNC; "
                                 "first formatter group at raw offset 24\n"),
            1U);
  expectNotContains(csv, ",1,error,");
  expectNotContains(csv, ",2,itm,");
  expectNotContains(csv, ",sync,");
  expectNonEmptyFile(workDirectory() / "Synthetic.ctf" / "metadata");
  expectNonEmptyFile(workDirectory() / "Synthetic.ctf" / "stream_1");
  expectNonEmptyFile(workDirectory() / "Synthetic.ctf" / "stream_2");
}

TEST_F(CtraceIntegTests, ReportsInitialRoutedByteSkipsAndDecodesAfterRealHardwareSync)
{
  const auto traceRun = readTestTextFile(testDataDirectory() / "formatted-synthetic" / "Synthetic.ctrace-run.yml");
  writeSyntheticFormattedFixture(workDirectory(), traceRun);
  const auto raw = FormattedTraceTestSupport::memoryAlignedFrames({
      {1U, {0x11U, 0x22U, 0x33U}},
      {1U, FormattedTraceTestSupport::itmHardwareSync()},
      {1U, FormattedTraceTestSupport::itmSoftwarePacket(1U, 'A')},
      {2U, FormattedTraceTestSupport::itmHardwareSync()},
      {2U, FormattedTraceTestSupport::itmSoftwarePacket(1U, 'B')},
  });
  writeTestFile(workDirectory() / "Synthetic.TB.raw", {reinterpret_cast<const char*>(raw.data()), raw.size()});

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Synthetic", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  constexpr std::string_view skipped{
      "3 bytes skipped due to missing SYNC; "
      "first formatter group at raw offset 0"};
  EXPECT_EQ(countOccurrences(result.stderrText, skipped), 1U);
  expectContains(result.stderrText, "[info]");
  expectContains(result.stderrText, "stream=1");
  expectNotContains(result.stderrText, "[error]");
  expectNotContains(result.stderrText, "no hardware ITM SYNC");
  const auto csv = readTestTextFile(workDirectory() / "Synthetic.TB.csv");
  EXPECT_EQ(countOccurrences(csv, ",1,info,,,,," + std::string(skipped) + "\n"), 1U);
  EXPECT_EQ(countOccurrences(csv, skipped), 1U);
  expectContains(csv, "0,1,itm,1,0x41,,,\n");
  expectContains(csv, "0,2,itm,1,0x42,,,\n");
  EXPECT_LT(csv.find(skipped), csv.find("0,1,itm,1,0x41,,,\n"));
  expectNotContains(csv, ",1,error,");
  expectNotContains(csv, ",2,error,");
  expectNotContains(csv, ",sync,");
  expectNonEmptyFile(workDirectory() / "Synthetic.ctf" / "metadata");
  expectNonEmptyFile(workDirectory() / "Synthetic.ctf" / "stream_1");
  expectNonEmptyFile(workDirectory() / "Synthetic.ctf" / "stream_2");
}

TEST_F(CtraceIntegTests, ExpandsDwtEventCountersAcrossCsvAndCtf)
{
  writeTestFile(workDirectory() / "Events.ctrace-run.yml", R"yml(ctrace-run:
  ctrace-setup:
    - timestamps:
        clock: 400000000
  ctrace-refs: []
)yml");

  const std::string raw{"\0\0\0\0\0\x80\x05\x21\x09\x41", 10U};
  writeTestFile(workDirectory() / "Events.SWO.raw", raw);

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Events", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  EXPECT_EQ("cycles,stream,type,source,value,pc,address,note\n"
            "0,,event,0,0x21,,,\n"
            "0,,itm,1,0x41,,,\n",
            readTestTextFile(workDirectory() / "Events.SWO.csv"));
  expectContains(readTestTextFile(workDirectory() / "Events.ctf" / "metadata"), "name = \"DWT_EVENT\"");
  expectNonEmptyFile(workDirectory() / "Events.ctf" / "stream_0");
  expectContains(readTestTextFile(workDirectory() / "Events.SWO.traceanalysis.xml"),
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
      0x00U,
      0x00U,
      0x00U,
      0x00U,
      0x00U,
      0x80U,
      0x45U,
      0x01U,
      0x10U,
      0x55U,
      0x01U,
      0x20U,
      0x65U,
      0x01U,
      0x30U,
      0x75U,
      0x01U,
      0x40U,
  }};
  EXPECT_EQ(readTestBinaryFile(workDirectory() / "trace-match.SWO.raw"),
            std::vector<unsigned char>(expectedRaw.begin(), expectedRaw.end()));

  const auto result = run({"ctrace", workDirectory().string(), "--target", "trace-match", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  EXPECT_EQ("cycles,stream,type,source,value,pc,address,note\n"
            "1,,dwt,0,,,,\n"
            "3,,dwt,1,,,,\n"
            "6,,dwt,2,,,,\n"
            "10,,dwt,3,,,,\n",
            readTestTextFile(workDirectory() / "trace-match.SWO.csv"));

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

  const auto metadata = readTestTextFile(workDirectory() / "trace-match.ctf" / "metadata");
  expectContains(metadata, "name = \"DWT_MATCH\"");
  expectContains(metadata, "\"Match Comparator 3\" = 3");
  const auto xml = readTestTextFile(workDirectory() / "trace-match.SWO.traceanalysis.xml");
  expectContains(xml, "<label value=\"DWT Match\" />");
  expectContains(xml, "<definedValue name=\"Something happened\" value=\"1\"");
}

TEST_F(CtraceIntegTests, ConvertsCapturedDwtEventCountersAcrossOverflow)
{
  const auto fixtureDirectory = testDataDirectory() / "trace-event";
  copyFixtureFile(fixtureDirectory, "trace-event.raw", "trace-event.SWO.raw");
  copyFixtureFile(fixtureDirectory, "trace-event.ctrace-run.yml");

  const auto raw = readTestBinaryFile(workDirectory() / "trace-event.SWO.raw");
  ASSERT_EQ(raw.size(), 19999U);
  constexpr std::array<unsigned char, 6U> hardwareSync{0U, 0U, 0U, 0U, 0U, 0x80U};
  ASSERT_GE(raw.size(), 10005U);
  EXPECT_TRUE(std::equal(hardwareSync.begin(), hardwareSync.end(), raw.begin()));
  EXPECT_EQ(raw[9998U], 0x70U);
  EXPECT_TRUE(std::equal(hardwareSync.begin(), hardwareSync.end(), raw.begin() + 9999U));

  const auto result = run({"ctrace", workDirectory().string(), "--target", "trace-event", "--all"});
  EXPECT_EQ(result.exitCode, 0) << result.stderrText;
  expectContains(result.stderrText, "[warning] first overflow occurred at cycle timestamp 796135");
  expectContains(result.stderrText, "[info] processed 19999 input bytes in ");
  expectContains(result.stderrText, "); trace/diagnostic records: 8599\n");

  const auto csv = readTestTextFile(workDirectory() / "trace-event.SWO.csv");
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

  expectContains(readTestTextFile(workDirectory() / "trace-event.ctf" / "metadata"), "name = \"DWT_EVENT\"");
  expectContains(readTestTextFile(workDirectory() / "trace-event.SWO.traceanalysis.xml"),
                 "<label value=\"DWT Event Counters\" />");
}

TEST_F(CtraceIntegTests, ReportsInvalidDwtEventCounterWithoutPartialDecode)
{
  writeTestFile(workDirectory() / "InvalidEvent.ctrace-run.yml", R"yml(ctrace-run:
  ctrace-setup:
    - timestamps:
        clock: 400000000
  ctrace-refs: []
)yml");

  const std::string raw{"\0\0\0\0\0\x80\x05\x41\x09\x41", 10U};
  writeTestFile(workDirectory() / "InvalidEvent.SWO.raw", raw);

  const auto result = run({"ctrace", workDirectory().string(), "--target", "InvalidEvent", "--all"});
  EXPECT_EQ(1, result.exitCode);
  expectContains(result.stderrText, "unsupported DWT event-counter payload: size 1, value 0x41");
  expectContains(result.stderrText, "raw_offset=6");
  EXPECT_EQ("cycles,stream,type,source,value,pc,address,note\n"
            "0,,error,,,,,\"unsupported DWT event-counter payload: size 1, value 0x41; expected a non-zero 1-byte "
            "mask using bits 0..5 only\"\n"
            "0,,itm,1,0x41,,,\n",
            readTestTextFile(workDirectory() / "InvalidEvent.SWO.csv"));
}

TEST_F(CtraceIntegTests, ExpandsPmuEventCountersAcrossCsvAndCtf)
{
  writeTestFile(workDirectory() / "Pmu.ctrace-run.yml", R"yml(ctrace-run:
  ctrace-setup:
    - timestamps:
        clock: 400000000
  ctrace-refs: []
)yml");

  const std::string raw{"\0\0\0\0\0\x80\x1d\x81\x09\x41", 10U};
  writeTestFile(workDirectory() / "Pmu.SWO.raw", raw);

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Pmu", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  EXPECT_EQ("cycles,stream,type,source,value,pc,address,note\n"
            "0,,pmu,3,0x81,,,\n"
            "0,,itm,1,0x41,,,\n",
            readTestTextFile(workDirectory() / "Pmu.SWO.csv"));
  expectContains(readTestTextFile(workDirectory() / "Pmu.ctf" / "metadata"), "name = \"PMU_EVENT\"");
  expectNonEmptyFile(workDirectory() / "Pmu.ctf" / "stream_0");
  expectContains(readTestTextFile(workDirectory() / "Pmu.SWO.traceanalysis.xml"),
                 "<label value=\"PMU Event Counters\" />");
}

TEST_F(CtraceIntegTests, ReportsInvalidPmuEventCounterWithoutPartialDecode)
{
  writeTestFile(workDirectory() / "InvalidPmu.ctrace-run.yml", R"yml(ctrace-run:
  ctrace-setup:
    - timestamps:
        clock: 400000000
  ctrace-refs: []
)yml");

  const std::string raw{"\0\0\0\0\0\x80\x1d\x00\x09\x41", 10U};
  writeTestFile(workDirectory() / "InvalidPmu.SWO.raw", raw);

  const auto result = run({"ctrace", workDirectory().string(), "--target", "InvalidPmu", "--all"});
  EXPECT_EQ(1, result.exitCode);
  expectContains(result.stderrText, "unsupported PMU event-counter payload: size 1, value 0x0");
  expectContains(result.stderrText, "raw_offset=6");
  EXPECT_EQ("cycles,stream,type,source,value,pc,address,note\n"
            "0,,error,,,,,\"unsupported PMU event-counter payload: size 1, value 0x0; expected a non-zero 1-byte "
            "mask using bits 0..7\"\n"
            "0,,itm,1,0x41,,,\n",
            readTestTextFile(workDirectory() / "InvalidPmu.SWO.csv"));
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
  writeTestFile(workDirectory() / "Board.SWO.raw", {});

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Board"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  expectContains(result.stderrText, "aligned DWT range");
  expectContains(result.stderrText, "configured DWT data trace");
  expectContains(result.stderrText, "configured ITM channel");
  expectContains(result.stderrText, "applied ctrace-run meta");
}

TEST_F(CtraceIntegTests, ReportsDiagnosticsFromConsumedTraceRunReferences)
{
  writeTestFile(workDirectory() / "Diagnostics.ctrace-run.yml", R"yml(ctrace-run:
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
      info:
        - configured ITM channel zero
        - retained secondary setup information
      warning: ITM channel zero uses fallback routing
      error:
        - target could not enable ITM channel zero
        - target rejected the fallback configuration
    - ctrace-ref: core/exceptions
      type: exception
      error: ignored reference diagnostic
)yml");
  writeTestFile(workDirectory() / "Diagnostics.SWO.raw", {});

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Diagnostics", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  expectContains(result.stderrText, "[info] configured ITM channel zero:");
  expectContains(result.stderrText, "[info] retained secondary setup information:");
  expectContains(result.stderrText, "[warning] ITM channel zero uses fallback routing:");
  expectContains(result.stderrText, "[error] target could not enable ITM channel zero:");
  expectContains(result.stderrText, "[error] target rejected the fallback configuration:");
  expectContains(result.stderrText, "ctraceRef=core/itm, type=itm, pname=core");
  expectContains(result.stderrText, "[error] ignored reference diagnostic:");
  expectContains(result.stderrText, "ctraceRef=core/exceptions, type=exception");

  expectNonEmptyFile(workDirectory() / "Diagnostics.SWO.csv");
  expectNonEmptyFile(workDirectory() / "Diagnostics.ctf" / "metadata");
  expectNonEmptyFile(workDirectory() / "Diagnostics.ctf" / "stream_0");
  expectNonEmptyFile(workDirectory() / "Diagnostics.SWO.traceanalysis.xml");
}

TEST_F(CtraceIntegTests, GeneratesRequestedOutputsAfterDecoderError)
{
  const auto fixtureDirectory = testDataDirectory() / "trace-run";
  copyFixtureFile(fixtureDirectory, "Minimal.ctrace-run.yml");
  writeTestFile(workDirectory() / "Minimal.SWO.raw", std::string{"\x01\x41", 2U});

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Minimal", "--all"});
  EXPECT_EQ(1, result.exitCode);
  expectContains(result.stderrText, "[info] processed 2 input bytes in ");
  expectContains(result.stderrText, "); trace/diagnostic records: 1\n");
  expectNotContains(result.stderrText, "[info] decoded ");

  const auto csvPath = workDirectory() / "Minimal.SWO.csv";
  expectNonEmptyFile(csvPath);
  expectContains(readTestTextFile(csvPath), ",error,");
  expectNonEmptyFile(workDirectory() / "Minimal.ctf" / "metadata");
  expectNonEmptyFile(workDirectory() / "Minimal.ctf" / "stream_0");
  expectNonEmptyFile(workDirectory() / "Minimal.SWO.traceanalysis.xml");
}

TEST_F(CtraceIntegTests, ConvertsLegacyUnformattedSwoFixtureToGoldenOutputs)
{
  const auto fixtureDirectory = testDataDirectory() / "Blinky+Arm";
  copyFixtureFile(fixtureDirectory, "Blinky+Arm.SWO.raw");

  // The legacy pyTS configuration predates timestamps.clock. CTF requires it, and the captured CM7 ran at 480 MHz.
  auto traceRun = readTestTextFile(fixtureDirectory / "Blinky+Arm.ctrace-run.yml");
  constexpr std::string_view legacyTimestampBlock{"    timestamps:\n      itm-prescaler: 1\n  - pname: CM4"};
  constexpr std::string_view ctfTimestampBlock{
      "    timestamps:\n      clock: 480000000\n      itm-prescaler: 1\n  - pname: CM4"};
  const auto timestampPosition = traceRun.find(legacyTimestampBlock);
  ASSERT_NE(std::string::npos, timestampPosition);
  ASSERT_EQ(std::string::npos, traceRun.find(legacyTimestampBlock, timestampPosition + 1U));
  traceRun.replace(timestampPosition, legacyTimestampBlock.size(), ctfTimestampBlock.data(), ctfTimestampBlock.size());
  writeTestFile(workDirectory() / "Blinky+Arm.ctrace-run.yml", traceRun);

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Blinky+Arm", "--all"});
  EXPECT_EQ(1, result.exitCode) << result.stderrText;
  EXPECT_EQ(readTestTextFile(fixtureDirectory / "Blinky+Arm.SWO.csv"),
            readTestTextFile(workDirectory() / "Blinky+Arm.SWO.csv"));

  const auto goldenDirectory = fixtureDirectory / "expected";
  auto metadata = normalizeGeneratedTextLineEndings(
      readTestTextFile(workDirectory() / "Blinky+Arm.ctf" / "metadata"), "CTF metadata");
  const auto traceUuid = normalizeCtfMetadataTraceUuid(metadata);
  auto stream = readTestBinaryFile(workDirectory() / "Blinky+Arm.ctf" / "stream_0");
  normalizeCtfStreamTraceUuid(stream, traceUuid);
  expectMatchesGolden(readTestTextFile(goldenDirectory / "Blinky+Arm.ctf" / "metadata"), metadata, "CTF metadata");
  expectMatchesGolden(readTestBinaryFile(goldenDirectory / "Blinky+Arm.ctf" / "stream_0"), stream, "CTF binary stream");
  expectMatchesGolden(readTestTextFile(goldenDirectory / "Blinky+Arm.SWO.traceanalysis.xml"),
                      normalizeGeneratedTextLineEndings(
                          readTestTextFile(workDirectory() / "Blinky+Arm.SWO.traceanalysis.xml"), "Trace Compass XML"),
                      "Trace Compass XML");

  EXPECT_FALSE(std::filesystem::exists(workDirectory() / "Blinky+Arm.TB.csv"));
  EXPECT_FALSE(std::filesystem::exists(workDirectory() / "Blinky+Arm.TB.traceanalysis.xml"));
  EXPECT_FALSE(std::filesystem::exists(workDirectory() / "Blinky+Arm.TB.ctf"));
}

TEST_F(CtraceIntegTests, RejectsAmbiguousRawInputsWithoutAnExplicitFormat)
{
  const auto original = readTestTextFile(testDataDirectory() / "formatted-synthetic" / "Synthetic.ctrace-run.yml");
  for (const auto nullValue : {false, true}) {
    auto traceRun = original;
    replaceFixtureText(traceRun, "  trace-format: formatted\n", nullValue ? "  trace-format: null\n" : "");
    for (const auto otherChannel : {"SWO", "TB_ETB"}) {
      const auto directory = workDirectory() / (nullValue ? "null" : "absent") / otherChannel;
      SCOPED_TRACE(directory.string());
      writeSyntheticFormattedFixture(directory, traceRun);
      writeTestFile(directory / ("Synthetic." + std::string(otherChannel) + ".raw"), {});

      const auto result = run({"ctrace", directory.string(), "--target", "Synthetic", "--all"});
      EXPECT_EQ(1, result.exitCode);
      expectContains(result.stderrText, "multiple eligible raw trace inputs found for solution-set Synthetic:");
      expectContains(result.stderrText, "Synthetic.TB.raw");
      expectContains(result.stderrText, "Synthetic." + std::string(otherChannel) + ".raw");
      expectSyntheticArtifacts(directory, false, false);
      EXPECT_FALSE(std::filesystem::exists(directory / ("Synthetic." + std::string(otherChannel) + ".csv")));
    }
  }
}

TEST_F(CtraceIntegTests, ConvertsReconstructedFormattedTraceBusFixture)
{
  const auto fixtureDirectory = testDataDirectory() / "TB-Trace";
  copyFixtureFile(fixtureDirectory, "Blinky+Arm.ctrace-run.yml");
  copyFixtureFile(fixtureDirectory, "Blinky+Arm.TB.raw");

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Blinky+Arm", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;

  const auto csv = readTestTextFile(workDirectory() / "Blinky+Arm.TB.csv");
  EXPECT_EQ(countOccurrences(traceCsvRows(csv), "\n"), 526U);
  EXPECT_EQ(countCsvStreamRows(csv, "1"), 213U);
  EXPECT_EQ(countCsvStreamRows(csv, "2"), 312U);
  EXPECT_EQ(countCsvStreamRows(csv, "0"), 0U);

  const auto ctfDirectory = workDirectory() / "Blinky+Arm.ctf";
  const auto stream1 =
      CtfTestSupport::readCtfRecords(ctfDirectory / "stream_1", CtfStreamWriter::EventContextLayout::RouteLabeled);
  const auto stream2 =
      CtfTestSupport::readCtfRecords(ctfDirectory / "stream_2", CtfStreamWriter::EventContextLayout::RouteLabeled);
  ASSERT_FALSE(stream1.empty());
  ASSERT_FALSE(stream2.empty());
  EXPECT_TRUE(std::all_of(stream1.begin(), stream1.end(), [](const auto& record) { return record.traceBusId == 1U; }));
  EXPECT_TRUE(std::all_of(stream2.begin(), stream2.end(), [](const auto& record) { return record.traceBusId == 2U; }));
  EXPECT_FALSE(std::filesystem::exists(ctfDirectory / "stream_0"));
  EXPECT_EQ(countOccurrences(readTestTextFile(ctfDirectory / "metadata"), "clock {"), 2U);

  EXPECT_FALSE(std::filesystem::exists(workDirectory() / "Blinky+Arm.TB.traceanalysis.xml"));
  EXPECT_EQ(countOccurrences(result.stderrText,
                             "Trace Compass XML was not generated because emitted CTF streams use multiple clock "
                             "domains"),
            1U);
}

TEST_F(CtraceIntegTests, DecodesDeterministicSyntheticFormattedPacketFamiliesOnAnchorAndFallbackRoutes)
{
  const auto fixtureDirectory = testDataDirectory() / "formatted-synthetic";
  writeSyntheticFormattedFixture(workDirectory(), readTestTextFile(fixtureDirectory / "Synthetic.ctrace-run.yml"));

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Synthetic", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  expectNotContains(result.stderrText, "decode error");

  expectCompleteSyntheticCsv(workDirectory() / "Synthetic.TB.csv");

  const auto ctfDirectory = workDirectory() / "Synthetic.ctf";
  expectSyntheticCtfRoute(ctfDirectory / "stream_1", 1U, 240U);
  expectSyntheticCtfRoute(ctfDirectory / "stream_2", 2U, 480U);
  EXPECT_FALSE(std::filesystem::exists(ctfDirectory / "stream_0"));
  const auto metadata = readTestTextFile(ctfDirectory / "metadata");
  expectContains(metadata, "freq = 240000000;");
  expectContains(metadata, "freq = 480000000;");
  EXPECT_FALSE(std::filesystem::exists(workDirectory() / "Synthetic.TB.traceanalysis.xml"));
  EXPECT_EQ(countOccurrences(result.stderrText,
                             "Trace Compass XML was not generated because emitted CTF streams use multiple clock "
                             "domains"),
            1U);
}

TEST_F(CtraceIntegTests, DefaultsAbsentAndNullTraceFormatToFormattedForTraceBuffers)
{
  const auto original = readTestTextFile(testDataDirectory() / "formatted-synthetic" / "Synthetic.ctrace-run.yml");
  for (const auto nullValue : {false, true}) {
    auto traceRun = original;
    replaceFixtureText(traceRun, "  trace-format: formatted\n", nullValue ? "  trace-format: null\n" : "");
    for (const auto channel : {"TB", "TB_ETB"}) {
      const auto directory = workDirectory() / (nullValue ? "null" : "absent") / channel;
      SCOPED_TRACE(directory.string());
      writeSyntheticFormattedFixture(directory, traceRun, channel);
      writeTestFile(directory / "Synthetic.ER.raw", {});

      const auto result = run({"ctrace", directory.string(), "--target", "Synthetic", "--all"});
      ASSERT_EQ(0, result.exitCode) << result.stderrText;
      expectCompleteSyntheticCsv(directory / ("Synthetic." + std::string(channel) + ".csv"));
      expectSyntheticCtfRoute(directory / "Synthetic.ctf" / "stream_1", 1U, 240U);
      expectSyntheticCtfRoute(directory / "Synthetic.ctf" / "stream_2", 2U, 480U);
      EXPECT_FALSE(std::filesystem::exists(directory / "Synthetic.ctf" / "stream_0"));
      EXPECT_FALSE(std::filesystem::exists(directory / "Synthetic.ER.csv"));
    }
  }
}

TEST_F(CtraceIntegTests, RequiresTraceBusIdWhenTraceBufferFormatIsInferred)
{
  writeSyntheticFormattedFixture(workDirectory(), R"yml(ctrace-run:
  ctrace-setup:
    - pname: core
  ctrace-refs:
    - ctrace-ref: core/itm
      type: itm
      pname: core
)yml");

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Synthetic", "--all"});
  EXPECT_EQ(1, result.exitCode);
  expectContains(result.stderrText, "processor ITM route anchor requires a CoreSight Trace Bus ID");
  expectSyntheticArtifacts(workDirectory(), false, false);
}

TEST_F(CtraceIntegTests, SupportsFormattedCheckCsvCtfAndAllArtifactMatrix)
{
  struct Mode {
    std::string_view name;
    std::string_view option;
    bool csv;
    bool ctf;
  };
  constexpr std::array<Mode, 4U> modes{{
      {"check", {}, false, false},
      {"csv", "--csv", true, false},
      {"ctf", "--ctf", false, true},
      {"all", "--all", true, true},
  }};
  const auto traceRun = readTestTextFile(testDataDirectory() / "formatted-synthetic" / "Synthetic.ctrace-run.yml");

  for (const auto& mode : modes) {
    const auto directory = workDirectory() / mode.name;
    writeSyntheticFormattedFixture(directory, traceRun);
    std::vector<std::string> arguments{"ctrace", directory.string(), "--target", "Synthetic"};
    if (!mode.option.empty()) {
      arguments.emplace_back(mode.option);
    }

    const auto result = run(std::move(arguments));
    EXPECT_EQ(0, result.exitCode) << mode.name << ": " << result.stderrText;
    expectSyntheticArtifacts(directory, mode.csv, mode.ctf);
    if (mode.csv) {
      expectCompleteSyntheticCsv(directory / "Synthetic.TB.csv");
    }
  }
}

TEST_F(CtraceIntegTests, AppliesMultiValueTypeAndStreamUnionsAndTheirIntersection)
{
  const auto traceRun = readTestTextFile(testDataDirectory() / "formatted-synthetic" / "Synthetic.ctrace-run.yml");
  const auto unionDirectory = workDirectory() / "union";
  writeSyntheticFormattedFixture(unionDirectory, traceRun);

  auto result = run({"ctrace", unionDirectory.string(), "--target", "Synthetic", "--all", "--type", "itm", "pmu",
                     "--stream", "1", "2"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  expectSyntheticArtifacts(unionDirectory, true, true);
  const auto unionCsv = readTestTextFile(unionDirectory / "Synthetic.TB.csv");
  EXPECT_EQ(countOccurrences(unionCsv, ",itm,"), 6U);
  EXPECT_EQ(countOccurrences(unionCsv, ",pmu,"), 2U);
  EXPECT_EQ(countCsvStreamRows(unionCsv, "1"), 4U);
  EXPECT_EQ(countCsvStreamRows(unionCsv, "2"), 4U);
  for (const auto unexpected : {",dwt,", ",event,", ",pcsample,", ",global_ts,", ",overflow,"}) {
    expectNotContains(unionCsv, unexpected);
  }
  expectOnlyCtfEventIds(unionDirectory / "Synthetic.ctf" / "stream_1",
                        {CtfSchema::EventId::Itm, CtfSchema::EventId::PmuEvent});
  expectOnlyCtfEventIds(unionDirectory / "Synthetic.ctf" / "stream_2",
                        {CtfSchema::EventId::Itm, CtfSchema::EventId::PmuEvent});

  const auto intersectionDirectory = workDirectory() / "intersection";
  writeSyntheticFormattedFixture(intersectionDirectory, traceRun);
  result = run({"ctrace", intersectionDirectory.string(), "--target", "Synthetic", "--all", "--type", "dwt", "pmu",
                "--stream", "2"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  const auto intersectionCsv = readTestTextFile(intersectionDirectory / "Synthetic.TB.csv");
  EXPECT_EQ(countCsvStreamRows(intersectionCsv, "1"), 0U);
  EXPECT_EQ(countCsvStreamRows(intersectionCsv, "2"), 6U);
  EXPECT_EQ(countOccurrences(intersectionCsv, ",dwt,"), 5U);
  EXPECT_EQ(countOccurrences(intersectionCsv, ",pmu,"), 1U);
  expectOnlyCtfEventIds(intersectionDirectory / "Synthetic.ctf" / "stream_2",
                        {CtfSchema::EventId::DwtValue, CtfSchema::EventId::DwtAddress, CtfSchema::EventId::DwtMatch,
                         CtfSchema::EventId::PmuEvent});
  EXPECT_FALSE(std::filesystem::exists(intersectionDirectory / "Synthetic.ctf" / "stream_1"));
  expectNonEmptyFile(intersectionDirectory / "Synthetic.TB.traceanalysis.xml");
}

TEST_F(CtraceIntegTests, DefersAbsentAndNullFormattedClocksToCtfOutputValidation)
{
  struct Mode {
    std::string_view name;
    std::string_view option;
    int exitCode;
    bool csv;
  };
  constexpr std::array<Mode, 4U> modes{{
      {"check", {}, 0, false},
      {"csv", "--csv", 0, true},
      {"ctf", "--ctf", 1, false},
      {"all", "--all", 1, true},
  }};
  const auto original = readTestTextFile(testDataDirectory() / "formatted-synthetic" / "Synthetic.ctrace-run.yml");

  for (const auto nullValue : {false, true}) {
    auto traceRun = original;
    const auto replacement = nullValue ? std::string("        clock: null\n") : std::string{};
    replaceFixtureText(traceRun, "        clock: 240000000\n", replacement);
    replaceFixtureText(traceRun, "        clock: 480000000\n", replacement);
    const auto variant = nullValue ? std::string("null") : std::string("absent");

    for (const auto& mode : modes) {
      const auto directory = workDirectory() / (variant + "-" + std::string(mode.name));
      writeSyntheticFormattedFixture(directory, traceRun);
      std::vector<std::string> arguments{"ctrace", directory.string(), "--target", "Synthetic"};
      if (!mode.option.empty()) {
        arguments.emplace_back(mode.option);
      }

      const auto result = run(std::move(arguments));
      EXPECT_EQ(mode.exitCode, result.exitCode) << variant << "/" << mode.name << ": " << result.stderrText;
      expectSyntheticArtifacts(directory, mode.csv, false);
      if (mode.exitCode != 0) {
        expectContains(result.stderrText, "timestamps.clock");
      }
      if (mode.csv) {
        expectCompleteSyntheticCsv(directory / "Synthetic.TB.csv");
      }
    }
  }
}

TEST_F(CtraceIntegTests, RejectsMissingAndInvalidFormattedRouteFallbacksBeforeOutputs)
{
  constexpr std::string_view missingRoute = R"yml(ctrace-run:
  trace-format: formatted
  ctrace-setup:
    - pname: core
      timestamps:
        clock: 240000000
  ctrace-refs: []
)yml";
  constexpr std::string_view invalidFallback = R"yml(ctrace-run:
  trace-format: formatted
  ctrace-setup:
    - pname: core
      timestamps:
        clock: 240000000
  ctrace-refs:
    - ctrace-ref: core/timesync
      type: global_ts
      pname: core
      stream: 1
)yml";

  for (const auto& invalid :
       {std::pair<std::string_view, std::string_view>{"missing", missingRoute}, {"invalid", invalidFallback}}) {
    const auto directory = workDirectory() / invalid.first;
    writeSyntheticFormattedFixture(directory, std::string(invalid.second));
    const auto result = run({"ctrace", directory.string(), "--target", "Synthetic", "--all"});
    EXPECT_EQ(1, result.exitCode) << invalid.first << ": " << result.stderrText;
    expectContains(result.stderrText, "requires an ITM route anchor or supported feature fallback");
    expectSyntheticArtifacts(directory, false, false);
  }
}

TEST_F(CtraceIntegTests, RoutesExplicitlyFormattedSwoNamedInputToNonzeroStreams)
{
  const auto traceRun = readTestTextFile(testDataDirectory() / "formatted-synthetic" / "Synthetic.ctrace-run.yml");
  writeSyntheticFormattedFixture(workDirectory(), traceRun, "SWO");

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Synthetic", "--all"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  const auto csvPath = workDirectory() / "Synthetic.SWO.csv";
  expectCompleteSyntheticCsv(csvPath);
  expectNonEmptyFile(workDirectory() / "Synthetic.ctf" / "stream_1");
  expectNonEmptyFile(workDirectory() / "Synthetic.ctf" / "stream_2");
  EXPECT_FALSE(std::filesystem::exists(workDirectory() / "Synthetic.ctf" / "stream_0"));
  EXPECT_FALSE(std::filesystem::exists(workDirectory() / "Synthetic.SWO.traceanalysis.xml"));
}

TEST_F(CtraceIntegTests, CompletesHealthyBackendWhenOtherOutputTargetHasWrongType)
{
  const auto traceRun = readTestTextFile(testDataDirectory() / "formatted-synthetic" / "Synthetic.ctrace-run.yml");
  const auto csvFailure = workDirectory() / "csv-failure";
  writeSyntheticFormattedFixture(csvFailure, traceRun);
  ASSERT_TRUE(std::filesystem::create_directory(csvFailure / "Synthetic.TB.csv"));

  auto result = run({"ctrace", csvFailure.string(), "--target", "Synthetic", "--all"});
  EXPECT_EQ(1, result.exitCode) << result.stderrText;
  expectContains(result.stderrText, "csv output");
  expectContains(result.stderrText, "failed during start");
  EXPECT_TRUE(std::filesystem::is_directory(csvFailure / "Synthetic.TB.csv"));
  expectSyntheticCtfRoute(csvFailure / "Synthetic.ctf" / "stream_1", 1U, 240U);
  expectSyntheticCtfRoute(csvFailure / "Synthetic.ctf" / "stream_2", 2U, 480U);

  const auto ctfFailure = workDirectory() / "ctf-failure";
  writeSyntheticFormattedFixture(ctfFailure, traceRun);
  writeTestFile(ctfFailure / "Synthetic.ctf", "non-directory collision\n");
  result = run({"ctrace", ctfFailure.string(), "--target", "Synthetic", "--all"});
  EXPECT_EQ(1, result.exitCode) << result.stderrText;
  expectContains(result.stderrText, "ctf output");
  expectContains(result.stderrText, "failed during start");
  EXPECT_TRUE(std::filesystem::is_regular_file(ctfFailure / "Synthetic.ctf"));
  expectCompleteSyntheticCsv(ctfFailure / "Synthetic.TB.csv");
}

TEST_F(CtraceIntegTests, ReplacesStaleArtifactsAcrossMultiSingleMultiClockConversions)
{
  const auto traceRun = readTestTextFile(testDataDirectory() / "formatted-synthetic" / "Synthetic.ctrace-run.yml");
  writeSyntheticFormattedFixture(workDirectory(), traceRun);
  const auto ctfDirectory = workDirectory() / "Synthetic.ctf";
  const auto xmlPath = workDirectory() / "Synthetic.TB.traceanalysis.xml";

  auto result = run({"ctrace", workDirectory().string(), "--target", "Synthetic", "--ctf"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  expectSyntheticArtifacts(workDirectory(), false, true);

  writeTestFile(xmlPath, "stale xml\n");
  result = run({"ctrace", workDirectory().string(), "--target", "Synthetic", "--ctf", "--stream", "1"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  expectNonEmptyFile(ctfDirectory / "metadata");
  expectNonEmptyFile(ctfDirectory / "stream_1");
  EXPECT_FALSE(std::filesystem::exists(ctfDirectory / "stream_2"));
  expectNonEmptyFile(xmlPath);
  EXPECT_NE(readTestTextFile(xmlPath), "stale xml\n");

  writeTestFile(ctfDirectory / "stream_99", "stale stream\n");
  result = run({"ctrace", workDirectory().string(), "--target", "Synthetic", "--ctf"});
  EXPECT_EQ(0, result.exitCode) << result.stderrText;
  expectSyntheticArtifacts(workDirectory(), false, true);
  EXPECT_FALSE(std::filesystem::exists(ctfDirectory / "stream_99"));
}

TEST_F(CtraceIntegTests, RecoversAtHardwareSyncAfterResetDiscontinuity)
{
  const auto fixtureDirectory = testDataDirectory() / "Arm-reset";
  copyFixtureFile(fixtureDirectory, "Arm.SWO.raw");
  copyFixtureFile(fixtureDirectory, "Arm.ctrace-run.yml");

  const auto raw = readTestBinaryFile(workDirectory() / "Arm.SWO.raw");
  ASSERT_EQ(131071U, raw.size());
  constexpr std::array<unsigned char, 6U> hardwareSync{0U, 0U, 0U, 0U, 0U, 0x80U};
  for (const auto offset : {0U, 128U}) {
    ASSERT_LE(offset + hardwareSync.size(), raw.size());
    EXPECT_TRUE(std::equal(hardwareSync.begin(), hardwareSync.end(), raw.begin() + offset)) << offset;
  }

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Arm", "--csv"});
  EXPECT_EQ(1, result.exitCode) << result.stderrText;
  expectContains(result.stderrText, "[error] OpenCSD detected an invalid ITM packet sequence at raw offset 10");
  expectContains(result.stderrText,
                 "[error] OpenCSD consumed 116 raw bytes while waiting for usable ITM trace packets");
  expectContains(result.stderrText, "[info] processed 131071 input bytes in ");
  expectContains(result.stderrText, "); trace/diagnostic records: 52374\n");
  expectNotContains(result.stderrText, "OpenCSD made no progress");
  expectNotContains(result.stderrText, "OpenCSD made no decode progress");
  expectNotContains(result.stderrText, "decode aborted");

  const auto csv = readTestTextFile(workDirectory() / "Arm.SWO.csv");
  expectNotContains(csv, ",itm,");
  const auto recoveryError = csv.find("OpenCSD detected an invalid ITM packet sequence at raw offset 10.");
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

TEST_F(CtraceIntegTests, ReportsMalformedAsyncDetailsAndRealResynchronization)
{
  writeTestFile(workDirectory() / "Malformed.ctrace-run.yml", R"yml(ctrace-run:
  ctrace-setup:
    - pname: core
      timestamps:
        clock: 400000000
  ctrace-refs:
    - ctrace-ref: core/itm
      type: itm
      pname: core
      stream: 1
)yml");
  auto payload = FormattedTraceTestSupport::itmHardwareSync();
  // A broken acquisition boundary can expose the end of a PC sample as 00 08.
  // Once synchronized, OpenCSD interprets 00 as ASYNC and rejects the following 08.
  payload.insert(payload.end(), {0x00U, 0x08U});
  const auto lostPacket = FormattedTraceTestSupport::itmSoftwarePacket(1U, 'X');
  payload.insert(payload.end(), lostPacket.begin(), lostPacket.end());
  const auto sync = FormattedTraceTestSupport::itmHardwareSync();
  payload.insert(payload.end(), sync.begin(), sync.end());
  const auto retainedPacket = FormattedTraceTestSupport::itmSoftwarePacket(1U, 'A');
  payload.insert(payload.end(), retainedPacket.begin(), retainedPacket.end());
  const auto capture = FormattedTraceTestSupport::memoryAlignedFrames({{1U, payload}});
  writeTestFile(workDirectory() / "Malformed.TB.raw", std::string(capture.begin(), capture.end()));

  const auto result = run({"ctrace", workDirectory().string(), "--target", "Malformed", "--all"});
  EXPECT_EQ(result.exitCode, 1) << "the retained output must not hide the input error";
  const auto csv = readTestTextFile(workDirectory() / "Malformed.TB.csv");
  for (const auto& text : {result.stderrText, csv}) {
    expectContains(text, "OCSD_ERR_BAD_PACKET_SEQ");
    expectContains(text, "Async Packet: unexpected none zero value");
    expectContains(text, "packet=ASYNC");
    expectContains(text, "bytes=[00 08]");
    expectContains(text, "ITM decoding resumed at hardware SYNC at raw offset");
    EXPECT_EQ(countOccurrences(text, "OCSD_ERR_BAD_PACKET_SEQ"), 1U);
  }
  expectContains(csv, ",1,itm,1,0x41");
  expectNotContains(csv, ",1,itm,1,0x58");
  expectNonEmptyFile(workDirectory() / "Malformed.ctf" / "metadata");
  expectNonEmptyFile(workDirectory() / "Malformed.ctf" / "stream_1");
}

TEST_F(CtraceIntegTests, RetainsCsvAndUnfilteredAbortAfterIncompleteFormattedTail)
{
  using namespace FormattedTraceTestSupport;
  auto payload = itmHardwareSync();
  appendBytes(payload, itmSoftwarePacket(1U, 'A'));
  // Resolve the valid prefix before leaving the four-byte PC sample incomplete.
  appendBytes(payload, itmLocalTimestampPacket(42U));
  payload.insert(payload.end(), {0x17U, 0xf2U});
  const auto capture = memoryAlignedFrames({{1U, payload}});
  const auto abortMessage = "decode aborted after processing " + std::to_string(capture.size()) +
                            " input bytes; trace is incomplete: OpenCSD aborted end-of-trace processing: "
                            "incomplete ITM packet at end of input";
  struct SelectionCase {
    std::string name;
    std::vector<std::string> arguments;
    bool includesPayload;
    bool includesRouteError;
  };
  const std::vector<SelectionCase> selections{
      {"all", {}, true, true},
      {"type-filter", {"--type", "itm"}, true, false},
      {"stream-filter", {"--stream", "2"}, false, false},
  };

  for (const auto& selection : selections) {
    SCOPED_TRACE(selection.name);
    const auto directory = workDirectory() / selection.name;
    writeTestFile(directory / "Incomplete.ctrace-run.yml", R"yml(ctrace-run:
  trace-format: formatted
  ctrace-setup:
    - pname: core
      timestamps:
        clock: 400000000
  ctrace-refs:
    - ctrace-ref: core/itm
      type: itm
      pname: core
      stream: 1
)yml");
    writeTestFile(directory / "Incomplete.TB.raw", std::string(capture.begin(), capture.end()));
    std::vector<std::string> arguments{"ctrace", directory.string(), "--target", "Incomplete", "--all"};
    arguments.insert(arguments.end(), selection.arguments.begin(), selection.arguments.end());

    const auto result = run(std::move(arguments));
    EXPECT_EQ(result.exitCode, 1) << result.stderrText;
    expectContains(result.stderrText, "[error] " + abortMessage);
    EXPECT_EQ(countOccurrences(result.stderrText, "decode aborted after processing "), 1U);
    expectContains(result.stderrText, "incomplete ITM packet at end of input at raw offset");
    expectContains(result.stderrText, "packet=DWT, size=2 bytes, bytes=[17 f2]");
    expectNotContains(result.stderrText, "OCSD_RESP_CONT");

    const auto csvPath = directory / "Incomplete.TB.csv";
    expectNonEmptyFile(csvPath);
    const auto csv = readTestTextFile(csvPath);
    const auto lines = readTestLines(csvPath);
    ASSERT_GE(lines.size(), 2U);
    EXPECT_EQ(lines.back(), ",,error,,,,," + abortMessage)
        << "the final abort is input-wide, has no cycles/source, and bypasses selection";
    EXPECT_EQ(countOccurrences(csv, "decode aborted after processing "), 1U);
    const auto expectedPayload = selection.includesPayload ? "42,1,itm,1,0x41,,,\n" : "";
    const auto semanticRows = traceCsvRows(csv);
    if (selection.includesPayload) {
      expectContains(semanticRows, expectedPayload);
    } else {
      EXPECT_EQ(countCsvStreamRows(csv, "1"), 0U);
    }
    EXPECT_EQ(countOccurrences(csv, ",1,error,"), selection.includesRouteError ? 1U : 0U);
    if (!selection.includesRouteError) {
      EXPECT_EQ(semanticRows, "cycles,stream,type,source,value,pc,address,note\n" +
                                 std::string(expectedPayload) + ",,error,,,,," + abortMessage + "\n");
    }
    expectNotContains(csv, ",pcsample,");
    EXPECT_FALSE(std::filesystem::exists(directory / "Incomplete.ctf"));
    EXPECT_FALSE(std::filesystem::exists(directory / "Incomplete.TB.traceanalysis.xml"));
  }
}

} // namespace
