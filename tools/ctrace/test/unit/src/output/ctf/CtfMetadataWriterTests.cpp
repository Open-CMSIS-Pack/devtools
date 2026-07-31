/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestPath.hpp"
#include "TestSupport.hpp"

#include <gtest/gtest.h>

#include "ctf/CtfMetadataWriter.hpp"
#include "ctf/TraceCompassXmlWriter.hpp"
#include "TraceOutputConfig.hpp"

#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <vector>

TEST(CtraceUnitTests, testCtfMetadataWriterEscapesAndDeduplicatesSourceLabels)
{
  const TemporaryTestPath path("ctrace-metadata-writer");
  std::filesystem::create_directories(path.path());
  const std::vector<ResolvedTraceSource> sources{
      {"itm", 1U, 1U, std::string("ITM3"), std::nullopt, "unsigned int", 4U},
      {"itm", 2U, 1U, std::string("ITM3_1"), std::nullopt, "unsigned int", 4U},
      {"itm", 3U, 1U, std::string("ITM3"), std::nullopt, "unsigned int", 4U},
      {"itm", 4U, 1U, std::string("line\rbreak"), std::nullopt, "unsigned int", 4U},
      {"itm", 5U, 1U, std::nullopt, std::nullopt, "unsigned int", 4U},
      {"future", 6U, 1U, std::string("ignored"), std::nullopt, "unsigned int", 4U},
      {"dwt", 0U, 1U, std::nullopt, std::numeric_limits<std::uint64_t>::max(), "unsigned int", 4U},
  };

  CtfMetadataWriter::write(path.path(), "00000000-0000-4000-8000-000000000000", 1000000U, sources, {});
  const auto metadata = readTestTextFile(path.path() / "metadata");
  EXPECT_NE(metadata.find("ITM3_2"), std::string::npos);
  EXPECT_NE(metadata.find("line\\rbreak"), std::string::npos);
}

TEST(CtraceUnitTests, testCtfMetadataWriterRejectsMissingOutputDirectory)
{
  const TemporaryTestPath path("ctrace-metadata-writer-missing");
  EXPECT_THROW(CtfMetadataWriter::write(path.path(), "uuid", 1U, {}, {}), std::runtime_error);
}

TEST(CtraceUnitTests, testTraceCompassXmlWriterRejectsDirectoryTarget)
{
  const TemporaryTestPath path("ctrace-trace-compass-directory-target");
  std::filesystem::create_directories(path.path());
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

#if defined(__linux__)
TEST(CtraceUnitTests, testCtfTextWritersReportDeviceWriteFailures)
{
  const TemporaryTestPath path("ctrace-metadata-device-failure");
  std::filesystem::create_directories(path.path());
  std::filesystem::create_symlink("/dev/full", path.path() / "metadata");
  EXPECT_THROW(CtfMetadataWriter::write(path.path(), "uuid", 1U, {}, {}), std::runtime_error);
  EXPECT_THROW(TraceCompassXmlWriter::writeFile("/dev/full"), std::runtime_error);
}
#endif
