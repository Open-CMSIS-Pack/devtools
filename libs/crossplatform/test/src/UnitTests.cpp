/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "CrossPlatformUtils.h"
#include "RteFsUtils.h"
#include "gtest/gtest.h"

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
  EXPECT_EQ(CrossPlatformUtils::GetCMSISPackRootDir().find((CrossPlatformUtils::GetEnv(DEFAULT_PACKROOTDEF))), 0);
  EXPECT_EQ(CrossPlatformUtils::GetDefaultCMSISPackRootDir().find((CrossPlatformUtils::GetEnv(DEFAULT_PACKROOTDEF))), 0);
#endif
  EXPECT_EQ(CrossPlatformUtils::GetCMSISPackRootDir(), CrossPlatformUtils::GetDefaultCMSISPackRootDir());
}

TEST(CrossPlatformUnitTests, GetExecutablePath) {
  std::error_code ec;
  std::string exePath = fs::canonical(CrossPlatformUtils::GetExecutablePath(ec)).generic_string();
  EXPECT_STREQ(TEST_BIN_PATH, exePath.c_str());
  EXPECT_EQ(ec.value(), 0);

}

TEST(CrossPlatformUnitTests, CanExecute) {
  EXPECT_TRUE(CrossPlatformUtils::CanExecute(TEST_BIN_PATH));
  std::string genFolder = std::string(GLOBAL_TEST_DIR) + std::string("/packs/ARM/RteTestGenerator/0.1.0/Generator with spaces/");
  std::string genExe = genFolder + ((CrossPlatformUtils::GetHostType() == "win") ? string("script.bat") : string("script.sh"));
  EXPECT_TRUE(CrossPlatformUtils::CanExecute(genExe));
  std::string noExe = genFolder + string("noexe.sh");
  EXPECT_FALSE(CrossPlatformUtils::CanExecute(noExe));
}

TEST(CrossPlatformUnitTests, GetRegistryString) {
  if (CrossPlatformUtils::GetHostType() == "win") {
    EXPECT_TRUE(CrossPlatformUtils::GetRegistryString("HKEY_CURRENT_USER\\DUMMY_KEY_\\DUMMY_VAL").empty());
    EXPECT_TRUE(CrossPlatformUtils::GetRegistryString("DUMMY_KEY_\\DUMMY_VAL").empty());

    EXPECT_TRUE(!CrossPlatformUtils::GetRegistryString("HKEY_CURRENT_USER\\Environment\\Temp").empty());
    EXPECT_TRUE(!CrossPlatformUtils::GetRegistryString("Environment\\Temp").empty());

    EXPECT_EQ(CrossPlatformUtils::GetRegistryString("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CommonFilesDir"), "C:\\Program Files\\Common Files");
    EXPECT_EQ(CrossPlatformUtils::GetRegistryString("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CommonFilesDir"), "C:\\Program Files\\Common Files");

    // fall-back to environment variable
    EXPECT_EQ(CrossPlatformUtils::GetRegistryString("PATH"), CrossPlatformUtils::GetEnv("PATH"));

  } else {
    EXPECT_EQ(CrossPlatformUtils::GetRegistryString("PATH"), CrossPlatformUtils::GetEnv("PATH"));
  }
}

TEST(CrossPlatformUnitTests, GetLongPathRegStatus) {
  CrossPlatformUtils::REG_STATUS status = CrossPlatformUtils::GetLongPathRegStatus();
  if (CrossPlatformUtils::GetHostType() == "win") {
    EXPECT_TRUE(CrossPlatformUtils::REG_STATUS::ENABLED == status || CrossPlatformUtils::REG_STATUS::DISABLED == status);
  }
  else {
    EXPECT_EQ(CrossPlatformUtils::REG_STATUS::NOT_SUPPORTED, status);
  }
}

TEST(CrossPlatformUnitTests, ExecCommand) {
  auto result = CrossPlatformUtils::ExecCommand("invalid command");
  EXPECT_EQ(false, (0 == result.second) ? true : false) << result.first;

  string testdir = "mkdir test dir";
  error_code ec;
  if (filesystem::exists(testdir)) {
    filesystem::remove(testdir);
  }
  result = CrossPlatformUtils::ExecCommand("mkdir \"" + testdir + "\"");
  EXPECT_TRUE(filesystem::exists(testdir));
  EXPECT_EQ(true, (0 == result.second) ? true : false) << result.first;
}

// end of UnitTests.cpp
