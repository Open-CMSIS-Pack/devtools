/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CBuildIntegTestEnv.h"

#include "AuxCmd.h"

using namespace std;

class AuxCmdStub : public AuxCmd {
public:
  bool MkdirCmd(const list<string>& params) {
    return AuxCmd::MkdirCmd(params);
  }

  bool RmdirCmd(const list<string>& params, const string& except) {
    return AuxCmd::RmdirCmd(params, except);
  }

  bool TouchCmd(const list<string>& params) {
    return AuxCmd::TouchCmd(params);
  }
};

// Validate mkdir operation
TEST(AuxCmdTests, MkdirCmdTest)
{
  string cmd;
  int ret_val;
  list<string> directories = {
    testout_folder+"/AuxCmdTest/0",
    testout_folder+"/AuxCmdTest/1",
    testout_folder+"/AuxCmdTest/2/22",
  };
  AuxCmdStub auxcmd = AuxCmdStub();
  bool result = auxcmd.MkdirCmd(directories);
  ASSERT_EQ(result, true);

  // Assert that all directories exist
  for (const string& dir : directories) {
    cmd = SH + string(" \"[ -d ") + dir + string(" ]\"");
    ret_val = system(cmd.c_str());
    ASSERT_EQ(ret_val, 0);
  }
}

// Validate rmdir operation
TEST(AuxCmdTests, RmdirCmdTest)
{
  vector<string> directories = {
    testout_folder+"/AuxCmdTest/0",
    testout_folder+"/AuxCmdTest/1",
    testout_folder+"/AuxCmdTest/2/",
    testout_folder+"/AuxCmdTest/2/22/222",
  };
  string except = testout_folder+"/AuxCmdTest/0/00";

  // Create directories
  AuxCmdStub auxcmd = AuxCmdStub();
  bool result = auxcmd.MkdirCmd(list<string>{directories[0], directories[1], directories[2], directories[3], except});
  ASSERT_EQ(result, true);

  // Remove all base 'directories' but 'except'
  result = auxcmd.RmdirCmd(list<string>{directories[0], directories[1], directories[2]}, except);
  ASSERT_EQ(result, true);

  // Assert 'except' directory exist
  string cmd = SH + string(" \"[ -d ") + except + string(" ]\"");
  int ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0);

  // Assert other directories were removed
  cmd = SH + string(" \"[ ! -d ") + directories[1] + string(" ]\"");
  ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0);

  cmd = SH + string(" \"[ ! -d ") + directories[2] + string(" ]\"");
  ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0);
}

// Validate touch operation
TEST(AuxCmdTests, TouchCmdTest)
{
  string file = testout_folder+"/AuxCmdTest/Timestamp.txt";

  // Initialize: remove file if present, create directories if necessary
  error_code ec;
  fs::remove(file, ec);
  fs::create_directories(fs::path(file).parent_path());

  // Assert file does not exist
  string cmd = SH + string(" \"[ ! -f ") + file + string(" ]\"");
  int ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0);

  // Create file
  AuxCmdStub auxcmd = AuxCmdStub();
  bool result = auxcmd.TouchCmd(list<string>{file});
  ASSERT_EQ(result, true);

  // Assert file exists
  cmd = SH + string(" \"[ -f ") + file + string(" ]\"");
  ret_val = system(cmd.c_str());
  ASSERT_EQ(ret_val, 0);

  // Get timestamp1
  auto timestamp1 = fs::last_write_time(file, ec);

  // Touch file
  result = auxcmd.TouchCmd(list<string>{file});
  ASSERT_EQ(result, true);

  // Get timestamp2
  auto timestamp2 = fs::last_write_time(file, ec);

  // Assert timestamp was updated
  ASSERT_EQ(timestamp2 != timestamp1, true);
}
