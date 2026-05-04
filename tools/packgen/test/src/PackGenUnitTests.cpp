/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "PackGen.h"
#include "PackGenTestEnv.h"
#include "RteFsUtils.h"

#include "gtest/gtest.h"

#include <iostream>
#include <fstream>
#include <regex>

using namespace std;

static constexpr const char* packName = "TestPack";
static constexpr const char* packDescription = "TestPack description";
static constexpr const char* packVendor = "ARM";
static constexpr const char* packLicense = "LICENSE";
static constexpr const char* packUrl = "http://arm.com/";
static constexpr const char* packRepositoryUrl = "https://github.com/ARM-software/CMSIS-Driver.git";
static constexpr const char* packRepositoryType = "git";

static constexpr const char* releaseVersion1 = "1.0.0";
static constexpr const char* releaseDate1 = "2021-08-01";
static constexpr const char* releaseDescription1 = "First release";
static constexpr const char* releaseTag1 = "tag-1.0.0";
static constexpr const char* releaseUrl1 = "https://github.com/MDK-Packs/releases/download/ARM.Dummy.1.0.0.pack";
static constexpr const char* releaseDeprecated1 = "2021-12-01";
static constexpr const char* releaseVersion2 = "2.0.0";
static constexpr const char* releaseDate2 = "2021-08-02";
static constexpr const char* releaseDescription2 = "Second release";
static constexpr const char* releaseTag2 = "tag-2.0.0";
static constexpr const char* releaseUrl2 = "https://github.com/MDK-Packs/releases/download/ARM.Dummy.2.0.0.pack";
static constexpr const char* releaseDeprecated2 = "2021-12-02";

static constexpr const char* requirementPackageVendor1 = "Test Vendor 1";
static constexpr const char* requirementPackageName1 = "Test Name 1";
static constexpr const char* requirementPackageVersion1 = "1.1.1";
static constexpr const char* requirementPackageVendor2 = "Test Vendor 2";
static constexpr const char* requirementPackageName2 = "Test Name 2";
static constexpr const char* requirementPackageVersion2 = "2.2.2";

static constexpr const char* taxonomyCclass1 = "Test Class 1";
static constexpr const char* taxonomyCgroup1 = "Test Group 1";
static constexpr const char* taxonomyDescription1 = "Taxonomy description 1";
static constexpr const char* taxonomyCclass2 = "Test Class 2";
static constexpr const char* taxonomyCgroup2 = "Test Group 2";
static constexpr const char* taxonomyDescription2 = "Taxonomy description 2";

class PackGenUnitTests : public PackGen, public ::testing::Test {
public:
  PackGenUnitTests() {}
  virtual ~PackGenUnitTests() {}
  void CompareFile(const string& file1, const string& file2);
  void CompareFiletree(const string& dir1, const string& dir2);
protected:
  std::regex versionStrRegex{ R"(^(packgen\s\d+(?:\.\d+){2}([+\d\w-]+)?\s\(C\)\s[\d]{4}(-[\d]{4})?\sArm\sLtd.\sand\sContributors(\r\n|\n))$)" };
};

void PackGenUnitTests::CompareFile(const string& file1, const string& file2) {
  ifstream f1, f2;
  string l1, l2;
  bool ret_val;

  f1.open(file1);
  ret_val = f1.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << file1;

  f2.open(file2);
  ret_val = f2.is_open();
  ASSERT_EQ(ret_val, true) << "Failed to open " << file2;

  while (getline(f1, l1) && getline(f2, l2)) {
    if (!l1.empty() && l1.rfind('\r') == l1.length() - 1) {
      l1.pop_back();
    }

    if (!l2.empty() && l2.rfind('\r') == l2.length() - 1) {
      l2.pop_back();
    }

    if (l1 != l2) {
      FAIL() << "error: " << file1 << " is different from " << file2;
    }
  }

  f1.close();
  f2.close();
}

