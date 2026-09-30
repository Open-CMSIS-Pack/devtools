/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "DiagnosticMessages.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

TEST(CtraceUnitTests, testDiagnosticLocationPreservesOpaqueDetail)
{
  EXPECT_EQ(locatedDiagnosticMessage("input.yml", 0U, "native: \"error\""),
            "input.yml: native: \"error\"");
  EXPECT_EQ(locatedDiagnosticMessage("input.yml", 42U, "native: \"error\""),
            "input.yml(42): native: \"error\"");
  EXPECT_EQ(yamlParseMessage("input.yml", std::nullopt, std::nullopt, "native: detail"),
            "failed to parse trace-run configuration: input.yml: native: detail");
  EXPECT_EQ(yamlParseMessage("input.yml", 42U, std::nullopt, "native: detail"),
            "failed to parse trace-run configuration: input.yml(42): native: detail");
  EXPECT_EQ(yamlParseMessage("input.yml", 42U, 7U, "native: detail"),
            "failed to parse trace-run configuration: input.yml(42,7): native: detail");
  EXPECT_EQ(yamlParseMessage("input.yml", std::nullopt, 7U, "native: detail"),
            "failed to parse trace-run configuration: input.yml: native: detail");
}

TEST(CtraceUnitTests, testDiagnosticBackendPreservesEmptyAndExternalDetail)
{
  EXPECT_EQ(pathDiagnosticMessage(PathDiagnosticCode::CsvWrite, "a.csv"), "Failed to write CSV output a.csv");
  EXPECT_EQ(pathDiagnosticMessage(PathDiagnosticCode::CsvInspect, "a.csv", "permission denied"),
            "Failed to inspect existing CSV output a.csv: permission denied");
  EXPECT_EQ(pathDiagnosticMessage(PathDiagnosticCode::CsvInspect, "a.csv", ""),
            "Failed to inspect existing CSV output a.csv: ");
  EXPECT_EQ(backendFailureMessage("csv", "", "start", "foreign: detail"),
            "csv output failed during start: foreign: detail");
  EXPECT_EQ(backendFailureMessage("csv", "file name.csv", "write", "foreign: detail"),
            "csv output 'file name.csv' failed during write: foreign: detail");
}

TEST(CtraceUnitTests, testOperationalDiagnosticRetains64BitValues)
{
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  EXPECT_EQ(rawInputAlignmentMessage(16U, "capture.raw", maximum),
            "formatted raw trace input size must be a multiple of 16 bytes: "
            "capture.raw (size=18446744073709551615)");
  EXPECT_EQ(unknownNormalizedRouteMessage(maximum),
            "OpenCSD element references unknown normalized route 18446744073709551615");
  EXPECT_EQ(ctfDataSizeMessage(maximum, "float"),
            "CTF output cannot use ctrace-run size 18446744073709551615 with data-type 'float'; "
            "supported data-type values are 'unsigned', 'signed', and 'float'; "
            "size must be 1, 2, or 4, and float requires size 4");
}

TEST(CtraceUnitTests, testDecodeSummaryRetainsPrecisionAndZeroDuration)
{
  EXPECT_EQ(decodeSummaryMessage(0U, 0.0, 3U),
            "processed 0 input bytes in 0.000 s (0.00 MiB/s); trace/diagnostic records: 3");
  EXPECT_EQ(decodeSummaryMessage(1048576U, 2.0, 4U),
            "processed 1048576 input bytes in 2.000 s (0.50 MiB/s); trace/diagnostic records: 4");
}
