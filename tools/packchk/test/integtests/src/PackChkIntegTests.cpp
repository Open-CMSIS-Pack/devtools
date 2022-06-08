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

TEST_F(PackChkIntegTests, AddRefPacks) {
  const char* argv[10];

  string outDir = PackChkIntegTestEnv::testoutput_dir + "/Packs";
  ASSERT_TRUE(RteFsUtils::CreateDirectories(outDir));

  string refNameTest = "TestPack";
  string refName1 = "RefPack1";
  string refName2 = "RefPack2";
  string refName3 = "RefPack3";
  string refName4 = "RefPack4";

  string refPackTest = "/" + refNameTest;
  string refPack1 = "/" + refName1;
  string refPack2 = "/" + refName2;
  string refPack3 = "/" + refName3;
  string refPack4 = "/" + refName4;
  ASSERT_TRUE(RteFsUtils::CreateDirectories(outDir + refPackTest));
  ASSERT_TRUE(RteFsUtils::CreateDirectories(outDir + refPack1));
  ASSERT_TRUE(RteFsUtils::CreateDirectories(outDir + refPack2));
  ASSERT_TRUE(RteFsUtils::CreateDirectories(outDir + refPack3));
  ASSERT_TRUE(RteFsUtils::CreateDirectories(outDir + refPack4));

  refPackTest += "/" + refNameTest + ".pdsc";
  refPack1 += "/" + refName1 + ".pdsc";
  refPack2 += "/" + refName2 + ".pdsc";
  refPack3 += "/" + refName3 + ".pdsc";
  refPack4 += "/" + refName4 + ".pdsc";
  const string contentBegin = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"\
                              "<package schemaVersion=\"1.3\" xmlns:xs=\"http://www.w3.org/2001/XMLSchema-instance\" xs:noNamespaceSchemaLocation=\"PACK.xsd\">\n"\
                              "  <name>";    
  const string contentEnd =   "  </name>\n"\
                              "</package>\n";

  refPackTest = outDir + refPackTest;
  refPack1 = outDir + refPack1;
  refPack2 = outDir + refPack2;
  refPack3 = outDir + refPack3;
  refPack4 = outDir + refPack4;

  ASSERT_TRUE(RteFsUtils::CreateFile(refPackTest, contentBegin + refNameTest + contentEnd));
  ASSERT_TRUE(RteFsUtils::CreateFile(refPack1, contentBegin + refName1 + contentEnd));
  ASSERT_TRUE(RteFsUtils::CreateFile(refPack2, contentBegin + refName2 + contentEnd));
  ASSERT_TRUE(RteFsUtils::CreateFile(refPack3, contentBegin + refName3 + contentEnd));
  ASSERT_TRUE(RteFsUtils::CreateFile(refPack4, contentBegin + refName4 + contentEnd));

  ASSERT_TRUE(RteFsUtils::Exists(refPackTest));
  ASSERT_TRUE(RteFsUtils::Exists(refPack1));
  ASSERT_TRUE(RteFsUtils::Exists(refPack2));
  ASSERT_TRUE(RteFsUtils::Exists(refPack3));
  ASSERT_TRUE(RteFsUtils::Exists(refPack4));

  argv[0] = (char*)"";
  argv[1] = (char*)refPackTest.c_str();
  argv[2] = (char*)"-i";
  argv[3] = (char*)refPack1.c_str();
  argv[4] = (char*)"-i";
  argv[5] = (char*)refPack2.c_str();
  argv[6] = (char*)"-i";
  argv[7] = (char*)refPack3.c_str();
  argv[8] = (char*)"-i";
  argv[9] = (char*)refPack4.c_str();

  PackChk packChk;
  packChk.Check(10, argv, nullptr);
  
  const RteGlobalModel& model = packChk.GetModel();
  const RtePackageMap& packs = model.GetPackages();

  for(auto& [name, pack] : packs) {
    if(name != refNameTest && name != refName1 && name != refName2 && name != refName3 && name != refName4) {
      FAIL() << "RefPack was not added";
    }
  }

}

// Validate software pack with directory starting by .
TEST_F(PackChkIntegTests, CheckPackWithDot) {
  const char* argv[2];

  const string& pdscFile = PackChkIntegTestEnv::localtestdata_dir +
    "/TestPackDot/TestVendor.TestPackDot.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();

  PackChk packChk;
  EXPECT_EQ(0, packChk.Check(2, argv, nullptr));
}


// Validate software pack with SemVer semantic versioning
TEST_F(PackChkIntegTests, CheckSemVer) {
  const char* argv[2];

  const string& pdscFile = PackChkIntegTestEnv::localtestdata_dir +
    "/SemVerTest/Arm.SemVerTest_DFP.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();

  PackChk packChk;
  EXPECT_EQ(1, packChk.Check(2, argv, nullptr));

  struct {
    int M329 = 0;
    int M394 = 0;
    int M393 = 0;
    int M396 = 0;
  } cnt;

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  for (const string& msg : errMsgs) {
    size_t s;

    if ((s = msg.find("M329", 0)) != string::npos) {
      cnt.M329++;
    }
    if ((s = msg.find("M393", 0)) != string::npos) {
      cnt.M393++;
    }
    if ((s = msg.find("M394", 0)) != string::npos) {
      cnt.M394++;
    }
    if ((s = msg.find("M396", 0)) != string::npos) {
      cnt.M396++;
    }
  }

  if(cnt.M329 != 2 || cnt.M393 != 3 || cnt.M394 != 4 || cnt.M396 != 3) {
    FAIL() << "Occurrences of M329, M393, M394, M396 are wrong.";
  }
}