void PackGenUnitTests::CompareFiletree(const string& dir1, const string& dir2) {
  error_code ec;
  vector<string> vector1, vector2;

  for (const auto& p : fs::recursive_directory_iterator(dir1, ec)) {
    vector1.push_back(p.path().filename().generic_string());
  }

  for (const auto& p : fs::recursive_directory_iterator(dir2, ec)) {
    const auto& ext = p.path().extension();
    if ((ext == ".txt")||(ext == ".yml")) {
      continue;
    }
    vector2.push_back(p.path().filename().generic_string());
  }

  ASSERT_EQ(vector1 == vector2, true) << "Directory '" << dir1
    << "' filetree is different from '" << dir2 << "' reference";
}

TEST_F(PackGenUnitTests, RunPackGen) {
  char *argv[5];

  // Empty options
  EXPECT_EQ(0, RunPackGen(1, argv));

  // Help options
  const string& manifest = testinput_folder + "/CMakeTestProject/manifest.yml";
  argv[1] = (char*)manifest.c_str();
  argv[2] = (char*)"--output";
  argv[3] = (char*)testoutput_folder.c_str();
  argv[4] = (char*)"--help";
  EXPECT_EQ(0, RunPackGen(5, argv));

  // Invalid manifest file
  const string& invalidManifest = testinput_folder + "/invalid_manifest.yml";
  argv[1] = (char*)invalidManifest.c_str();
  EXPECT_EQ(1, RunPackGen(2, argv));

  // Invalid manifest file, missing 'packs' section
  const string& invalidManifest2 = testinput_folder + "/invalid_manifest2.yml";
  argv[1] = (char*)invalidManifest2.c_str();
  EXPECT_EQ(1, RunPackGen(2, argv));

  // Manifest file doesn't exist
  const string& unknownManifest = testinput_folder + "/Unknown_manifest.yml";
  argv[1] = (char*)unknownManifest.c_str();
  EXPECT_EQ(1, RunPackGen(2, argv));

  // Non existent option
  argv[1] = (char*) "--nonexistent";
  EXPECT_EQ(1, RunPackGen(2, argv));
}

TEST_F(PackGenUnitTests, RunPackGenVersion_1) {
  char* argv[2];
  std::stringstream coutBuf;
  // Redirect std::cout to buffer
  std::streambuf* buffer = std::cout.rdbuf(coutBuf.rdbuf());

  // -V option
  argv[1] = (char*)"-V";
  EXPECT_EQ(0, RunPackGen(2, argv));
  EXPECT_TRUE(std::regex_match(coutBuf.str(), versionStrRegex));

  // Restore original buffer before exiting
  std::cout.rdbuf(buffer);

}

TEST_F(PackGenUnitTests, RunPackGenVersion_2) {
  char* argv[2];
  std::stringstream coutBuf;
  // Redirect std::cout to buffer
  std::streambuf* buffer = std::cout.rdbuf(coutBuf.rdbuf());

  // --version option
  argv[1] = (char*)"--version";
  EXPECT_EQ(0, RunPackGen(2, argv));
  EXPECT_TRUE(std::regex_match(coutBuf.str(), versionStrRegex));

  // Restore original buffer before exiting
  std::cout.rdbuf(buffer);
}

TEST_F(PackGenUnitTests, RunPackGenVerbose) {
  char* argv[7];
  std::stringstream coutBuf;

  // Redirect std::cout to buffer
  std::streambuf* buffer = std::cout.rdbuf(coutBuf.rdbuf());

  // Options
  const string& manifest = testinput_folder + "/CMakeTestProject/manifest.yml";
  argv[1] = (char*)manifest.c_str();
  argv[2] = (char*)"--output";
  argv[3] = (char*)testoutput_folder.c_str();
  argv[4] = (char*)"--nocheck";
  argv[5] = (char*)"--nozip";
  argv[6] = (char*)"--verbose";
  EXPECT_EQ(0, RunPackGen(7, argv));

  // Write verbose info in file
  ofstream fileStream(testoutput_folder + "/verbose.info");
  fileStream << coutBuf.str();
  fileStream << flush;
  fileStream.close();

  // Check generated pack filetree
  CompareFiletree(testoutput_folder + "/ARM.TestPack.1.0.0",
    testinput_folder + "/CMakeTestProject");

  // Check generated PDSC
  CompareFile(testoutput_folder + "/ARM.TestPack.1.0.0/ARM.TestPack.pdsc",
    testinput_folder + "/CMakeTestProject/ARM.TestPack.pdsc");

  // Check verbose info
  CompareFile(testoutput_folder + "/verbose.info",
    testinput_folder + "/verbose.info");

  // Restore original buffer before exiting
  std::cout.rdbuf(buffer);
}

