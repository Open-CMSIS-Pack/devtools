/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CtfTestSupport.h"
#include "TestPath.h"
#include "TestPlatform.h"
#include "TestSupport.h"
#include "ctf/CtfBundleOutput.h"
#include "ctf/TraceCompassXmlOutput.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

/** @brief Creates one independent emitted stream with an optional graphical topic. */
static CtfMetadataModel xmlMetadata(std::uint8_t discriminator, std::optional<std::string> processor = std::nullopt,
                                    std::optional<CtfGraphicalTopic> topic = CtfGraphicalTopic::ProcessorState)
{
  auto topology = CtfTestSupport::legacyTopology(1000000U);
  topology.clockDomains.front().uuid = CtfTestSupport::testUuid(discriminator);
  topology.streams.front().processorName = std::move(processor);
  CtfMetadataModel metadata(CtfTestSupport::testUuid(), std::move(topology));
  if (topic.has_value()) {
    metadata.observeGraphicalTopic(CtfStreamClassId{0U}, *topic);
  }
  return metadata;
}

/** @brief Creates formatted routes that share a clock or use two independent clocks. */
static CtfMetadataModel routedXmlMetadata(bool independentClocks)
{
  CtfMetadataTopology topology{
      {{CtfClockDomainId{1U}, "first_clock", CtfTestSupport::testUuid(1U), 1000000U, false}},
      {
          {CtfStreamClassId{1U}, {TraceRouteId{10U}, 1U}, "core-one", CtfClockDomainId{1U}},
          {CtfStreamClassId{111U}, {TraceRouteId{20U}, 111U}, std::nullopt, CtfClockDomainId{1U}},
      },
      {},
  };
  if (independentClocks) {
    topology.clockDomains.push_back({CtfClockDomainId{2U}, "second_clock", CtfTestSupport::testUuid(2U), 1000000U, false});
    topology.streams.back().clockDomainId = CtfClockDomainId{2U};
  }
  CtfMetadataModel metadata(CtfTestSupport::testUuid(), std::move(topology));
  metadata.observeGraphicalTopic(CtfStreamClassId{1U}, CtfGraphicalTopic::DwtValue);
  metadata.observeGraphicalTopic(CtfStreamClassId{111U}, CtfGraphicalTopic::DwtAddress);
  return metadata;
}

TEST(CtraceUnitTests, testTraceCompassXmlOutputCombinesIndependentChannels)
{
  const TemporaryTestPath root("ctrace-shared-xml-channels");
  const auto path = root.path() / "target.traceanalysis.xml";
  CollectingDiagnosticSink diagnostics;
  TraceCompassXmlOutput output(path, diagnostics);
  writeTestFile(path, "stale-xml");
  output.prepare();
  EXPECT_FALSE(std::filesystem::exists(path));
  output.add("SWO", xmlMetadata(1U, "core"));
  output.add("TB", xmlMetadata(2U, "core", CtfGraphicalTopic::Exception));
  output.add("TB_ETB", xmlMetadata(3U, std::string{}, CtfGraphicalTopic::DwtMatch));
  output.finish();

  const auto xml = readTestTextFile(path);
  EXPECT_NE(xml.find("Processor State - SWO - core"), std::string::npos);
  EXPECT_NE(xml.find("EXCEPTION - TB - core"), std::string::npos);
  EXPECT_NE(xml.find("DWT Match - TB_ETB"), std::string::npos);
  for (const auto discriminator : {1U, 2U, 3U}) {
    EXPECT_NE(xml.find(CtfTestSupport::testUuid(static_cast<std::uint8_t>(discriminator)).toString()), std::string::npos);
  }
  EXPECT_TRUE(diagnostics.events().empty());
}

