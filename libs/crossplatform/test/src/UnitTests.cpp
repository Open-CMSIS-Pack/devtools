/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "gtest/gtest.h"
#include "CrossPlatformUtils.h"

using namespace std;


TEST(CrossPlatformUnitTests, GetEnv_Empty) {
  string value = CrossPlatformUtils::GetEnv("");
  EXPECT_TRUE(value.empty());
}

TEST(CrossPlatformUnitTests, GetEnv_Existing) {
  string value = CrossPlatformUtils::GetEnv("PATH"); // present always in our test environment
  EXPECT_FALSE(value.empty());
}

TEST(CrossPlatformUnitTests, GetEnv_NoExising) {
  string value = CrossPlatformUtils::GetEnv("DUMMY_ENV_VAR_NON_EXISTING");
  EXPECT_TRUE(value.empty());
}

TEST(CrossPlatformUnitTests, SetEnv_EmptyVar) {
  EXPECT_FALSE(CrossPlatformUtils::SetEnv("", "DUMMY_ENV_VAR_VALUE"));
}


TEST(CrossPlatformUnitTests, SetGetEnv_NonEmpty) {
  string value = "non_empty";
  EXPECT_TRUE(CrossPlatformUtils::SetEnv("DUMMY_ENV_VAR", value));
  EXPECT_EQ(CrossPlatformUtils::GetEnv("DUMMY_ENV_VAR"), value);
}

TEST(CrossPlatformUnitTests, SetGetEnv_Empty) {
  string value;
  EXPECT_TRUE(CrossPlatformUtils::SetEnv("DUMMY_ENV_VAR", value));
  EXPECT_TRUE(CrossPlatformUtils::GetEnv("DUMMY_ENV_VAR").empty());
}


TEST(CrossPlatformUnitTests, GetPackRootDir_ValidEnvSet) {
  string value = "packrootpath";
  EXPECT_TRUE(CrossPlatformUtils::SetEnv("CMSIS_PACK_ROOT", value));
  EXPECT_EQ(CrossPlatformUtils::GetEnv("CMSIS_PACK_ROOT"), value);
  EXPECT_EQ(CrossPlatformUtils::GetCMSISPackRootDir(), value);
}

TEST(CrossPlatformUnitTests, GetPackRootDir_NoEnvSet) {
  EXPECT_TRUE(CrossPlatformUtils::SetEnv("CMSIS_PACK_ROOT", ""));
  EXPECT_EQ(CrossPlatformUtils::GetCMSISPackRootDir(), CrossPlatformUtils::GetDefaultCMSISPackRootDir());
}

TEST(CrossPlatformUnitTests, GetPackRootDir_Default) {
  EXPECT_TRUE(CrossPlatformUtils::SetEnv("CMSIS_PACK_ROOT", ""));
#ifdef DEFAULT_PACKROOTDEF
  EXPECT_EQ(CrossPlatformUtils::GetCMSISPackRootDir(), DEFAULT_PACKROOTDEF);
  EXPECT_EQ(CrossPlatformUtils::GetDefaultCMSISPackRootDir(), DEFAULT_PACKROOTDEF);
#endif
  EXPECT_EQ(CrossPlatformUtils::GetCMSISPackRootDir(), CrossPlatformUtils::GetDefaultCMSISPackRootDir());
}

// end of UnitTests.cpp