TEST_F(PackGenUnitTests, RunPackGen_Rel_Output_Path) {
  char* argv[6];

  const string& relOutPath = fs::relative(fs::path(testoutput_folder), fs::current_path()).generic_string();
  const string& manifest = testinput_folder + "/CMakeTestProject/manifest.yml";
  argv[1] = (char*)manifest.c_str();
  argv[2] = (char*)"--output";
  argv[3] = (char*)relOutPath.c_str();
  argv[4] = (char*)"--nocheck";
  argv[5] = (char*)"--nozip";
  EXPECT_EQ(0, RunPackGen(6, argv));

  // Check generated pack file tree
  CompareFiletree(testoutput_folder + "/ARM.TestPack.1.0.0",
    testinput_folder + "/CMakeTestProject");

  // Check generated PDSC
  CompareFile(testoutput_folder + "/ARM.TestPack.1.0.0/ARM.TestPack.pdsc",
    testinput_folder + "/CMakeTestProject/ARM.TestPack.pdsc");
}

TEST_F(PackGenUnitTests, RunPackGen_With_Defines) {
  error_code ec;
  vector<string> vecfile;
  char* argv[7];

  const string& manifest = testinput_folder + "/TestProject/manifest.yml";

  if (fs::exists(testoutput_folder))
    fs::remove_all(testoutput_folder);

  argv[1] = (char*)manifest.c_str();
  argv[2] = (char*)"--output";
  argv[3] = (char*)testoutput_folder.c_str();
  argv[4] = (char*)"--nocheck";
  argv[5] = (char*)"--nozip";
  argv[6] = (char*)"--verbose";
  EXPECT_EQ(0, RunPackGen(7, argv));

  // Check generated pack file tree
  string fileName;

  const string& outPackPath = testoutput_folder + "/TestVendor.TestPack.1.0.0";
  for (const auto& p : fs::recursive_directory_iterator(outPackPath, ec)) {
    fileName = p.path().filename().generic_string();
    EXPECT_EQ(fileName.find(".info"), string::npos);
    EXPECT_EQ(fileName.find(".jpeg"), string::npos);
    vecfile.push_back(fileName);
  }
  EXPECT_EQ(vecfile.size(), 12);

  // Check generated PDSC
  CompareFile(testoutput_folder + "/TestVendor.TestPack.1.0.0/TestVendor.TestPack.pdsc",
    testinput_folder + "/TestProject/TestVendor.TestPack.pdsc");
}

TEST_F(PackGenUnitTests, RunPackGen_OutOfTree) {
  char* argv[8];

  const string& manifest = testinput_folder + "/TestProject/out-of-root/manifest.yml";
  const string& sourceRoot = testinput_folder + "/TestProject";
  argv[1] = (char*)manifest.c_str();
  argv[2] = (char*)"--output";
  argv[3] = (char*)testoutput_folder.c_str();
  argv[4] = (char*)"--nocheck";
  argv[5] = (char*)"--nozip";
  argv[6] = (char*)"--source";
  argv[7] = (char*)sourceRoot.c_str();
  EXPECT_EQ(0, RunPackGen(8, argv));

  // Check generated PDSC
  CompareFile(testoutput_folder + "/TestVendor.TestPackOutOfRoot.1.0.0/TestVendor.TestPackOutOfRoot.pdsc",
    testinput_folder + "/TestProject/out-of-root/TestVendor.TestPackOutOfRoot.pdsc");
}

TEST_F(PackGenUnitTests, RunPackGenMultipleBuilds) {
  char* argv[6];

  const string& manifest = testinput_folder + "/CMakeTestMultipleBuilds/manifest.yml";
  argv[1] = (char*)manifest.c_str();
  argv[2] = (char*)"--output";
  argv[3] = (char*)testoutput_folder.c_str();
  argv[4] = (char*)"--nocheck";
  argv[5] = (char*)"--nozip";
  EXPECT_EQ(0, RunPackGen(6, argv));

  // Check generated pack filetree
  CompareFiletree(testoutput_folder + "/ARM.TestPackMultipleBuilds.1.0.0",
                  testinput_folder + "/CMakeTestMultipleBuilds");

  // Check generated PDSC
  CompareFile(testoutput_folder + "/ARM.TestPackMultipleBuilds.1.0.0/ARM.TestPackMultipleBuilds.pdsc",
              testinput_folder + "/CMakeTestMultipleBuilds/ARM.TestPackMultipleBuilds.pdsc");
}

