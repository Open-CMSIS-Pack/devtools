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

  string GetPackXsd();
  void CheckCopyPackXsd();
  void DeletePackXsd();
};

void PackChkIntegTests::SetUp() {
  CheckCopyPackXsd();
}

void PackChkIntegTests::TearDown() {
  ErrLog::Get()->ClearLogMessages();
}

string PackChkIntegTests::GetPackXsd() {
  const string schemaDestDir = string(PROJMGRUNITTESTS_BIN_PATH) + "/../etc";
  const string schemaFileName = schemaDestDir + "/PACK.xsd";

  return schemaFileName;
}

void PackChkIntegTests::CheckCopyPackXsd() {
  error_code ec;

  const string schemaDestDir = string(PROJMGRUNITTESTS_BIN_PATH) + "/../etc";
  const string schemaFileName = GetPackXsd();

  if (RteFsUtils::Exists(schemaFileName)) {
    return;
  }

  // Copy Pack.xsd
  const string packXsd = string(PACKXSD_FOLDER) + "/PACK.xsd";
  if (RteFsUtils::Exists(schemaDestDir)) {
    RteFsUtils::RemoveDir(schemaDestDir);
  }
  RteFsUtils::CreateDirectories(schemaDestDir);
  fs::copy(fs::path(packXsd), fs::path(schemaFileName), ec);
}

void PackChkIntegTests::DeletePackXsd() {
  const string schemaFileName = GetPackXsd();

  if (!RteFsUtils::Exists(schemaFileName)) {
    return;
  }

  // Delete Pack.xsd
  RteFsUtils::RemoveFile(schemaFileName);
  ASSERT_FALSE(RteFsUtils::Exists(schemaFileName));
}


 // Validate packchk when no .pdsc file found
TEST_F(PackChkIntegTests, FileNotAVailable) {
  const char* argv[2];

  argv[0] = (char*)"";
  argv[1] = (char*)"UNKNOWN.FILE.pdsc";

  PackChk packChk;
  EXPECT_EQ(1, packChk.Check(2, argv, nullptr));
}

TEST_F(PackChkIntegTests, VersionOption) {
  const char* argv[2];

  argv[0] = (char*)"";
  argv[1] = (char*)"-V";

  PackChk packChk;
  EXPECT_EQ(0, packChk.Check(2, argv, nullptr));

  argv[1] = (char*)"--version";
  EXPECT_EQ(0, packChk.Check(2, argv, nullptr));
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
  const char* argv[3];

  const string& pdscFile = PackChkIntegTestEnv::localtestdata_dir +
    "/InvalidPack/TestVendor.TestInvalidPack.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char*)"--disable-validation";

  PackChk packChk;
  EXPECT_EQ(1, packChk.Check(3, argv, nullptr));
}

