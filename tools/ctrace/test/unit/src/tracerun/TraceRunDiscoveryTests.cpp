/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestPath.h"
#include "TestPlatform.h"
#include "TestSupport.h"
#include <gtest/gtest.h>
#include "CtraceRunMeta.h"
#include "TraceRunDiscovery.h"
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

static_assert(!std::is_aggregate_v<TraceRunInputDescriptor>);
static_assert(!std::is_default_constructible_v<TraceRunInputDescriptor>);
static_assert(!std::is_copy_constructible_v<TraceRunInputDescriptor>);
static_assert(!std::is_default_constructible_v<CtraceRunMeta>);
using ResolveInputSignature = TraceRunInputDescriptor (*)(TraceRunConfig, const SkippedTraceRunInputSink&);
static_assert(std::is_same_v<decltype(&TraceRunDiscovery::resolveInput), ResolveInputSignature>);

/** @brief Creates parsed configuration carrying the requested declaration state. */
static TraceRunConfig inputConfig(const std::filesystem::path& configFile,
                                   const std::optional<TraceRunFormat>& traceFormat = std::nullopt)
{
  TraceRunConfig config;
  config.path = configFile.string();
  config.traceFormat = traceFormat;
  TraceRunReference route;
  route.ctraceRef = "core/itm";
  route.type = "itm";
  route.processorName = "core";
  route.stream = 1U;
  config.references.push_back(std::move(route));
  return config;
}

TEST(CtraceUnitTests, testTraceRunDiscovery)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-run-discovery-test");
  const auto& traceDir = temporaryPath.path();
  writeTestFile(traceDir / "Beta.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Alpha.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Alpha.SWO.raw");
  writeTestFile(traceDir / "Alpha.TB.raw");
  writeTestFile(traceDir / "Alpha.TB_ETB-0.raw");
  writeTestFile(traceDir / "Alpha.TB_MTB.raw");
  writeTestFile(traceDir / "Alpha.ER.raw");
  writeTestFile(traceDir / "Alpha.swo.raw");
  writeTestFile(traceDir / "Alpha.custom.raw");
  writeTestFile(traceDir / "unrelated.raw");
  writeTestFile(traceDir / "Alpha..raw");
  writeTestFile(traceDir / "Alpha.TB_.raw");
  writeTestFile(traceDir / "Alpha.TBish.raw");
  writeTestFile(traceDir / "Alpha.TB_bad.name.raw");
  writeTestFile(traceDir / "Alpha.TB_bad name.raw");
  writeTestFile(traceDir / "Alpha.TB_\xC3\x84.raw");
  std::filesystem::create_directories(traceDir / "ignored.ctrace-run.yml");
  std::filesystem::create_directories(traceDir / "Alpha.SWO.raw.dir");
  std::filesystem::create_directories(traceDir / "Alpha.TB_ETB_0.raw");

  const auto batch = TraceRunDiscovery::selectConfigFiles(traceDir, std::nullopt);
  ASSERT_TRUE(batch.size() == 2U) << "TraceRunDiscovery batch configuration count mismatch";
  ASSERT_TRUE(batch[0].filename() == "Alpha.ctrace-run.yml") << "TraceRunDiscovery batch sort mismatch";
  ASSERT_TRUE(batch[1].filename() == "Beta.ctrace-run.yml") << "TraceRunDiscovery second batch item mismatch";

  const auto selected = TraceRunDiscovery::selectConfigFiles(traceDir, std::string("Alpha"));
  ASSERT_TRUE(selected.size() == 1U) << "TraceRunDiscovery target selection count mismatch";
  ASSERT_TRUE(selected[0].filename() == "Alpha.ctrace-run.yml") << "TraceRunDiscovery target path mismatch";
  ASSERT_TRUE(TraceRunDiscovery::solutionSetName(selected[0]) == "Alpha") << "TraceRunDiscovery solution-set mismatch";

  for (const auto& format : {std::optional<TraceRunFormat>{}, {TraceRunFormat::Unformatted},
                            {TraceRunFormat::Formatted}}) {
    std::vector<std::string> skippedChannels;
    const auto message = captureExceptionMessage([&] {
      (void)TraceRunDiscovery::resolveInput(inputConfig(selected[0], format),
                                            [&](const auto& input) { skippedChannels.push_back(input.channel); });
    });
    ASSERT_TRUE(message.has_value());
    EXPECT_NE(message->find("multiple eligible raw trace inputs"), std::string::npos);
    for (const auto* channel : {"SWO", "TB", "TB_ETB-0", "TB_ETB_0", "TB_MTB"}) {
      EXPECT_NE(message->find("Alpha." + std::string(channel) + ".raw"), std::string::npos);
    }
    for (const auto* channel : {"swo", "custom", "TB_", "TBish", "TB_bad.name", "TB_bad name", "TB_\xC3\x84"}) {
      EXPECT_EQ(message->find("Alpha." + std::string(channel) + ".raw"), std::string::npos);
    }
    EXPECT_EQ(skippedChannels, (std::vector<std::string>{"ER"}));
  }

  const std::vector<std::string> unsafeTargets{
      "",
      ".",
      "..",
      "../Alpha",
      "..\\Alpha",
      "/Alpha",
      "C:Alpha",
      "C:/Alpha",
      "C:\\Alpha",
      "\\\\server\\share",
      "Alpha:stream",
      "Alpha?Beta",
      "Alpha.",
      "Alpha ",
      "NUL",
      "con.txt",
      "PRN",
      "AUX",
      "CONIN$",
      "CONOUT$",
      "COM1.log",
      "LPT9",
      "bad\x01name",
  };
  for (const auto& unsafeTarget : unsafeTargets) {
    ASSERT_TRUE(throwsWithMessage([&] { (void)TraceRunDiscovery::selectConfigFiles(traceDir, unsafeTarget); },
                                  "solution-set name"))
        << "TraceRunDiscovery should reject unsafe target name: " + unsafeTarget;
  }

  EXPECT_THROW((void)TraceRunDiscovery::selectConfigFiles(traceDir, std::string("Missing")), std::runtime_error);
  EXPECT_THROW((void)TraceRunDiscovery::selectConfigFiles(traceDir, std::string("ABCD")), std::runtime_error);
  EXPECT_THROW((void)TraceRunDiscovery::selectConfigFiles(traceDir, std::string("COM0")), std::runtime_error);
  EXPECT_THROW((void)TraceRunDiscovery::selectConfigFiles(traceDir / "missing", std::nullopt), std::runtime_error);
  EXPECT_THROW((void)TraceRunDiscovery::solutionSetName("wrong.yml"), std::runtime_error);
  EXPECT_THROW((void)TraceRunDiscovery::solutionSetName(".ctrace-run.yml"), std::runtime_error);

  const TemporaryTestPath emptyPath("ctrace-trace-run-discovery-empty-test");
  emptyPath.createDirectory();
  writeTestFile(emptyPath.path() / ".ctrace-run.yml");
  EXPECT_THROW((void)TraceRunDiscovery::selectConfigFiles(emptyPath.path(), std::nullopt), std::runtime_error);
}