TEST_F(PackGenUnitTests, ParseManifestTest) {
  // Empty manifest
  m_manifest = "";
  EXPECT_FALSE(ParseManifest());

  // Invalid manifest file
  m_manifest = testinput_folder + "/invalid_manifest.yml";
  EXPECT_FALSE(ParseManifest());

  // Non existent manifest
  m_manifest = "non-existent-manifest.yml";
  EXPECT_FALSE(ParseManifest());

  // Non existent manifest
  m_manifest = testinput_folder + "/no-target-manifest.yml";
  EXPECT_FALSE(ParseManifest());

  // Correct manifest
  m_manifest = testinput_folder + "/CMakeTestProject/manifest.yml";
  EXPECT_TRUE(ParseManifest());
}

TEST_F(PackGenUnitTests, ParseManifestInfoTest) {
  YAML::Node item;
  packInfo pack;

  item["name"] = packName;
  item["description"] = packDescription;
  item["vendor"] = packVendor;
  item["license"] = packLicense;
  item["url"] = packUrl;
  item["repository"]["url"] = packRepositoryUrl;
  item["repository"]["type"] = packRepositoryType;
  m_repoRoot = "TEST_REPO";

  ParseManifestInfo(item, pack);

  EXPECT_EQ(packName, pack.name);
  EXPECT_EQ(packDescription, pack.description);
  EXPECT_EQ(packVendor, pack.vendor);
  EXPECT_EQ(packLicense, pack.license);
  EXPECT_EQ(packUrl, pack.url);
  EXPECT_EQ(packRepositoryUrl, pack.repository.url);
  EXPECT_EQ(packRepositoryType, pack.repository.type);
}

TEST_F(PackGenUnitTests, ParseManifestReleasesTest) {
  YAML::Node item, subitem1, subitem2;
  packInfo pack;
  subitem1["version"] = releaseVersion1;
  subitem1["date"] = releaseDate1;
  subitem1["description"] = releaseDescription1;
  subitem1["tag"] = releaseTag1;
  subitem1["url"] = releaseUrl1;
  subitem1["deprecated"] = releaseDeprecated1;
  subitem2["version"] = releaseVersion2;
  subitem2["date"] = releaseDate2;
  subitem2["description"] = releaseDescription2;
  subitem2["tag"] = releaseTag2;
  subitem2["url"] = releaseUrl2;
  subitem2["deprecated"] = releaseDeprecated2;
  item["releases"].push_back(subitem1);
  item["releases"].push_back(subitem2);

  ParseManifestReleases(item, pack);

  ASSERT_EQ(pack.releases.size(), 2);
  EXPECT_EQ(releaseVersion1, pack.releases.front().attributes["version"]);
  EXPECT_EQ(releaseDate1, pack.releases.front().attributes["date"]);
  EXPECT_EQ(releaseDescription1, pack.releases.front().attributes["description"]);
  EXPECT_EQ(releaseTag1, pack.releases.front().attributes["tag"]);
  EXPECT_EQ(releaseUrl1, pack.releases.front().attributes["url"]);
  EXPECT_EQ(releaseDeprecated1, pack.releases.front().attributes["deprecated"]);
  EXPECT_EQ(releaseVersion2, pack.releases.back().attributes["version"]);
  EXPECT_EQ(releaseDate2, pack.releases.back().attributes["date"]);
  EXPECT_EQ(releaseDescription2, pack.releases.back().attributes["description"]);
  EXPECT_EQ(releaseTag2, pack.releases.back().attributes["tag"]);
  EXPECT_EQ(releaseUrl2, pack.releases.back().attributes["url"]);
  EXPECT_EQ(releaseDeprecated2, pack.releases.back().attributes["deprecated"]);
}

