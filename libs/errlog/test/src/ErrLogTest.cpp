/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gtest/gtest.h"
#include "ErrLog.h"
#include "ErrOutputterSaveToStdoutOrFile.h"

#include "RteFsUtils.h"

#include <vector>
#include <list>
#include <string>

using namespace std;

// Directories
const string dirnameBase = "ErrLogTest";
const string dirnameLogs = dirnameBase + "/logs";

// Filenames
const string filenameLog = dirnameLogs + "/file.log";

class ErrLogTest :public ::testing::Test {
protected:
  void SetUp()   override
  {
    if(!ErrLog::Get()->GetOutputter()) {
      ErrLog::Get()->SetOutputter(new ErrOutputterSaveToStdoutOrFile());
    }
    RteFsUtils::CreateDirectories(dirnameLogs);
  }
  void TearDown() override
  {
    RteFsUtils::DeleteTree(dirnameBase);
  }

  void CompareMessages(const list<string>& messages, const list<string>& expectedMessages) {
    auto it = messages.begin();
    auto ite = expectedMessages.begin();
    EXPECT_EQ(true, messages.size() == expectedMessages.size());
    for (; it != messages.end() && ite != expectedMessages.end(); it++, ite++) {
      EXPECT_EQ(true, *it == *ite);
    }
  }
};


TEST_F(ErrLogTest, Message) {
  static const list<string> testMessages = {
    "\n",
    "*** CRITICAL ERROR M999:",
    " Message.test",
    " (Line 4) ",
    "\n  ",
    "Log Message not found in Messages Table",
    "\n",
    "\n",
    "<TEXT>",
    " (Line 1)",
    "\n",
    "device",
    " (Line 9)",
    "\n",
    "substitute1substitute2",
    " (Line 14)"
  };


  ErrLog::Get()->ClearLogMessages();

  std::string fileName = "Message.test";
  ErrLog::Get()->SetFileName (fileName);
  ErrLog::Get()->SetLogFileName(filenameLog);

  LogMsg           ("M999",  4, 0);
  LogMsg           ("M001",  1, 0);
  LogMsg           ("M001", VAL("TEXT", "device"),  9, 0);
  LogMsg           ("M002", VAL("TEXT", "substitute1"), VAL("TEXT2", "substitute2"), 14, 0);
  CompareMessages(ErrLog::Get()->GetLogMessages(), testMessages);
  ErrLog::Get()->Save();
  ErrLog::Get()->ClearLogMessages();
  EXPECT_EQ( true, ErrLog::Get()->GetErrCnt() == 0);
}

TEST_F(ErrLogTest, PdscMessage) {
  static const list<string> testMessages = {
    "\n",
    "\n",
    "*** ERROR M017:",
    " PdscMessage.test",
    " (Line 17) ",
    "\n  ",
    "An Error Message ( test_message_substitute ) cannot be suppressed."
  };

  ErrLog::Get()->ClearLogMessages();

  std::string fileName = "PdscMessage.test";
  ErrLog::Get()->SetFileName (fileName);

  PdscMsg msg;
  msg.SetMsg        ("M017", 17, 0);                // <%NODE1%> inisde <%NODE2%> is not a child of <%NODE3%> node.
  msg.AddSubstitude ("MSG", " test_message_substitute ");
  ErrLog::Get()->Message (msg);

  CompareMessages(ErrLog::Get()->GetLogMessages(), testMessages);
  ErrLog::Get()->Save();
  ErrLog::Get()->ClearLogMessages();
}