TEST(CtraceUnitTests, testTraceCompassXmlOutputKeepsViewsSeparateWithinSharedClock)
{
  const TemporaryTestPath path("ctrace-shared-clock.xml");
  CollectingDiagnosticSink diagnostics;
  TraceCompassXmlOutput output(path.path(), diagnostics);
  output.prepare();
  output.add("TB", routedXmlMetadata(false));
  output.finish();
  const auto xml = readTestTextFile(path.path());
  EXPECT_NE(xml.find("DWT_VALUE - TB - core-one"), std::string::npos);
  EXPECT_NE(xml.find("DWT_ADDR - TB"), std::string::npos);
  EXPECT_EQ(xml.find("DWT_ADDR - TB - 111"), std::string::npos);
  EXPECT_EQ(xml.find("PROCESSOR_STATE"), std::string::npos);
  EXPECT_TRUE(diagnostics.events().empty());
}

TEST(CtraceUnitTests, testTraceCompassXmlOutputOmitsMultiClockInputButKeepsHealthySibling)
{
  const TemporaryTestPath path("ctrace-xml-multiple-clocks.xml");
  CollectingDiagnosticSink diagnostics;
  TraceCompassXmlOutput output(path.path(), diagnostics);
  output.prepare();
  output.add("TB", routedXmlMetadata(true));
  output.add("SWO", xmlMetadata(3U));
  output.finish();
  const auto xml = readTestTextFile(path.path());
  EXPECT_NE(xml.find("Processor State - SWO"), std::string::npos);
  EXPECT_EQ(xml.find("DWT_VALUE"), std::string::npos);
  EXPECT_EQ(diagnostics.events().size(), 1U);
  EXPECT_EQ(diagnostics.failureCount(), 0U);
  EXPECT_TRUE(diagnostics.containsContext("clockDomains", "2"));
  EXPECT_TRUE(diagnostics.containsContext("inputChannel", "TB"));
}

TEST(CtraceUnitTests, testTraceCompassXmlOutputOmitsEmptyAndPointOnlyInputs)
{
  const TemporaryTestPath path("ctrace-xml-no-views.xml");
  CollectingDiagnosticSink diagnostics;
  TraceCompassXmlOutput output(path.path(), diagnostics);
  writeTestFile(path.path(), "old-xml");
  output.prepare();
  output.add("TB", CtfMetadataModel(CtfTestSupport::testUuid(), {}));
  output.add("SWO", xmlMetadata(1U, std::nullopt, std::nullopt));
  output.finish();
  EXPECT_FALSE(std::filesystem::exists(path.path()));
  EXPECT_TRUE(diagnostics.events().empty());
}

TEST(CtraceUnitTests, testTraceCompassXmlOutputUsesOnlyEmittedStreams)
{
  const TemporaryTestPath path("ctrace-xml-emitted-streams.xml");
  CollectingDiagnosticSink diagnostics;
  TraceCompassXmlOutput output(path.path(), diagnostics);
  const auto metadata = routedXmlMetadata(true).projectToEmittedStreams({CtfStreamClassId{1U}});
  output.prepare();
  output.add("TB", metadata);
  output.finish();
  const auto xml = readTestTextFile(path.path());
  EXPECT_NE(xml.find("DWT_VALUE - TB - core-one"), std::string::npos);
  EXPECT_EQ(xml.find("DWT_ADDR"), std::string::npos);
  EXPECT_TRUE(diagnostics.events().empty());
}

TEST(CtraceUnitTests, testTraceCompassXmlOutputRequiresClockIdentityOnlyForViews)
{
  const TemporaryTestPath path("ctrace-xml-required-clock.xml");
  CollectingDiagnosticSink diagnostics;
  TraceCompassXmlOutput output(path.path(), diagnostics);
  CtfMetadataModel metadata(CtfTestSupport::testUuid(), CtfTestSupport::legacyTopology(1000000U));
  output.prepare();
  EXPECT_NO_THROW(output.add("SWO", metadata));
  metadata.observeGraphicalTopic(CtfStreamClassId{0U}, CtfGraphicalTopic::ProcessorState);
  EXPECT_THROW(output.add("SWO", metadata), std::invalid_argument);
  output.finish();
  EXPECT_FALSE(std::filesystem::exists(path.path()));
}