TEST_F(PackGenUnitTests, ParseManifestRequirementsTest) {
  YAML::Node item, subitem1, subitem2;
  packInfo pack;
  subitem1["attributes"]["vendor"] = requirementPackageVendor1;
  subitem1["attributes"]["name"] = requirementPackageName1;
  subitem1["attributes"]["version"] = requirementPackageVersion1;
  subitem2["attributes"]["vendor"] = requirementPackageVendor2;
  subitem2["attributes"]["name"] = requirementPackageName2;
  subitem2["attributes"]["version"] = requirementPackageVersion2;
  item["requirements"]["packages"].push_back(subitem1);
  item["requirements"]["packages"].push_back(subitem2);

  ParseManifestRequirements(item, pack);

  ASSERT_EQ(pack.requirements.packages.size(), 2);
  EXPECT_EQ(requirementPackageVendor1, pack.requirements.packages.front()["vendor"]);
  EXPECT_EQ(requirementPackageName1, pack.requirements.packages.front()["name"]);
  EXPECT_EQ(requirementPackageVersion1, pack.requirements.packages.front()["version"]);
  EXPECT_EQ(requirementPackageVendor2, pack.requirements.packages.back()["vendor"]);
  EXPECT_EQ(requirementPackageName2, pack.requirements.packages.back()["name"]);
  EXPECT_EQ(requirementPackageVersion2, pack.requirements.packages.back()["version"]);
}

TEST_F(PackGenUnitTests, ParseManifestTaxonomyTest) {
  YAML::Node item, subitem1, subitem2;
  packInfo pack;
  subitem1["attributes"]["Cclass"] = taxonomyCclass1;
  subitem1["attributes"]["Cgroup"] = taxonomyCgroup1;
  subitem1["description"] = taxonomyDescription1;
  subitem2["attributes"]["Cclass"] = taxonomyCclass2;
  subitem2["attributes"]["Cgroup"] = taxonomyCgroup2;
  subitem2["description"] = taxonomyDescription2;
  item["taxonomy"].push_back(subitem1);
  item["taxonomy"].push_back(subitem2);

  ParseManifestTaxonomy(item, pack);

  ASSERT_EQ(pack.taxonomy.size(), 2);
  EXPECT_EQ(taxonomyCclass1, pack.taxonomy.front().attributes["Cclass"]);
  EXPECT_EQ(taxonomyCgroup1, pack.taxonomy.front().attributes["Cgroup"]);
  EXPECT_EQ(taxonomyDescription1, pack.taxonomy.front().description);
  EXPECT_EQ(taxonomyCclass2, pack.taxonomy.back().attributes["Cclass"]);
  EXPECT_EQ(taxonomyCgroup2, pack.taxonomy.back().attributes["Cgroup"]);
  EXPECT_EQ(taxonomyDescription2, pack.taxonomy.back().description);
}

TEST_F(PackGenUnitTests, CreatePackInfoTest) {
  packInfo pack;
  m_pdscTree = new XMLTreeSlim();
  XMLTreeElement* rootElement;
  rootElement = m_pdscTree->CreateElement("package");
  ASSERT_NE(rootElement, nullptr);

  pack.name = packName;
  pack.description = packDescription;
  pack.vendor = packVendor;
  pack.license = packLicense;
  pack.url = packUrl;

  CreatePackInfo(rootElement, pack);

  EXPECT_EQ(packName, rootElement->GetChildText("name"));
  EXPECT_EQ(packDescription, rootElement->GetChildText("description"));
  EXPECT_EQ(packVendor, rootElement->GetChildText("vendor"));
  EXPECT_EQ(packLicense, rootElement->GetChildText("license"));
  EXPECT_EQ(packUrl, rootElement->GetChildText("url"));
}

