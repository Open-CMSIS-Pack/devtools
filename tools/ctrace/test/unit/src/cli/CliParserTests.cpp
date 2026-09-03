/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.h"
#include <gtest/gtest.h>
#include "CliOptions.h"
#include "CliParser.h"
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/** @brief Parses a compact command-line argument list. */
static CliOptions parse(std::initializer_list<const char*> arguments)
{
  return CliParser::parse(std::vector<std::string>(arguments.begin(), arguments.end()));
}

/** @brief Parses and validates a compact command-line argument list. */
static CliOptions parseAndValidate(std::initializer_list<const char*> arguments)
{
  auto options = parse(arguments);
  CliParser::validate(options);
  return options;
}

/** @brief Tests whether CLI validation reports expected text. */
static bool validationErrorContains(std::initializer_list<const char*> arguments, std::string_view expected)
{
  return throwsWithMessage(
      [&arguments] {
        const auto options = parse(arguments);
        CliParser::validate(options);
      },
      expected);
}

/** @brief Tests whether parsing the supplied command line fails. */
static bool parseFails(std::initializer_list<const char*> arguments)
{
  return throwsException([&arguments] { (void)parse(arguments); });
}

/** @brief Requires a command line to select the expected output format. */
static void expectOutputFormat(std::initializer_list<const char*> arguments, OutputFormat expected)
{
  EXPECT_EQ(parseAndValidate(arguments).outputFormat, expected);
}

TEST(CtraceUnitTests, testCliParserOutputOptions)
{
  const auto all = parseAndValidate(
      {"ctrace", ".trace", "--all", "--target", "Board.Debug", "--type", "dwt", "itm", "--stream", "1", "2"});
  ASSERT_TRUE(all.traceDir == std::optional<std::string>(".trace")) << "CliParser trace directory mismatch";
  ASSERT_TRUE(all.targetName == std::optional<std::string>("Board.Debug")) << "CliParser target mismatch";
  ASSERT_TRUE(all.outputFormat == OutputFormat::All) << "CliParser all output mismatch";
  ASSERT_TRUE(all.selection.types == std::vector<std::string>({"dwt", "itm"})) << "CliParser type filter mismatch";
  ASSERT_TRUE(all.selection.streams == std::vector<std::uint8_t>({1U, 2U})) << "CliParser stream filter mismatch";

  const auto knownTypes = parseAndValidate({"ctrace", ".trace", "--type", "event", "pmu", "pcsample"});
  ASSERT_TRUE(knownTypes.selection.types == std::vector<std::string>({"event", "pmu", "pcsample"}))
      << "CliParser known packet-type filter mismatch";

  expectOutputFormat({"ctrace", ".trace"}, OutputFormat::None);
  expectOutputFormat({"ctrace", ".trace", "--csv"}, OutputFormat::Csv);
  expectOutputFormat({"ctrace", ".trace", "--ctf"}, OutputFormat::Ctf);
  expectOutputFormat({"ctrace", ".trace", "-a"}, OutputFormat::All);
  expectOutputFormat({"ctrace", ".trace", "--csv", "--ctf"}, OutputFormat::All);

  const auto version = parseAndValidate({"ctrace", "-V"});
  ASSERT_TRUE(version.version) << "CliParser short version alias mismatch";

  const auto help = parseAndValidate({"ctrace", "--help"});
  ASSERT_TRUE(help.help) << "CliParser help option mismatch";
  const auto shortHelp = parseAndValidate({"ctrace", "-h"});
  ASSERT_TRUE(shortHelp.help) << "CliParser short help alias mismatch";
  const auto helpText = CliParser::helpString();
  ASSERT_TRUE(helpText.find("--help") == std::string::npos) << "CliParser help text must hide --help";
  for (const auto expected : {
           "Generate only CSV output",
           "Generate only CTF / Trace Compass XML output",
           "Generate CSV and CTF / XML output",
           "Filter output for specific packet types (default: all)",
           "Filter output for specific streams (default: all)",
           "Specify a trace solution-set (default: all)",
           "Print version",
       }) {
    ASSERT_TRUE(helpText.find(expected) != std::string::npos) << "CliParser help text differs from the specification";
  }

  const auto trailingTraceDirectory =
      parseAndValidate({"ctrace", "--type", "dwt", "itm", "--stream", "1", "2", "--all", "--", ".trace"});
  ASSERT_TRUE(trailingTraceDirectory.traceDir == std::optional<std::string>(".trace"))
      << "CliParser must accept a delimited trace directory after selector options";
  ASSERT_TRUE(trailingTraceDirectory.selection.types == std::vector<std::string>({"dwt", "itm"}) &&
              trailingTraceDirectory.selection.streams == std::vector<std::uint8_t>({1U, 2U}))
      << "CliParser trailing trace directory must preserve selector values";
}

TEST(CtraceUnitTests, testCliParserRejectsInvalidSelections)
{
  ASSERT_TRUE(validationErrorContains({"ctrace", "-V", "--type", "DWT"}, "Invalid --type value: DWT"))
      << "--version must not bypass packet type validation";

  ASSERT_TRUE(validationErrorContains({"ctrace"}, "Specify <trace-dir>"))
      << "CliParser should require a trace directory";

  ASSERT_TRUE(validationErrorContains({"ctrace", ".trace", "--type", "invalid"}, "Invalid --type value"))
      << "CliParser should reject invalid packet types";
  ASSERT_TRUE(validationErrorContains({"ctrace", ".trace", "--type", "dwt", "invalid", "--all"},
                                      "Invalid --type value: invalid"))
      << "CliParser should reject an invalid selector after a valid selector";

  ASSERT_TRUE(validationErrorContains({"ctrace", ".trace", "--type", "DWT"}, "Invalid --type value: DWT"))
      << "CliParser should enforce case-sensitive packet type names";

  ASSERT_TRUE(parseFails({"ctrace", ".trace", "--type=dwt,event"}))
      << "CliParser should accept only the specified space-separated selectors";
}

TEST(CtraceUnitTests, testCliParserStreamRangeAndUnknownOptions)
{
  const auto traceBusZero = parse({"ctrace", ".trace", "--stream", "0"});
  ASSERT_TRUE(traceBusZero.selection.streams == std::vector<std::uint8_t>({0U}))
      << "CliParser must accept Trace Bus ID 0 for unformatted input";

  for (const auto* stream : {"112", "255"}) {
    ASSERT_TRUE(validationErrorContains({"ctrace", ".trace", "--stream", stream}, "CoreSight Trace Bus ID"))
        << std::string("CliParser accepted invalid Trace Bus ID ") + stream;
  }

  ASSERT_TRUE(parseFails({"ctrace", ".trace", "--unknown"})) << "CliParser should reject unknown options";
  ASSERT_TRUE(parseFails({"ctrace", ".trace", "--stream", "invalid"}))
      << "CliParser should reject non-numeric stream IDs";
  ASSERT_TRUE(parseFails({"ctrace", ".trace", "--stream", "1x"}))
      << "CliParser should reject trailing stream ID characters";
  ASSERT_TRUE(parseFails({"ctrace", ".trace", "--stream", "999999999999999999999"}))
      << "CliParser should reject overflowing stream IDs";
  ASSERT_TRUE(parseFails({"ctrace", ".trace", "--type="})) << "CliParser should reject an empty inline selector";
  ASSERT_TRUE(parseFails({"ctrace", ".trace", "--type"})) << "CliParser should require at least one selector value";
  ASSERT_TRUE(parseFails({"ctrace", ".trace", "unexpected"})) << "CliParser should reject a second positional argument";
}