TEST(CtraceUnitTests, testTraceCompassXmlOutputRejectsClockReuseAcrossDifferentInputs)
{
  const TemporaryTestPath path("ctrace-xml-clock-reuse.xml");
  CollectingDiagnosticSink diagnostics;
  TraceCompassXmlOutput output(path.path(), diagnostics);
  output.prepare();
  output.add("SWO", xmlMetadata(1U));
  EXPECT_THROW(output.add("TB", routedXmlMetadata(false)), std::invalid_argument);
  EXPECT_THROW(output.add("TB_ETB", xmlMetadata(1U)), std::invalid_argument);
  output.finish();
  const auto xml = readTestTextFile(path.path());
  EXPECT_NE(xml.find("Processor State - SWO"), std::string::npos);
  EXPECT_EQ(xml.find("DWT_VALUE"), std::string::npos);
  EXPECT_EQ(xml.find("TB_ETB"), std::string::npos);
}

TEST(CtraceUnitTests, testTraceCompassXmlOutputPrepareResetsViewsAndClockIdentity)
{
  const TemporaryTestPath path("ctrace-xml-restart.xml");
  CollectingDiagnosticSink diagnostics;
  TraceCompassXmlOutput output(path.path(), diagnostics);
  output.prepare();
  output.add("SWO", xmlMetadata(1U));
  output.finish();
  output.prepare();
  EXPECT_FALSE(std::filesystem::exists(path.path()));
  output.add("TB", xmlMetadata(1U, "replacement", CtfGraphicalTopic::DwtEvent));
  output.finish();
  const auto xml = readTestTextFile(path.path());
  EXPECT_EQ(xml.find("PROCESSOR_STATE"), std::string::npos);
  EXPECT_NE(xml.find("DWT Event Counters - TB - replacement"), std::string::npos);
}

TEST(CtraceUnitTests, testTraceCompassXmlOutputSupportsUnicodeAndParentRelativePaths)
{
  const TemporaryTestPath root("ctrace-xml-unicode");
  std::filesystem::create_directories(root.path() / "working");
  const auto path = root.path() / "working" / ".." / std::filesystem::u8path(u8"Gr\u00f6\u00dfe") / "trace.xml";
  CollectingDiagnosticSink diagnostics;
  TraceCompassXmlOutput output(path, diagnostics);
  output.prepare();
  output.add("TB", xmlMetadata(1U));
  output.finish();
  EXPECT_TRUE(std::filesystem::is_regular_file(path));
}

TEST(CtraceUnitTests, testTraceCompassXmlOutputRejectsUnsafeTargets)
{
  CollectingDiagnosticSink diagnostics;
  for (const auto& path : {std::filesystem::path{}, std::filesystem::path("."), std::filesystem::path(".."),
                           std::filesystem::temp_directory_path().root_path()}) {
    EXPECT_THROW((void)TraceCompassXmlOutput(path, diagnostics), std::invalid_argument);
  }
}

TEST(CtraceUnitTests, testTraceCompassXmlOutputPreservesWrongTargetTypes)
{
  const TemporaryTestPath root("ctrace-xml-target-types");
  const auto directory = root.path() / "directory.xml";
  writeTestFile(directory / "marker", "keep");
  CollectingDiagnosticSink diagnostics;
  TraceCompassXmlOutput output(directory, diagnostics);
  EXPECT_THROW(output.prepare(), std::runtime_error);
  EXPECT_EQ(readTestTextFile(directory / "marker"), "keep");

  const auto parent = root.path() / "not-a-directory";
  writeTestFile(parent, "keep-parent");
  TraceCompassXmlOutput blocked(parent / "trace.xml", diagnostics);
  EXPECT_THROW(blocked.prepare(), std::runtime_error);
  EXPECT_EQ(readTestTextFile(parent), "keep-parent");

  TraceCompassXmlOutput longPath(root.path() / std::string(1024U, 'x'), diagnostics);
  // Windows may accept the missing path during prepare() and reject it only when writing.
  const auto generateXml = [&] {
    longPath.prepare();
    longPath.add("SWO", xmlMetadata(1U));
    longPath.finish();
  };
  EXPECT_THROW(generateXml(), std::runtime_error);
}