// Validate software pack with component dependencies
TEST_F(PackChkIntegTests, CheckComponentDependency) {
  const char* argv[9];

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
  argv[8] = (char*)"--disable-validation";

  PackChk packChk;
  EXPECT_EQ(0, packChk.Check(9, argv, nullptr));

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
  const char* argv[11];

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

  ASSERT_TRUE(RteFsUtils::CreateTextFile(refPackTest, contentBegin + refNameTest + contentEnd));
  ASSERT_TRUE(RteFsUtils::CreateTextFile(refPack1, contentBegin + refName1 + contentEnd));
  ASSERT_TRUE(RteFsUtils::CreateTextFile(refPack2, contentBegin + refName2 + contentEnd));
  ASSERT_TRUE(RteFsUtils::CreateTextFile(refPack3, contentBegin + refName3 + contentEnd));
  ASSERT_TRUE(RteFsUtils::CreateTextFile(refPack4, contentBegin + refName4 + contentEnd));

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
  argv[10] = (char*)"--disable-validation";

  PackChk packChk;
  packChk.Check(11, argv, nullptr);

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
  const char* argv[3];

  const string& pdscFile = PackChkIntegTestEnv::localtestdata_dir +
    "/SemVerTest/Arm.SemVerTest_DFP.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char*)"--disable-validation";

  PackChk packChk;
  EXPECT_EQ(1, packChk.Check(3, argv, nullptr));

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


// Validate license path
TEST_F(PackChkIntegTests, CheckPackLicense) {
  const char* argv[3];

  const string& pdscFile = PackChkIntegTestEnv::localtestdata_dir +
    "/TestLicense/TestVendor.TestPackLicense.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char*)"--disable-validation";

  PackChk packChk;
  EXPECT_EQ(0, packChk.Check(3, argv, nullptr));

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  bool bFound = false;
  for (const string& msg : errMsgs) {
    size_t s;
    if ((s = msg.find("M327")) != string::npos) {
      bFound = true;
      break;
    }
  }

  if (!bFound) {
    FAIL() << "error: missing warning M327";
  }
}

// Validate license path
TEST_F(PackChkIntegTests, CheckFeatureSON) {
  const char* argv[3];

  const string& pdscFile = PackChkIntegTestEnv::localtestdata_dir +
    "/TestSON/TestVendor.TestSON.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char *)"--disable-validation";

  PackChk packChk;
  EXPECT_EQ(1, packChk.Check(3, argv, nullptr));

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  bool bFound = false;
  for (const string& msg : errMsgs) {
    size_t s;
    if ((s = msg.find("M371")) != string::npos) {
      bFound = true;
      break;
    }
  }

  if (!bFound) {
    FAIL() << "error: missing error M371";
  }
}

// Validate self resolving component
TEST_F(PackChkIntegTests, CheckCompResolvedByItself) {
  const char* argv[3];

  const string& pdscFile = PackChkIntegTestEnv::localtestdata_dir +
    "/CompResolvedByItself/ARM.CompResolvedByItself.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char *)"--disable-validation";

  PackChk packChk;
  EXPECT_EQ(0, packChk.Check(3, argv, nullptr));

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  int foundCnt = 0;
  for (const string& msg : errMsgs) {
    size_t s;
    if ((s = msg.find("M389")) != string::npos) {
      foundCnt++;
    }
  }

  if (foundCnt != 2) {
    FAIL() << "error: missing message M389";
  }
}

// Validate Option: -n PackName.txt
TEST_F(PackChkIntegTests, CheckPackFileName) {
  const char* argv[5];

  const string& pdscFile = PackChkIntegTestEnv::localtestdata_dir +
    "/PackNameFile/Arm.PackNameFile_DFP.pdsc";
  string outDir = PackChkIntegTestEnv::testoutput_dir + "/PackFileName";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));
  ASSERT_TRUE(RteFsUtils::CreateDirectories(outDir));

  string packNameFile = outDir;
  packNameFile += (char*)"/PackFileName.txt";

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char*)"-n";
  argv[3] = (char*)packNameFile.c_str();
  argv[4] = (char*)"--disable-validation";

  PackChk packChk;
  EXPECT_EQ(1, packChk.Check(5, argv, nullptr));

  ASSERT_TRUE(RteFsUtils::Exists(packNameFile));

  string content;
  getline(std::ifstream(packNameFile), content);

  if(content.compare("Arm.PackNameFile_DFP.0.1.1.pack")) {
    FAIL() << "error: Pack name file must contain 'Arm.PackNameFile_DFP.0.1.1.pack'";
  }
}

// Validate "--allow-suppress-error"
TEST_F(PackChkIntegTests, CheckAllowSuppressError) {
  const char* argv[6];

  const string& pdscFile = PackChkIntegTestEnv::localtestdata_dir +
    "/AllowSuppressError/TestVendor.TestInvalidPack.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char*)"--allow-suppress-error";
  argv[3] = (char*)"-x";
  argv[4] = (char*)"M323";
  argv[5] = (char*)"--disable-validation";

  PackChk packChk;
  EXPECT_EQ(0, packChk.Check(6, argv, nullptr));

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  bool bFound = false;
  for (const string& msg : errMsgs) {
    size_t s;
    if ((s = msg.find("M323")) != string::npos) {
      bFound = true;
      break;
    }
  }

  if (bFound) {
    FAIL() << "error: found error M323";
  }
}

// Validate files are inside pack root
TEST_F(PackChkIntegTests, CheckTestPackRoot) {
  const char* argv[2];

  const string& pdscFile = PackChkIntegTestEnv::localtestdata_dir +
    "/TestPackRoot/Pack/TestVendor.TestPackRoot.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();

  PackChk packChk;
  EXPECT_EQ(1, packChk.Check(2, argv, nullptr));

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  int foundCnt = 0;
  for (const string& msg : errMsgs) {
    size_t s;
    if ((s = msg.find("M313")) != string::npos) {
      foundCnt++;
    }
  }

  if (foundCnt != 1) {
    FAIL() << "error: missing message M313";
  }
}

