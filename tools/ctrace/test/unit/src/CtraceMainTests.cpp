/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtraceMain.h"
#include "TestPath.h"
#include "TestSupport.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

/** @brief Verifies that one generated output file exists and is not empty. */
static void expectNonEmptyFile(const std::filesystem::path& path)
{
  ASSERT_TRUE(std::filesystem::is_regular_file(path)) << path;
  EXPECT_GT(std::filesystem::file_size(path), 0U) << path;
}

TEST(CtraceUnitTests, testCtraceMainGeneratesAllOutputs)
{
  TemporaryTestPath temporary("ctrace-main-all-outputs-test");
  const auto& directory = temporary.createDirectory();
  writeTestFile(directory / "Minimal.ctrace-run.yml", R"yml(ctrace-run:
  ctrace-setup:
    - timestamps:
        clock: 400000000
  ctrace-refs: []
)yml");

  const std::string raw{"\0\0\0\0\0\x80\x09\x41", 8U};
  writeTestFile(directory / "Minimal.SWO.raw", raw);

  EXPECT_EQ(CtraceMain({"ctrace", directory.string(), "--target", "Minimal", "--all"}), 0);
  EXPECT_EQ(readTestTextFile(directory / "Minimal.SWO.csv"),
            "cycles,stream,type,source,value,pc,offset,note\n"
            "0,0,itm,1,0x41,,,\n");
  expectNonEmptyFile(directory / "Minimal.ctf" / "metadata");
  expectNonEmptyFile(directory / "Minimal.ctf" / "stream_0");
  expectNonEmptyFile(directory / "Minimal.SWO.traceanalysis.xml");
}
