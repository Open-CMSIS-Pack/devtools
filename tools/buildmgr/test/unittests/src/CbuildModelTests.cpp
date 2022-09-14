/*
 * Copyright (c) 2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildUnitTestEnv.h"

#include "CbuildModel.h"

using namespace std;

class CbuildModelTests : public CbuildModel, public ::testing::Test {
};

TEST_F(CbuildModelTests, GetAccessSequence) {
  string src, sequence;
  size_t offset = 0;

  src = "$Bpack$";
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, 7);
  EXPECT_EQ(sequence, "Bpack");
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, string::npos);

  src = "$Pack(vendor.name)$";
  offset = 0;
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, 19);
  EXPECT_EQ(sequence, "Pack(vendor.name)");
  offset = 0;
  EXPECT_TRUE(GetAccessSequence(offset, sequence, sequence, '(', ')'));
  EXPECT_EQ(offset, 17);
  EXPECT_EQ(sequence, "vendor.name");

  src = "$Dpack";
  offset = 0;
  EXPECT_FALSE(GetAccessSequence(offset, src, sequence, '$', '$'));
}


TEST_F(CbuildModelTests, GetCompatibleToolchain_Select_Latest) {
  string ext = ".cmake", name = "AC6", versionRange = "6.5.0:6.18.0", dir;
  string toolchainDir = testinput_folder + "/toolchain";
  string expectedToolchainVersion = "6.16.0";
  string expectedToolchainConfig = toolchainDir + "/AC6.6.16.0.cmake";
  RteFsUtils::NormalizePath(expectedToolchainConfig);

  // Select latest compatible toolchain
  ASSERT_TRUE(RteFsUtils::CreateDirectories(toolchainDir));
  ASSERT_TRUE(RteFsUtils::CreateFile(toolchainDir + "/AC6.6.6.4.cmake", ""));
  ASSERT_TRUE(RteFsUtils::CreateFile(toolchainDir + "/AC6.6.16.0.cmake", ""));
  ASSERT_TRUE(RteFsUtils::CreateFile(toolchainDir + "/GCC.6.19.0.cmake", ""));
  EXPECT_TRUE(GetCompatibleToolchain(name, versionRange, toolchainDir, ext));
  EXPECT_EQ(m_toolchainConfigVersion, expectedToolchainVersion);
  EXPECT_EQ(m_toolchainConfig, expectedToolchainConfig);
  ASSERT_TRUE(RteFsUtils::RemoveDir(toolchainDir));
}

TEST_F(CbuildModelTests, GetCompatibleToolchain_Failed) {
  string ext = ".cmake", name = "AC6", versionRange = "6.5.0:6.18.0", dir;
  string toolchainDir = testinput_folder + "/toolchain";
  // Toolchain not found
  ASSERT_TRUE(RteFsUtils::CreateDirectories(toolchainDir));
  EXPECT_FALSE(GetCompatibleToolchain(name, versionRange, toolchainDir, ext));
  EXPECT_EQ(m_toolchainConfigVersion, RteUtils::EMPTY_STRING);
  EXPECT_EQ(m_toolchainConfig, RteUtils::EMPTY_STRING);
  ASSERT_TRUE(RteFsUtils::RemoveDir(toolchainDir));
}

TEST_F(CbuildModelTests, GetCompatibleToolchain_Invalid_Files) {
  string ext = ".cmake", name = "AC6", versionRange = "6.5.0:6.18.0", dir;
  string toolchainDir = testinput_folder + "/toolchain";
  // No .cmake file found
  ASSERT_TRUE(RteFsUtils::CreateDirectories(toolchainDir));
  ASSERT_TRUE(RteFsUtils::CreateFile(toolchainDir + "/test.info", ""));
  ASSERT_TRUE(RteFsUtils::CreateFile(toolchainDir + "/AC6.info", ""));
  EXPECT_FALSE(GetCompatibleToolchain(name, versionRange, toolchainDir, ext));
  EXPECT_EQ(m_toolchainConfigVersion, RteUtils::EMPTY_STRING);
  EXPECT_EQ(m_toolchainConfig, RteUtils::EMPTY_STRING);
  ASSERT_TRUE(RteFsUtils::RemoveDir(toolchainDir));
}

TEST_F(CbuildModelTests, GetCompatibleToolchain_Select_Latest_recursively) {
  string ext = ".cmake", name = "AC6", versionRange = "6.5.0:6.18.0", dir;
  string toolchainDir = testinput_folder + "/toolchain";
  string expectedToolchainVersion = "6.16.0";
  string expectedToolchainConfig = toolchainDir + "/Test/AC6.6.16.0.cmake";
  RteFsUtils::NormalizePath(expectedToolchainConfig);

  // Select latest compatible toolchain
  ASSERT_TRUE(RteFsUtils::CreateDirectories(toolchainDir+"/Test"));
  ASSERT_TRUE(RteFsUtils::CreateFile(toolchainDir + "/Test/AC6.6.6.4.cmake", ""));
  ASSERT_TRUE(RteFsUtils::CreateFile(toolchainDir + "/Test/AC6.6.16.0.cmake", ""));
  ASSERT_TRUE(RteFsUtils::CreateFile(toolchainDir + "/Test/GCC.6.19.0.cmake", ""));
  EXPECT_TRUE(GetCompatibleToolchain(name, versionRange, toolchainDir, ext));
  EXPECT_EQ(m_toolchainConfigVersion, expectedToolchainVersion);
  EXPECT_EQ(m_toolchainConfig, expectedToolchainConfig);
  ASSERT_TRUE(RteFsUtils::RemoveDir(toolchainDir));
}

TEST_F(CbuildModelTests, EvalItemTranslationControls) {
  RteItem groupItem(nullptr);
  groupItem.SetTag("group");
  groupItem.SetAttribute("name", "engine");
  RteItem* optionsItem = groupItem.CreateChild("options");
  optionsItem->SetAttribute("optimize", "speed");

  EvalItemTranslationControls(&groupItem, OPTIONS);
  EXPECT_EQ("speed", m_optimize["/engine"]);
}
