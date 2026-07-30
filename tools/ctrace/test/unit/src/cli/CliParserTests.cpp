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
#include <vector>

TEST(CtraceUnitTests, testCliParser)
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

  const char* invalidVersionTypeArgv[] = {"ctrace", "-V", "--type", "DWT"};
  bool rejectedInvalidVersionType = false;
  try {
    const auto invalid = CliParser::parse(4, invalidVersionTypeArgv);
    CliParser::validate(invalid);
  } catch (const std::runtime_error& error) {
    rejectedInvalidVersionType = std::string(error.what()).find("Invalid --type value: DWT") != std::string::npos;
  }
  require(rejectedInvalidVersionType, "--version must not bypass packet type validation");

  const char* missingArgv[] = {"ctrace"};
  bool rejectedMissingTraceDir = false;
  try {
    const auto missing = CliParser::parse(1, missingArgv);
    CliParser::validate(missing);
  } catch (const std::runtime_error& error) {
    rejectedMissingTraceDir = std::string(error.what()).find("Specify <trace-dir>") != std::string::npos;
  }
  require(rejectedMissingTraceDir, "CliParser should require a trace directory");

  const char* invalidTypeArgv[] = {"ctrace", ".trace", "--type", "invalid"};
  bool rejectedInvalidType = false;
  try {
    const auto invalid = CliParser::parse(4, invalidTypeArgv);
    CliParser::validate(invalid);
  } catch (const std::runtime_error& error) {
    rejectedInvalidType = std::string(error.what()).find("Invalid --type value") != std::string::npos;
  }
  require(rejectedInvalidType, "CliParser should reject invalid packet types");

  for (const auto* type : {"event", "pmu", "pcsample"}) {
    const char* unsupportedTypeArgv[] = {"ctrace", ".trace", "--type", type};
    bool rejectedUnsupportedType = false;
    try {
      const auto unsupported = CliParser::parse(4, unsupportedTypeArgv);
      CliParser::validate(unsupported);
    } catch (const std::runtime_error& error) {
      rejectedUnsupportedType = std::string(error.what()).find("Invalid --type value") != std::string::npos;
    }
    require(rejectedUnsupportedType, std::string("CliParser should reject unimplemented type ") + type);
  }

  const char* wrongCaseTypeArgv[] = {"ctrace", ".trace", "--type", "DWT"};
  bool rejectedWrongCaseType = false;
  try {
    const auto invalid = CliParser::parse(4, wrongCaseTypeArgv);
    CliParser::validate(invalid);
  } catch (const std::runtime_error& error) {
    rejectedWrongCaseType = std::string(error.what()).find("Invalid --type value: DWT") != std::string::npos;
  }
  require(rejectedWrongCaseType, "CliParser should enforce case-sensitive packet type names");

  const char* commaSeparatedArgv[] = {"ctrace", ".trace", "--type=dwt,event"};
  bool rejectedCommaSeparatedTypes = false;
  try {
    const auto invalid = CliParser::parse(3, commaSeparatedArgv);
    CliParser::validate(invalid);
  } catch (const std::runtime_error&) {
    rejectedCommaSeparatedTypes = true;
  }
  require(rejectedCommaSeparatedTypes, "CliParser should accept only the specified space-separated selectors");

  const char* traceBusZeroArgv[] = {"ctrace", ".trace", "--stream", "0"};
  const auto traceBusZero = CliParser::parse(4, traceBusZeroArgv);
  require(traceBusZero.selection.streams == std::vector<std::uint8_t>({0U}),
          "CliParser must accept Trace Bus ID 0 for unformatted input");

  for (const auto* stream : {"112", "255"}) {
    const char* invalidStreamArgv[] = {"ctrace", ".trace", "--stream", stream};
    bool rejectedInvalidStream = false;
    try {
      (void)CliParser::parse(4, invalidStreamArgv);
    } catch (const std::runtime_error& error) {
      rejectedInvalidStream = std::string(error.what()).find("CoreSight Trace Bus ID") != std::string::npos;
    }
    require(rejectedInvalidStream, std::string("CliParser accepted invalid Trace Bus ID ") + stream);
  }

  const char* unknownArgv[] = {"ctrace", ".trace", "--unknown"};
  bool rejectedUnknown = false;
  try {
    (void)CliParser::parse(3, unknownArgv);
  } catch (const std::runtime_error&) {
    rejectedUnknown = true;
  }
  require(rejectedUnknown, "CliParser should reject unknown options");
}
