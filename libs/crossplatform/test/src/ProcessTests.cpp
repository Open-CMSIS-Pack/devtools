/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ProcessRunner.h"

#include "gtest/gtest.h"

TEST(ProcessTests, Process_Run_Happy) {
  ProcessRunner process;
  EXPECT_TRUE(process.Run(TEST_EXE, std::list<std::string>{}));
  EXPECT_TRUE(process.HasStarted(5));
  EXPECT_TRUE(process.Kill());
  EXPECT_TRUE(process.HasStopped(1));
}

TEST(ProcessTests, Process_Run_With_Args_Happy) {
  ProcessRunner process;
  EXPECT_TRUE(process.Run(TEST_EXE, std::list<std::string>{"arg1", "arg2"}));
  EXPECT_TRUE(process.HasStarted(1));
  EXPECT_TRUE(process.Kill());
  EXPECT_TRUE(process.HasStopped(1));
}

TEST(ProcessTests, Process_Run_With_SteamRead_Happy) {
  auto condition = [](const std::string msg) {
    return (std::string::npos != msg.find("Doing some task"));
  };

  ProcessRunner process(true);

  EXPECT_TRUE(process.Run(TEST_EXE, std::list<std::string>{"arg1"}));
  EXPECT_TRUE(process.HasStarted(1));
  EXPECT_TRUE(process.WaitForProcessOutput(condition, 5));
  EXPECT_TRUE(process.Kill());
  EXPECT_TRUE(process.HasStopped(1));
}

TEST(ProcessTests, Process_Run_MultipleTimes_Failed) {
  ProcessRunner process;
  EXPECT_TRUE(process.Run(TEST_EXE, std::list<std::string>{"arg1", "arg2"}));
  EXPECT_TRUE(process.HasStarted(1));

  EXPECT_FALSE(process.Run(TEST_EXE, std::list<std::string>{"arg1"}));
  EXPECT_TRUE(process.Kill());
  EXPECT_TRUE(process.HasStopped(1));
}

TEST(ProcessTests, Process_Run_ReadStderr) {
  auto condition = [](const std::string msg) {
    return (std::string::npos != msg.find("error: invalid arguments"));
  };

  ProcessRunner process(true);
  EXPECT_TRUE(process.Run(TEST_EXE,
    std::list<std::string>{"arg1", "arg2", "arg3", "arg4", "arg5"}));
  EXPECT_TRUE(process.WaitForProcessOutput(condition, 5));
  EXPECT_TRUE(process.HasStopped(1));
}