TEST(CtraceUnitTests, testTraceCompassXmlOutputReplacesSymlinksWithoutFollowingThem)
{
  const TemporaryTestPath root("ctrace-xml-symlink");
  const auto target = root.path() / "keep.xml";
  const auto path = root.path() / "output.xml";
  writeTestFile(target, "keep-target");
  std::error_code error;
  std::filesystem::create_symlink(target, path, error);
  if (error) {
    GTEST_SKIP() << error.message();
  }
  CollectingDiagnosticSink diagnostics;
  TraceCompassXmlOutput output(path, diagnostics);
  output.prepare();
  EXPECT_FALSE(std::filesystem::is_symlink(path));
  EXPECT_EQ(readTestTextFile(target), "keep-target");
  std::filesystem::create_symlink(target, path);
  output.add("SWO", xmlMetadata(1U));
  output.finish();
  EXPECT_FALSE(std::filesystem::is_symlink(path));
  EXPECT_TRUE(std::filesystem::is_regular_file(path));
  EXPECT_EQ(readTestTextFile(target), "keep-target");
}

TEST(CtraceUnitTests, testTraceCompassXmlOutputPermissionFailuresDoNotDeleteCtf)
{
  if (!TestPlatform::supports(TestPlatformCapability::PosixPermissions)) {
    GTEST_SKIP();
  }
  const TemporaryTestPath root("ctrace-xml-permissions");
  const auto parent = root.path() / "blocked";
  const auto path = parent / "trace.xml";
  const auto ctfDirectory = root.path() / "healthy.ctf";
  writeTestFile(path, "old-xml");
  CtfBundleOutput ctf(CtfOutputConfig(ctfDirectory, {}, CtfTestSupport::legacyTopology(1000000U)));
  ctf.start();
  ctf.writeEvent(softwarePacket(1U));
  ctf.stop();
  const auto metadataBefore = readTestTextFile(ctfDirectory / "metadata");
  CollectingDiagnosticSink diagnostics;
  TraceCompassXmlOutput output(path, diagnostics);
  const auto readOnly = std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec;
  std::filesystem::permissions(parent, readOnly);
  EXPECT_THROW(output.prepare(), std::runtime_error);
  std::filesystem::permissions(parent, std::filesystem::perms::owner_all);
  EXPECT_EQ(readTestTextFile(path), "old-xml");

  output.prepare();
  output.add("SWO", xmlMetadata(1U));
  std::filesystem::permissions(parent, readOnly);
  EXPECT_THROW(output.finish(), std::runtime_error);
  std::filesystem::permissions(parent, std::filesystem::perms::owner_all);
  EXPECT_FALSE(std::filesystem::exists(path));
  EXPECT_EQ(readTestTextFile(ctfDirectory / "metadata"), metadataBefore);
  EXPECT_TRUE(std::filesystem::is_regular_file(ctfDirectory / "stream_0"));
}

TEST(CtraceUnitTests, testTraceCompassXmlOutputReportsPseudoFilesystemWriteFailure)
{
  if (!TestPlatform::supports(TestPlatformCapability::LinuxSpecialFiles)) {
    GTEST_SKIP();
  }
  const auto path = TestPlatform::creationFailurePath("ctrace-xml-output.xml");
  CollectingDiagnosticSink diagnostics;
  TraceCompassXmlOutput output(path, diagnostics);
  output.prepare();
  output.add("SWO", xmlMetadata(1U));
  EXPECT_THROW(output.finish(), std::runtime_error);
  EXPECT_FALSE(std::filesystem::exists(path));
}
