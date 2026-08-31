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

#include "ctf/CtfMetadataWriter.h"
#include "ctf/CtfSchema.h"
#include "ctf/TraceCompassXmlWriter.h"
#include "TraceOutputConfig.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

TEST(CtraceUnitTests, testCtfMetadataWriterEscapesAndDeduplicatesSourceLabels)
{
  const TemporaryTestPath path("ctrace-metadata-writer");
  path.createDirectory();
  const std::vector<ResolvedTraceSource> sources{
      {"itm", 1U, 1U, std::string("ITM3"), std::nullopt, "unsigned", 4U},
      {"itm", 2U, 1U, std::string("ITM3_1"), std::nullopt, "unsigned", 4U},
      {"itm", 3U, 1U, std::string("ITM3"), std::nullopt, "unsigned", 4U},
      {"itm", 4U, 1U, std::string("line\rbreak"), std::nullopt, "unsigned", 4U},
      {"itm", 5U, 1U, std::nullopt, std::nullopt, "unsigned", 4U},
      {"itm", 6U, 1U, std::string("ITM3"), std::nullopt, "unsigned", 4U},
      {"future", 7U, 1U, std::string("ignored"), std::nullopt, "unsigned", 4U},
      {"dwt", 0U, 1U, std::nullopt, std::numeric_limits<std::uint64_t>::max(), "unsigned", 4U},
  };

  CtfMetadataWriter::write(path.path(), "00000000-0000-4000-8000-000000000000", 1000000U, sources,
                           {8U, 10U, 13U, 16U, 54U});
  const auto metadata = readTestTextFile(path.path() / "metadata");
  EXPECT_NE(metadata.find("ITM3_2"), std::string::npos);
  EXPECT_NE(metadata.find("\"ITM6\" = 6"), std::string::npos);
  EXPECT_NE(metadata.find("line\\rbreak"), std::string::npos);
  EXPECT_NE(metadata.find("\"Reserved 8\" = 8"), std::string::npos);
  EXPECT_NE(metadata.find("\"Reserved 10\" = 10"), std::string::npos);
  EXPECT_NE(metadata.find("\"Reserved 13\" = 13"), std::string::npos);
  EXPECT_NE(metadata.find("\"External IRQ 0\" = 16"), std::string::npos);
  EXPECT_NE(metadata.find("\"External IRQ 38\" = 54"), std::string::npos);
}

TEST(CtraceUnitTests, testCtfMetadataWriterRejectsMissingOutputDirectory)
{
  const TemporaryTestPath path("ctrace-metadata-writer-missing");
  EXPECT_THROW(CtfMetadataWriter::write(path.path(), "uuid", 1U, {}, {}), std::runtime_error);
}

TEST(CtraceUnitTests, testTraceCompassXmlWriterRejectsDirectoryTarget)
{
  const TemporaryTestPath path("ctrace-trace-compass-directory-target");
  path.createDirectory();
  EXPECT_THROW(TraceCompassXmlWriter::writeFile(path.path()), std::runtime_error);
}

TEST(CtraceUnitTests, testTraceCompassXmlWriterSupportsParentlessTarget)
{
  const auto path = std::filesystem::path("ctrace-parentless-trace-compass.xml");
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
  EXPECT_NO_THROW(TraceCompassXmlWriter::writeFile(path));
  EXPECT_TRUE(std::filesystem::is_regular_file(path));
  std::filesystem::remove(path, ignored);
}

TEST(CtraceUnitTests, testTraceCompassXmlUsesCurrentCtfEvents)
{
  const TemporaryTestPath path("ctrace-trace-compass-schema.xml");
  TraceCompassXmlWriter::writeFile(path.path());
  const auto xml = readTestTextFile(path.path());
  constexpr std::array<CtfSchema::EventId, 5U> visualizedEvents{
      CtfSchema::EventId::Itm,       CtfSchema::EventId::DwtValue,    CtfSchema::EventId::DwtAddress,
      CtfSchema::EventId::Exception, CtfSchema::EventId::TraceStatus,
  };

  for (const auto eventId : visualizedEvents) {
    EXPECT_NE(xml.find("eventName=\"" + std::string(CtfSchema::eventName(eventId)) + "\""), std::string::npos);
  }
}

TEST(CtraceUnitTests, testCtfTextWritersReportDeviceWriteFailures)
{
  if (!TestPlatform::supports(TestPlatformCapability::LinuxSpecialFiles)) {
    GTEST_SKIP();
  }
  const TemporaryTestPath path("ctrace-metadata-device-failure");
  path.createDirectory();
  std::filesystem::create_symlink(TestPlatform::writeFailurePath(), path.path() / "metadata");
  EXPECT_THROW(CtfMetadataWriter::write(path.path(), "uuid", 1U, {}, {}), std::runtime_error);
  EXPECT_THROW(TraceCompassXmlWriter::writeFile(TestPlatform::writeFailurePath()), std::runtime_error);
}