TEST(CtraceUnitTests, testTraceRunDiscoveryResolvesOnePreflightedInput)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-run-input-resolution-test");
  const auto& root = temporaryPath.createDirectory();
  const auto legacyConfig = root / "Legacy.ctrace-run.yml";
  const auto swo = root / "Legacy.SWO.raw";
  writeTestFile(swo);

  {
    auto legacy = TraceRunDiscovery::resolveInput(inputConfig(legacyConfig));
    EXPECT_EQ(legacy.path(), swo);
    EXPECT_EQ(legacy.format(), TraceRunFormat::Unformatted);
    EXPECT_EQ(legacy.metadata().traceFormat(), TraceRunFormat::Unformatted);
    ASSERT_EQ(legacy.metadata().routes().size(), 1U);
    EXPECT_FALSE(legacy.metadata().routes().front().identity.traceBusId.has_value());
  }

  const auto explicitConfig = root / "Explicit.ctrace-run.yml";
  const auto tb = root / "Explicit.TB_MTB.raw";
  for (const auto size : {0U, 13U}) {
    writeTestFile(tb, std::string(size, 'u'));
    auto explicitUnformatted =
        TraceRunDiscovery::resolveInput(inputConfig(explicitConfig, TraceRunFormat::Unformatted));
    EXPECT_EQ(explicitUnformatted.path(), tb);
    EXPECT_EQ(explicitUnformatted.format(), TraceRunFormat::Unformatted);
    EXPECT_EQ(explicitUnformatted.metadata().traceFormat(), TraceRunFormat::Unformatted);
  }

  const auto formattedConfig = root / "Formatted.ctrace-run.yml";
  const auto formatted = root / "Formatted.TB.raw";
  for (const auto size : {0U, 16U, 32U}) {
    writeTestFile(formatted, std::string(size, 'f'));
    auto descriptor = TraceRunDiscovery::resolveInput(inputConfig(formattedConfig, TraceRunFormat::Formatted));
    EXPECT_EQ(descriptor.path(), formatted);
    EXPECT_EQ(descriptor.format(), TraceRunFormat::Formatted);
    EXPECT_EQ(descriptor.metadata().traceFormat(), TraceRunFormat::Formatted);
    ASSERT_EQ(descriptor.metadata().routes().size(), 1U);
    EXPECT_EQ(descriptor.metadata().routes().front().identity.traceBusId, 1U);
  }
  for (const auto size : {1U, 15U, 17U, 31U}) {
    writeTestFile(formatted, std::string(size, 'f'));
    EXPECT_TRUE(throwsWithMessage(
        [&] { (void)TraceRunDiscovery::resolveInput(inputConfig(formattedConfig, TraceRunFormat::Formatted)); },
        "multiple of 16 bytes"));
  }
}