TEST_F(PackGenUnitTests, CreatePackReleasesTest) {
  packInfo pack;
  m_pdscTree = new XMLTreeSlim();
  XMLTreeElement* rootElement;
  rootElement = m_pdscTree->CreateElement("package");
  ASSERT_NE(rootElement, nullptr);

  releaseInfo item;
  item.attributes["version"] = releaseVersion1;
  item.attributes["date"] = releaseDate1;
  item.attributes["description"] = releaseDescription1;
  pack.releases.push_back(item);
  item.attributes["version"] = releaseVersion2;
  item.attributes["date"] = releaseDate2;
  item.attributes["description"] = releaseDescription2;
  pack.releases.push_back(item);

  CreatePackReleases(rootElement, pack);

  EXPECT_EQ(releaseVersion1, rootElement->GetGrandChildren("releases").front()->GetAttribute("version"));
  EXPECT_EQ(releaseDate1, rootElement->GetGrandChildren("releases").front()->GetAttribute("date"));
  EXPECT_EQ(releaseDescription1, rootElement->GetGrandChildren("releases").front()->GetText());
  EXPECT_EQ(releaseVersion2, rootElement->GetGrandChildren("releases").back()->GetAttribute("version"));
  EXPECT_EQ(releaseDate2, rootElement->GetGrandChildren("releases").back()->GetAttribute("date"));
  EXPECT_EQ(releaseDescription2, rootElement->GetGrandChildren("releases").back()->GetText());
}

TEST_F(PackGenUnitTests, CreatePackRequirementsTest) {
  packInfo pack;
  m_pdscTree = new XMLTreeSlim();
  XMLTreeElement* rootElement;
  rootElement = m_pdscTree->CreateElement("package");
  ASSERT_NE(rootElement, nullptr);

  map<string, string> item;
  item["vendor"] = requirementPackageVendor1;
  item["name"] = requirementPackageName1;
  item["version"] = requirementPackageVersion1;
  pack.requirements.packages.push_back(item);
  item["vendor"] = requirementPackageVendor2;
  item["name"] = requirementPackageName2;
  item["version"] = requirementPackageVersion2;
  pack.requirements.packages.push_back(item);

  CreatePackRequirements(rootElement, pack);
  EXPECT_EQ(requirementPackageVendor1, rootElement->GetFirstChild("requirements")->GetGrandChildren("packages").front()->GetAttribute("vendor"));
  EXPECT_EQ(requirementPackageName1, rootElement->GetFirstChild("requirements")->GetGrandChildren("packages").front()->GetAttribute("name"));
  EXPECT_EQ(requirementPackageVersion1, rootElement->GetFirstChild("requirements")->GetGrandChildren("packages").front()->GetAttribute("version"));
  EXPECT_EQ(requirementPackageVendor2, rootElement->GetFirstChild("requirements")->GetGrandChildren("packages").back()->GetAttribute("vendor"));
  EXPECT_EQ(requirementPackageName2, rootElement->GetFirstChild("requirements")->GetGrandChildren("packages").back()->GetAttribute("name"));
  EXPECT_EQ(requirementPackageVersion2, rootElement->GetFirstChild("requirements")->GetGrandChildren("packages").back()->GetAttribute("version"));
}

TEST_F(PackGenUnitTests, CreatePackTaxonomyTest) {
  packInfo pack;
  m_pdscTree = new XMLTreeSlim();
  XMLTreeElement* rootElement;
  rootElement = m_pdscTree->CreateElement("package");
  ASSERT_NE(rootElement, nullptr);

  taxonomyInfo item;
  item.attributes["Cclass"] = taxonomyCclass1;
  item.attributes["Cgroup"] = taxonomyCgroup1;
  item.description = taxonomyDescription1;
  pack.taxonomy.push_back(item);
  item.attributes["Cclass"] = taxonomyCclass2;
  item.attributes["Cgroup"] = taxonomyCgroup2;
  item.description = taxonomyDescription2;
  pack.taxonomy.push_back(item);

  CreatePackTaxonomy(rootElement, pack);

  EXPECT_EQ(taxonomyCclass1, rootElement->GetGrandChildren("taxonomy").front()->GetAttribute("Cclass"));
  EXPECT_EQ(taxonomyCgroup1, rootElement->GetGrandChildren("taxonomy").front()->GetAttribute("Cgroup"));
  EXPECT_EQ(taxonomyDescription1, rootElement->GetGrandChildren("taxonomy").front()->GetText());
  EXPECT_EQ(taxonomyCclass2, rootElement->GetGrandChildren("taxonomy").back()->GetAttribute("Cclass"));
  EXPECT_EQ(taxonomyCgroup2, rootElement->GetGrandChildren("taxonomy").back()->GetAttribute("Cgroup"));
  EXPECT_EQ(taxonomyDescription2, rootElement->GetGrandChildren("taxonomy").back()->GetText());
}
