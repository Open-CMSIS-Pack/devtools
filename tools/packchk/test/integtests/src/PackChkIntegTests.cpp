/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "PackChkIntegTestEnv.h"

#include "PackChk.h"
#include "ErrLog.h"

#include <fstream>

using namespace std;

class PackChkIntegTests : public ::testing::Test {
public:
  void SetUp()    override;
  void TearDown() override;
};

void PackChkIntegTests::SetUp() {
}

void PackChkIntegTests::TearDown() {
  ErrLog::Get()->ClearLogMessages();
}

 // Validate packchk when no .pdsc file found
TEST_F(PackChkIntegTests, FileNotAVailable) {
  const char* argv[2];

  argv[0] = (char*)"";
  argv[1] = (char*)"UNKNOWN.FILE.pdsc";

  PackChk packChk;
  EXPECT_EQ(1, packChk.Check(2, argv, nullptr));
}

// Validate PackChk with invalid arguments
TEST_F(PackChkIntegTests, InvalidArguments) {
  const char *argv[3];

  const string& pdscFile = PackChkIntegTestEnv::globaltestdata_dir +
    "/packs/ARM/RteTest/0.1.0/ARM.RteTest.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*) pdscFile.c_str();
  argv[2] = (char*) "--invalid";

  PackChk packChk;
  EXPECT_EQ(1, packChk.Check(3, argv, nullptr));
}

// Validate software pack
TEST_F(PackChkIntegTests, CheckValidPack) {
  const char* argv[2];

  const string& pdscFile = PackChkIntegTestEnv::globaltestdata_dir +
    "/packs/ARM/RteTest/0.1.0/ARM.RteTest.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();

  PackChk packChk;
  EXPECT_EQ(0, packChk.Check(2, argv, nullptr));
}

// Validate invalid software pack
TEST_F(PackChkIntegTests, CheckInvalidPack) {
  const char* argv[2];

  const string& pdscFile = PackChkIntegTestEnv::localtestdata_dir +
    "/InvalidPack/TestVendor.TestInvalidPack.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();

  PackChk packChk;
  EXPECT_EQ(1, packChk.Check(2, argv, nullptr));
}

// Validate software pack with component dependencies
TEST_F(PackChkIntegTests, CheckComponentDependency) {
  const char* argv[8];

  const string& pdscFile = PackChkIntegTestEnv::globaltestdata_dir +
    "/packs/ARM/RteTest/0.1.0/ARM.RteTest.pdsc";
  const string& refFile = PackChkIntegTestEnv::globaltestdata_dir +
    "/packs/ARM/RteTest_DFP/0.1.1/ARM.RteTest_DFP.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));
  ASSERT_TRUE(RteFsUtils::Exists(refFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char*)"-i";
  argv[3] = (char*)refFile.c_str();
  argv[4] = (char*)"-x";
  argv[5] = (char*)"M334";
  argv[6] = (char*)"M324";
  argv[7] = (char*)"M362";

  PackChk packChk;
  EXPECT_EQ(0, packChk.Check(8, argv, nullptr));

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  for (const string& msg : errMsgs) {
    size_t s;
    if ((s = msg.find("M382", 0)) != string::npos) {
      FAIL() << "error: contains warning M382";
    }
  }
}

// Check generation of pack file name
TEST_F(PackChkIntegTests, WritePackFileName) {
  const char* argv[4];
  ifstream file;

  const string& pdscFile = PackChkIntegTestEnv::globaltestdata_dir +
    "/packs/ARM/RteTest/0.1.0/ARM.RteTest.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  string line, outFile = PackChkIntegTestEnv::testoutput_dir + "/packname.txt";
  if (RteFsUtils::Exists(outFile)) {
    RteFsUtils::RemoveFile(outFile);
  }

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char*)"-n";
  argv[3] = (char*)outFile.c_str();

  PackChk packChk;
  EXPECT_EQ(0, packChk.Check(4, argv, nullptr));
  EXPECT_TRUE(RteFsUtils::Exists(outFile));

  // Check output file content
  file.open(outFile);
  ASSERT_EQ(true, file.is_open()) << "Failed to open " << outFile;

  getline(file, line);
  EXPECT_EQ(line, "ARM.RteTest.0.1.0.pack");

  file.close();
}

// Verify that the specified URL matches the <url> element in the *.pdsc file
TEST_F(PackChkIntegTests, CheckPackServerURL) {
  const char* argv[4];

  const string& pdscFile = PackChkIntegTestEnv::globaltestdata_dir +
    "/packs/ARM/RteTest/0.1.0/ARM.RteTest.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char*)"-u";
  argv[3] = (char*)"www.keil.com/pack/";

  PackChk packChk;
  EXPECT_EQ(0, packChk.Check(4, argv, nullptr));
}

// Suppress all listed validation messages
TEST_F(PackChkIntegTests, SuppressValidationMsgs) {
  const char* argv[6];

  const string& pdscFile = PackChkIntegTestEnv::globaltestdata_dir +
    "/packs/ARM/RteTest/0.1.0/ARM.RteTest.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char*)"-x";
  argv[3] = (char*)"M382";
  argv[4] = (char*)"-x";
  argv[5] = (char*)"M324";

  PackChk packChk;
  EXPECT_EQ(0, packChk.Check(6, argv, nullptr));

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  for (const string& msg : errMsgs) {
    size_t s;
    if ((s = msg.find("M382")) != string::npos) {

      FAIL() << "error: contains warning M382";
    }

    if ((s = msg.find("M324")) != string::npos) {
      FAIL() << "error: contains warning M324";
    }
  }
}