TEST(CtraceUnitTests, testTraceRunDiscoveryResolvesChannelDefaultsAndExplicitOverrides)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-run-format-default-test");
  const auto& root = temporaryPath.createDirectory();
  for (const auto* channel : {"SWO", "TB", "TB_ETB-0"}) {
    const auto solutionSet = std::string("Default-") + channel;
    const auto configFile = root / (solutionSet + ".ctrace-run.yml");
    const auto rawFile = root / (solutionSet + "." + channel + ".raw");
    for (const auto& overrideFormat : {std::optional<TraceRunFormat>{}, {TraceRunFormat::Unformatted},
                                      {TraceRunFormat::Formatted}}) {
      SCOPED_TRACE(rawFile.string());
      const auto expected = overrideFormat.value_or(std::string(channel) == "SWO" ? TraceRunFormat::Unformatted
                                                                                 : TraceRunFormat::Formatted);
      writeTestFile(rawFile, std::string(expected == TraceRunFormat::Formatted ? 16U : 13U, '\0'));
      const auto config = inputConfig(configFile, overrideFormat);
      const auto descriptor = TraceRunDiscovery::resolveInput(config);
      EXPECT_EQ(descriptor.path(), rawFile);
      EXPECT_EQ(descriptor.format(), expected);
      EXPECT_EQ(descriptor.metadata().traceFormat(), expected);
      ASSERT_EQ(descriptor.metadata().routes().size(), 1U);
      EXPECT_EQ(descriptor.metadata().routes().front().identity.traceBusId.has_value(),
                expected == TraceRunFormat::Formatted);
      EXPECT_EQ(config.traceFormat, overrideFormat);
    }
  }
}

TEST(CtraceUnitTests, testTraceRunDiscoveryResolvesFormatBeforeMulticoreRouteNormalization)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-run-default-format-routes-test");
  const auto& root = temporaryPath.createDirectory();
  const auto rawFile = root / "Multicore.TB.raw";
  writeTestFile(rawFile, std::string(16U, '\0'));
  auto config = inputConfig(root / "Multicore.ctrace-run.yml");
  auto secondRoute = config.references.front();
  secondRoute.ctraceRef = "other/itm";
  secondRoute.processorName = "other";
  secondRoute.stream = 2U;
  config.references.push_back(std::move(secondRoute));

  const auto descriptor = TraceRunDiscovery::resolveInput(config);
  EXPECT_EQ(descriptor.format(), TraceRunFormat::Formatted);
  ASSERT_EQ(descriptor.metadata().routes().size(), 2U);
  EXPECT_EQ(descriptor.metadata().routes()[0].identity.traceBusId, 1U);
  EXPECT_EQ(descriptor.metadata().routes()[0].processorName, "core");
  EXPECT_EQ(descriptor.metadata().routes()[1].identity.traceBusId, 2U);
  EXPECT_EQ(descriptor.metadata().routes()[1].processorName, "other");
}