// Validate invalid file path (file is directory)
TEST_F(PackChkIntegTests, CheckFilenameIsDir) {
  const char *argv[3];

  const string &pdscFile = PackChkIntegTestEnv::localtestdata_dir +
                           "/FilenameIsDir/TestVendor.FilenameIsDirPack.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char *) "";
  argv[1] = (char *) pdscFile.c_str();
  argv[2] = (char *) "--disable-validation";

  PackChk packChk;
  EXPECT_EQ(1, packChk.Check(3, argv, nullptr));

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  int foundCnt = 0;
  for (const string &msg: errMsgs) {
    size_t s;
    if ((s = msg.find("M356")) != string::npos) {
      foundCnt++;
    }
  }

  if (foundCnt != 1) {
    FAIL() << "error: missing message M356";
  }
}

// Validate "--xsd"
TEST_F(PackChkIntegTests, CheckXsd) {
  const char* argv[4];

  const string& pdscFile = PackChkIntegTestEnv::globaltestdata_dir +
  "/packs/ARM/RteTest/0.1.0/ARM.RteTest.pdsc";
  const string& schemaFile = PackChkIntegTestEnv::localtestdata_dir + "/schema.xsd";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));
  ASSERT_TRUE(RteFsUtils::Exists(schemaFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char*)"--xsd";
  argv[3] = (char*)schemaFile.c_str();

  PackChk packChk;
  EXPECT_EQ(0, packChk.Check(4, argv, nullptr));
}

// Validate "--xsd" incorrect path
TEST_F(PackChkIntegTests, CheckNotExistXsd) {
  const char* argv[4];

  const string& pdscFile = PackChkIntegTestEnv::globaltestdata_dir +
                           "/packs/ARM/RteTest/0.1.0/ARM.RteTest.pdsc";
  const string& schemaFile = PackChkIntegTestEnv::localtestdata_dir + "/schemaNotExist.xsd";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));
  ASSERT_FALSE(RteFsUtils::Exists(schemaFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char*)"--xsd";
  argv[3] = (char*)schemaFile.c_str();

  PackChk packChk;
  EXPECT_EQ(1, packChk.Check(4, argv, nullptr));

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  bool bFound = false;
  for (const string& msg : errMsgs) {
    size_t s;
    if ((s = msg.find("M219")) != string::npos) {
      bFound = true;
      break;
    }
  }

  if (!bFound) {
    FAIL() << "error: missing error M219";
  }
}

TEST_F(PackChkIntegTests, CheckPackNamedXsdNotFound) {
  const char* argv[4];

  const string& pdscFile = PackChkIntegTestEnv::localtestdata_dir +
    "/SchemaValidation/TestVendor.SchemaValidation.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  DeletePackXsd();

  const string schemaFileName = GetPackXsd();

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char*)"--xsd";
  argv[3] = (char*)schemaFileName.c_str();

  PackChk packChk;
  EXPECT_EQ(1, packChk.Check(4, argv, nullptr));

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  int M219_foundCnt = 0;

  for (const string& msg : errMsgs) {
    size_t s;
    if ((s = msg.find("M219")) != string::npos) {
      M219_foundCnt++;    // follows one M511: <descripton>
    }
  }

  if (M219_foundCnt != 1) {
    FAIL() << "error: missing message M219";
  }
}

// Validate invalid file path (file is directory)
TEST_F(PackChkIntegTests, CheckFileNameHasSpace) {
  const char* argv[3];

  const string& pdscFile = PackChkIntegTestEnv::localtestdata_dir +
    "/FileNameHasSpace/TestVendor.FileNameHasSpacePack.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char *)"--disable-validation";

  PackChk packChk;
  EXPECT_EQ(0, packChk.Check(3, argv, nullptr));

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  int foundCnt = 0;
  for (const string& msg : errMsgs) {
    size_t s;
    if ((s = msg.find("M314")) != string::npos) {
      foundCnt++;
    }
  }

  if (foundCnt != 9) {
    FAIL() << "error: missing message M314";
  }
}

