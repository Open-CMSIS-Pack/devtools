/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.hpp"
#include <gtest/gtest.h>
#include "CliOptions.hpp"
#include "CliParser.hpp"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

static bool validationErrorContains(int argc, const char* const argv[], std::string_view expected)
{
  try {
    const auto options = CliParser::parse(argc, argv);
    CliParser::validate(options);
  } catch (const std::runtime_error& error) {
    return std::string_view(error.what()).find(expected) != std::string_view::npos;
  }
  return false;
}

static bool parseFails(int argc, const char* const argv[])
{
  try {
    (void)CliParser::parse(argc, argv);
  } catch (const std::runtime_error&) {
    return true;
  }
  return false;
}

TEST(CtraceUnitTests, testCliParserOutputOptions)
{
  const char* allArgv[] = {
      "ctrace", ".trace", "--all", "--target", "Board.Debug", "--type", "dwt", "itm", "--stream", "1", "2",
  };
  const auto all = CliParser::parse(11, allArgv);
  CliParser::validate(all);
  require(all.traceDir == std::optional<std::string>(".trace"), "CliParser trace directory mismatch");
  require(all.targetName == std::optional<std::string>("Board.Debug"), "CliParser target mismatch");
  require(all.outputFormat == OutputFormat::All, "CliParser all output mismatch");
  require(all.selection.types == std::vector<std::string>({"dwt", "itm"}), "CliParser type filter mismatch");
  require(all.selection.streams == std::vector<std::uint8_t>({1U, 2U}), "CliParser stream filter mismatch");

  const char* checkArgv[] = {"ctrace", ".trace"};
  const auto check = CliParser::parse(2, checkArgv);
  CliParser::validate(check);
  require(check.outputFormat == OutputFormat::None, "CliParser check-only output mismatch");

  const char* csvArgv[] = {"ctrace", ".trace", "--csv"};
  const auto csv = CliParser::parse(3, csvArgv);
  CliParser::validate(csv);
  require(csv.outputFormat == OutputFormat::Csv, "CliParser CSV output mismatch");

  const char* ctfArgv[] = {"ctrace", ".trace", "--ctf"};
  const auto ctf = CliParser::parse(3, ctfArgv);
  CliParser::validate(ctf);
  require(ctf.outputFormat == OutputFormat::Ctf, "CliParser CTF output mismatch");

  const char* shortAllArgv[] = {"ctrace", ".trace", "-a"};
  const auto shortAll = CliParser::parse(3, shortAllArgv);
  CliParser::validate(shortAll);
  require(shortAll.outputFormat == OutputFormat::All, "CliParser short all output mismatch");

  const char* combinedArgv[] = {"ctrace", ".trace", "--csv", "--ctf"};
  const auto combined = CliParser::parse(4, combinedArgv);
  CliParser::validate(combined);
  require(combined.outputFormat == OutputFormat::All, "CliParser combined output mismatch");

  const char* versionArgv[] = {"ctrace", "-V"};
  const auto version = CliParser::parse(2, versionArgv);
  CliParser::validate(version);
  require(version.version, "CliParser short version alias mismatch");
}

TEST(CtraceUnitTests, testCliParserRejectsInvalidSelections)
{
  const char* invalidVersionTypeArgv[] = {"ctrace", "-V", "--type", "DWT"};
  require(validationErrorContains(4, invalidVersionTypeArgv, "Invalid --type value: DWT"),
          "--version must not bypass packet type validation");

  const char* missingArgv[] = {"ctrace"};
  require(validationErrorContains(1, missingArgv, "Specify <trace-dir>"), "CliParser should require a trace directory");

  const char* invalidTypeArgv[] = {"ctrace", ".trace", "--type", "invalid"};
  require(validationErrorContains(4, invalidTypeArgv, "Invalid --type value"),
          "CliParser should reject invalid packet types");

  for (const auto* type : {"event", "pmu", "pcsample"}) {
    const char* unsupportedTypeArgv[] = {"ctrace", ".trace", "--type", type};
    require(validationErrorContains(4, unsupportedTypeArgv, "Invalid --type value"),
            std::string("CliParser should reject unimplemented type ") + type);
  }

  const char* wrongCaseTypeArgv[] = {"ctrace", ".trace", "--type", "DWT"};
  require(validationErrorContains(4, wrongCaseTypeArgv, "Invalid --type value: DWT"),
          "CliParser should enforce case-sensitive packet type names");

  const char* commaSeparatedArgv[] = {"ctrace", ".trace", "--type=dwt,event"};
  require(parseFails(3, commaSeparatedArgv), "CliParser should accept only the specified space-separated selectors");
}

TEST(CtraceUnitTests, testCliParserStreamRangeAndUnknownOptions)
{
  const char* traceBusZeroArgv[] = {"ctrace", ".trace", "--stream", "0"};
  const auto traceBusZero = CliParser::parse(4, traceBusZeroArgv);
  require(traceBusZero.selection.streams == std::vector<std::uint8_t>({0U}),
          "CliParser must accept Trace Bus ID 0 for unformatted input");

  for (const auto* stream : {"112", "255"}) {
    const char* invalidStreamArgv[] = {"ctrace", ".trace", "--stream", stream};
    require(validationErrorContains(4, invalidStreamArgv, "CoreSight Trace Bus ID"),
            std::string("CliParser accepted invalid Trace Bus ID ") + stream);
  }

  const char* unknownArgv[] = {"ctrace", ".trace", "--unknown"};
  require(parseFails(3, unknownArgv), "CliParser should reject unknown options");
}