TEST(CtraceUnitTests, testTraceRunDiscoveryRejectsInvalidInputSelectionBeforePreflight)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-run-input-selection-test");
  const auto& root = temporaryPath.createDirectory();
  EXPECT_TRUE(throwsWithMessage([&] { (void)TraceRunDiscovery::resolveInput({}); },
                                "configuration has no source path"));

  const auto missingConfig = root / "Missing.ctrace-run.yml";
  EXPECT_TRUE(throwsWithMessage([&] { (void)TraceRunDiscovery::resolveInput(inputConfig(missingConfig)); },
                                "no eligible raw trace input"));

  const auto eventRecorderConfig = root / "ErOnly.ctrace-run.yml";
  const auto eventRecorder = root / "ErOnly.ER.raw";
  writeTestFile(eventRecorder);
  std::vector<std::string> skippedChannels;
  EXPECT_TRUE(throwsWithMessage(
      [&] {
        (void)TraceRunDiscovery::resolveInput(inputConfig(eventRecorderConfig, TraceRunFormat::Formatted),
                                              [&](const auto& input) { skippedChannels.push_back(input.channel); });
      },
      "no eligible raw trace input"));
  EXPECT_EQ(skippedChannels, (std::vector<std::string>{"ER"}));

  const auto swoTbConfig = root / "SwoTb.ctrace-run.yml";
  const auto directoryInput = root / "SwoTb.SWO.raw";
  std::filesystem::create_directory(directoryInput);
  const auto regularInput = root / "SwoTb.TB.raw";
  writeTestFile(regularInput);
  EXPECT_TRUE(throwsWithMessage(
      [&] { (void)TraceRunDiscovery::resolveInput(inputConfig(swoTbConfig, TraceRunFormat::Unformatted)); },
      "multiple eligible raw trace inputs"));

  const auto tbNamedConfig = root / "TbNamed.ctrace-run.yml";
  writeTestFile(root / "TbNamed.TB.raw");
  writeTestFile(root / "TbNamed.TB_MTB.raw");
  EXPECT_TRUE(throwsWithMessage(
      [&] { (void)TraceRunDiscovery::resolveInput(inputConfig(tbNamedConfig, TraceRunFormat::Unformatted)); },
      "multiple eligible raw trace inputs"));

  const auto namedPairConfig = root / "NamedPair.ctrace-run.yml";
  const auto namedInput = root / "NamedPair.TB_MTB.raw";
  const auto otherNamedInput = root / "NamedPair.TB_ETB.raw";
  writeTestFile(namedInput);
  writeTestFile(otherNamedInput);
  EXPECT_TRUE(throwsWithMessage(
      [&] { (void)TraceRunDiscovery::resolveInput(inputConfig(namedPairConfig, TraceRunFormat::Formatted)); },
      "multiple eligible raw trace inputs"));

  const auto nonRegularConfig = root / "NonRegular.ctrace-run.yml";
  const auto nonRegular = root / "NonRegular.SWO.raw";
  std::filesystem::create_directory(nonRegular);
  EXPECT_TRUE(throwsWithMessage([&] { (void)TraceRunDiscovery::resolveInput(inputConfig(nonRegularConfig)); },
                                "not a regular file"));
}

TEST(CtraceUnitTests, testTraceRunDiscoveryAcceptsRegularSymlinkAndRejectsDanglingSymlink)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-run-symlink-input-test");
  const auto& root = temporaryPath.createDirectory();
  writeTestFile(root / "target.raw", "trace");

  std::error_code error;
  const auto regularLink = root / "Linked.SWO.raw";
  std::filesystem::create_symlink("target.raw", regularLink, error);
  if (error) {
    GTEST_SKIP() << error.message();
  }
  {
    auto descriptor = TraceRunDiscovery::resolveInput(inputConfig(root / "Linked.ctrace-run.yml"));
    EXPECT_EQ(descriptor.path(), regularLink);
  }

  const auto danglingLink = root / "Dangling.SWO.raw";
  std::filesystem::create_symlink("missing.raw", danglingLink);
  EXPECT_TRUE(
      throwsWithMessage([&] { (void)TraceRunDiscovery::resolveInput(inputConfig(root / "Dangling.ctrace-run.yml")); },
                        "not a regular file"));
}

TEST(CtraceUnitTests, testTraceRunDiscoveryRejectsUnreadableInput)
{
  if (!TestPlatform::supports(TestPlatformCapability::PosixPermissions)) {
    GTEST_SKIP();
  }

  const TemporaryTestPath temporaryPath("ctrace-trace-run-unreadable-input-test");
  const auto& root = temporaryPath.createDirectory();
  const auto configFile = root / "Unreadable.ctrace-run.yml";
  const auto rawInput = root / "Unreadable.SWO.raw";
  writeTestFile(rawInput, "trace");
  std::filesystem::permissions(rawInput, std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
  const auto message =
      captureExceptionMessage([&] { (void)TraceRunDiscovery::resolveInput(inputConfig(configFile)); });
  std::filesystem::permissions(rawInput, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);

  ASSERT_TRUE(message.has_value());
  EXPECT_NE(message->find("not readable"), std::string::npos);
}