// Validate invalid file path (file is directory)
TEST_F(PackChkIntegTests, CheckDuplicateFlashAlgo) {
  const char* argv[2];

  const string& pdscFile = PackChkIntegTestEnv::localtestdata_dir +
    "/DuplicateFlashAlgo/TestVendor.DuplicateFlashAlgo.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();

  PackChk packChk;
  EXPECT_EQ(1, packChk.Check(2, argv, nullptr));

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  int M348_foundCnt = 0;
  int M369_foundCnt = 0;
  for (const string& msg : errMsgs) {
    size_t s;
    if ((s = msg.find("M348")) != string::npos) {
      M348_foundCnt++;
    }
    if ((s = msg.find("M369")) != string::npos) {
      M369_foundCnt++;
    }
  }

  if (M348_foundCnt != 2 || M369_foundCnt != 4) {
    FAIL() << "error: missing message M348 or M369";
  }
}

// Test schema validation
TEST_F(PackChkIntegTests, CheckSchemaValidation) {
  const char* argv[3];

  const string& pdscFile = PackChkIntegTestEnv::localtestdata_dir +
    "/SchemaValidation/TestVendor.SchemaValidation.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();

  PackChk packChk;
  EXPECT_EQ(1, packChk.Check(2, argv, nullptr));

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  int M510_foundCnt = 0;
  int M511_foundCnt = 0;
  int M306_foundCnt = 0;

  for (const string& msg : errMsgs) {
    size_t s;
    if ((s = msg.find("M306")) != string::npos) {
      M306_foundCnt++;    // follows one M511: <descripton>
    }
    else if ((s = msg.find("M510")) != string::npos) {
      M510_foundCnt++;
    }
    else if ((s = msg.find("M511")) != string::npos) {
      M511_foundCnt++;
    }
  }

  if (M510_foundCnt != 0 || M511_foundCnt != 6 || M306_foundCnt != 1) {
    FAIL() << "error: missing message M510, M511 or M306";
  }
}


TEST_F(PackChkIntegTests, CheckConditionComponentDependency_Neg) {
  const char* argv[5];

  const string& pdscFile = PackChkIntegTestEnv::globaltestdata_dir +
    "/packs/ARM/RteTest_DFP/0.2.0/ARM.RteTest_DFP.pdsc";
  const string& refFile = PackChkIntegTestEnv::globaltestdata_dir +
    "/packs/ARM/RteTest/0.1.0/ARM.RteTest.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));
  ASSERT_TRUE(RteFsUtils::Exists(refFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char*)"-x";
  argv[3] = (char*)"!M317";
  argv[4] = (char*)"--disable-validation";

  PackChk packChk;
  EXPECT_EQ(0, packChk.Check(5, argv, nullptr));

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  int M317_foundCnt = 0;
  for (const string& msg : errMsgs) {
    size_t s;
    if ((s = msg.find("M317", 0)) != string::npos) {
      M317_foundCnt++;
    }
  }

  if(M317_foundCnt != 4) {
    FAIL() << "error: warning M317 count != 4";
  }
}

TEST_F(PackChkIntegTests, CheckConditionComponentDependency_Pos) {
  const char* argv[7];

  const string& pdscFile = PackChkIntegTestEnv::globaltestdata_dir +
    "/packs/ARM/RteTest_DFP/0.2.0/ARM.RteTest_DFP.pdsc";
  const string& refFile = PackChkIntegTestEnv::globaltestdata_dir +
    "/packs/ARM/RteTest/0.1.0/ARM.RteTest.pdsc";
  ASSERT_TRUE(RteFsUtils::Exists(pdscFile));
  ASSERT_TRUE(RteFsUtils::Exists(refFile));

  argv[0] = (char*)"";
  argv[1] = (char*)pdscFile.c_str();
  argv[2] = (char*)"-i";
  argv[3] = (char*)refFile.c_str();
  argv[4] = (char*)"-x";
  argv[5] = (char*)"!M317";
  argv[6] = (char*)"--disable-validation";

  PackChk packChk;
  EXPECT_EQ(0, packChk.Check(7, argv, nullptr));

  auto errMsgs = ErrLog::Get()->GetLogMessages();
  int M317_foundCnt = 0;
  for (const string& msg : errMsgs) {
    size_t s;
    if ((s = msg.find("M317", 0)) != string::npos) {
      M317_foundCnt++;
    }
  }

  if(M317_foundCnt) {
    FAIL() << "error: warning M317 found";
  }
}






