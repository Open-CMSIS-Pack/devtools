/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"
#include "gtest/gtest.h"

using namespace std;

class ProjMgrWorkerUnitTests : public ProjMgrWorker, public ::testing::Test {
protected:
  ProjMgrWorkerUnitTests() {}
  virtual ~ProjMgrWorkerUnitTests() {}

  void SetCsolutionPacks(CsolutionItem* csolution, std::vector<std::string> packs, std::string targetType);
};

void ProjMgrWorkerUnitTests::SetCsolutionPacks(CsolutionItem* csolution, std::vector<std::string> packs, std::string targetType) {
  ContextItem context;
  for (auto& pack : packs) {
    csolution->packs.push_back(PackItem{pack, {}});
  }
  context.csolution = csolution;
  context.type.target = targetType;
  m_contexts.insert(std::pair<string, ContextItem>(string(targetType), context));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessToolchain) {
  ToolchainItem expected;
  expected.name = "AC6";
  expected.version = "6.18.0";
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessToolchain(context));
  EXPECT_EQ(expected.name, context.toolchain.name);
  EXPECT_EQ(expected.version, context.toolchain.version);
}

TEST_F(ProjMgrWorkerUnitTests, ProcessToolchainOptions) {
  struct expectedOutput {
    bool result;
    string options, compiler, version;
  };

  std::map<std::string, expectedOutput> testInput =
  {
    { "", {false, "", "", ""}},
    { "TEST", {true, "", "TEST", "0.0.0"}},
    { "AC6", {true, "AC6", "ARMCC", "6.18.0"}}
  };

  for (auto input : testInput) {
    ContextItem context;
    context.compiler = input.first;

    EXPECT_EQ(input.second.result, ProcessToolchain(context));
    EXPECT_EQ(input.second.options, context.targetAttributes["Toptions"]);
    EXPECT_EQ(input.second.compiler, context.targetAttributes["Tcompiler"]);
    EXPECT_EQ(input.second.version, context.toolchain.version);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice) {
  map<string, string> expected = {
    {"Dclock", "10000000"},
    {"Dcore", "Cortex-M0"},
    {"DcoreVersion", "r0p0"},
    {"Dendian", "Little-endian"},
    {"Dfpu", "NO_FPU"},
    {"Dmpu", "NO_MPU"},
    {"Dname", "RteTest_ARMCM0"},
    {"Dvendor", "ARM:82"},
    {"Dsecure", "Non-secure"}
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  for (const auto& expectedAttribute : expected) {
    EXPECT_EQ(expectedAttribute.second, context.targetAttributes[expectedAttribute.first]);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents) {
  set<string> expected = {
    "ARM::Device:Startup&RteTest Startup@2.0.3",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expected.size(), context.components.size());
  auto it = context.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_Cvariant1) {
  set<string> expected = {
    "ARM::Device:Test variant@1.1.1",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test exact partial component identifier match without Cvendor
  const string& filename = testinput_folder + "/TestProject/test_component_variant1.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expected.size(), context.components.size());
  auto it = context.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_Cvariant2) {
  set<string> expected = {
    "ARM::Device:Test variant@1.1.1",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test exact partial component identifier match with Cvendor
  const string& filename = testinput_folder + "/TestProject/test_component_variant2.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expected.size(), context.components.size());
  auto it = context.components.begin();
  for (const auto& expectedComponent : expected) {
    EXPECT_EQ(expectedComponent, (*it++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_MultipleMatches1) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches, different versions
  const string& filename = testinput_folder + "/TestProject/test_component_multiple_matches1.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_FALSE(ProcessComponents(context));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_MultipleMatches2) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches
  const string& filename = testinput_folder + "/TestProject/test_component_multiple_matches2.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_FALSE(ProcessComponents(context));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponentsApi) {
  set<string> expectedComponents = {
    "ARM::Device:Startup&RteTest Startup@2.0.3",
    "ARM::RteTest:ApiExclusive:S1@0.9.9",
    "ARM::RteTest:CORE@0.1.1",
  };
  set<string> expectedPackages = {
    "ARM::RteTest@0.1.0",
    "ARM::RteTest_DFP@0.2.0",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test-api.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  ASSERT_EQ(expectedComponents.size(), context.components.size());
  auto componentIt = context.components.begin();
  for (const auto& expectedComponent : expectedComponents) {
    EXPECT_EQ(expectedComponent, (*componentIt++).first);
  }
  ASSERT_EQ(expectedPackages.size(), context.packages.size());
  auto packageIt = context.packages.begin();
  for (const auto& expectedPackage : expectedPackages) {
    EXPECT_EQ(expectedPackage, (*packageIt++).first);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDependencies) {
  map<string, set<string>> expected = {{ "ARM::Device:Startup&RteTest Startup@2.0.3" , {"require RteTest:CORE"} }};
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test-dependency.cproject.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  EXPECT_TRUE(ProcessComponents(context));
  EXPECT_TRUE(ProcessDependencies(context));
  EXPECT_FALSE(ValidateContext(context));
  ASSERT_EQ(expected.size(), context.validationResults.size());
  map<string, set<string>> dependenciesMap;
  for (const auto& [result, component, dependencies, _] : context.validationResults) {
    dependenciesMap[component] = dependencies;
  }
  for (const auto& [expectedComponent, expectedDependencies] : expected) {
    EXPECT_TRUE(dependenciesMap.find(expectedComponent) != dependenciesMap.end());
    const auto& dependencies = dependenciesMap[expectedComponent];
    for (const auto& expectedDependency : expectedDependencies) {
      EXPECT_TRUE(dependencies.find(expectedDependency) != dependencies.end());
    }
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDeviceFailed) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestProject/test.cproject_device_pname_unavailable_in_board.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_FALSE(ProcessDevice(context));
}

TEST_F(ProjMgrWorkerUnitTests, LoadUnknownPacks) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, {"ARM::RteTest_Unknown@2.0.1"}, "Test");
  ContextItem context;
  EXPECT_FALSE(LoadPacks(context));
  EXPECT_EQ(0, m_loadedPacks.size());
}

TEST_F(ProjMgrWorkerUnitTests, LoadDuplicatePacks) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, {"ARM::RteTest_DFP@0.2.0", "ARM::RteTest_DFP"}, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  // Check if only one pack is loaded
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM.RteTest_DFP.0.2.0", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadRequiredPacks) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::RteTest_DFP@0.2.0"}, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  // Check if only one pack is loaded
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM.RteTest_DFP.0.2.0", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadExactPackVersion) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::RteTest_DFP@0.1.1" }, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  // Check if only one pack is loaded
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM.RteTest_DFP.0.1.1", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadPacksNoPackage) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, {}, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  // by default all packs available should be loaded
  EXPECT_EQ(5, m_loadedPacks.size());
}

TEST_F(ProjMgrWorkerUnitTests, LoadFilteredPack_1) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::*Gen*" }, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  // Check if only one pack is loaded
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM.RteTestGenerator.0.1.0", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadFilteredPack_2) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM" }, "Test");

  // get list of available packs
  vector<string> availablePacks;
  EXPECT_TRUE(ListPacks(availablePacks, false, "Test"));
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  ASSERT_EQ(availablePacks.size(), m_loadedPacks.size());
}

TEST_F(ProjMgrWorkerUnitTests, LoadFilteredPack_3) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::RteTest_D*" }, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM.RteTest_DFP.0.2.0", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadFilteredPack_4) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::*" }, "Test");

  // get list of available packs
  vector<string> availablePacks;
  EXPECT_TRUE(ListPacks(availablePacks, false, "Test"));
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  ASSERT_EQ(availablePacks.size(), m_loadedPacks.size());
}

TEST_F(ProjMgrWorkerUnitTests, LoadFilteredPack_5) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::RteTest_DFP@0.2.0" }, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM.RteTest_DFP.0.2.0", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadPack_Filter_Unknown) {
  CsolutionItem csolution;
  StdStreamRedirect streamRedirect;
  string expected = "no match found for pack filter: keil::*";
  SetCsolutionPacks(&csolution, { "keil::*" }, "Test");
  ContextItem context;
  EXPECT_FALSE(LoadPacks(context));
  ASSERT_EQ(0, m_loadedPacks.size());
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrWorkerUnitTests, GetAccessSequence) {
  string src, sequence;
  size_t offset = 0;

  src = "Option=$Dname$ - $Dboard$";
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, 14);
  EXPECT_EQ(sequence, "Dname");
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, 25);
  EXPECT_EQ(sequence, "Dboard");
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, string::npos);

  src = "DEF=$Output(project)$";
  offset = 0;
  EXPECT_TRUE(GetAccessSequence(offset, src, sequence, '$', '$'));
  EXPECT_EQ(offset, 21);
  EXPECT_EQ(sequence, "Output(project)");
  offset = 0;
  EXPECT_TRUE(GetAccessSequence(offset, sequence, sequence, '(', ')'));
  EXPECT_EQ(offset, 15);
  EXPECT_EQ(sequence, "project");

  src = "Option=$Dname";
  offset = 0;
  EXPECT_FALSE(GetAccessSequence(offset, src, sequence, '$', '$'));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Invalid_Device_Name) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test.cproject_device_unknown.yml";
  const string& expected = "specified device 'RteTest_ARM_UNKNOWN' was not found";
  StdStreamRedirect streamRedirect;

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_FALSE(ProcessDevice(context));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Invalid_Device_Vendor) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestProject/test.cproject_device_unknown_vendor.yml";
  const string& expected = "specified device 'RteTest_ARMCM0' was not found";
  StdStreamRedirect streamRedirect;

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_FALSE(ProcessDevice(context));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_PName) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestProject/test.cproject_device_unknown_processor.yml";
  const string& expected = "processor name 'NOT_AVAILABLE' was not found";
  StdStreamRedirect streamRedirect;

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_FALSE(ProcessDevice(context));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_With_Board_And_Device_Info) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestProject/test.cproject_board_and_device.yml";

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessPrecedences_With_Only_Board) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test.cproject_only_board.yml";
  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Invalid_Board_Vendor) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestProject/test.cproject_board_vendor_invalid.yml";
  const string& expected = "board 'UNKNOWN::RteTest Dummy board' was not found";
  StdStreamRedirect streamRedirect;

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_FALSE(ProcessDevice(context));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Invalid_Board_Name) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestProject/test.cproject_board_name_invalid.yml";
  const string& expected = "board 'Keil::RteTest_unknown' was not found";
  StdStreamRedirect streamRedirect;

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_FALSE(ProcessDevice(context));
  auto errStr = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errStr.find(expected));
}

TEST_F(ProjMgrWorkerUnitTests, GetDeviceItem) {
  std::map<std::string, DeviceItem> input = {
    // {input, expected output}
    {"Vendor::Name:Processor", {"Vendor", "Name", "Processor"}},
    {"Name:Processor",         {"", "Name", "Processor"}},
    {"::Name:Processor",       {"", "Name", "Processor"}},
    {":Processor",             {"", "", "Processor"}},
    {"Vendor::Name:",          {"Vendor", "Name", ""}},
    {"::Name:",                {"", "Name", ""}},
    {"::Name",                 {"", "Name", ""}},
    {"Name",                   {"", "Name", ""}},
  };

  for (const auto& in : input) {
    DeviceItem item;
    GetDeviceItem(in.first, item);
    EXPECT_EQ(in.second.name, item.name);
    EXPECT_EQ(in.second.vendor, item.vendor);
    EXPECT_EQ(in.second.pname, item.pname);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevicePrecedence) {
  struct TestInfo {
    std::string elem1;
    std::string elem2;
    std::string elem3;
    bool expectedReturnVal;
    std::string expectedOutput;
  };

  std::vector<TestInfo> inputs = {
    // input1, input2, input3, expectedreturnVal, expectedOutput
    // positive tests
    { "name", "", "",                                 true, "name"},
    { "", "::name", "name",                           true, "name"},
    { "name:processor", "", "",                       true, "name:processor"},
    { ":processor", "vendor::name", "",               true, "vendor::name:processor"},
    { ":processor", "::name:processor", "::name",     true, "name:processor"},
    { "vendor::name", ":processor", "name",           true, "vendor::name:processor"},
    { ":processor", "vendor::name:processor", "name", true, "vendor::name:processor"},
    { "", "", "",                                     true, ""},
    { ":processor", "", "",                           true, ":processor"},
    // negative tests
    { "name:processor", "", "name:processor1",        false, ""},
    { ":processor", "vendor::name:processor1", "name",false, ""},
    { ":processor", "vendor::name:processor", "vendor::name:processor2", false, ""},
  };

  for (auto& in : inputs) {
    std::string out   = "";
    std::string elem1 = in.elem1;
    std::string elem2 = in.elem2;
    std::string elem3 = in.elem3;

    StringCollection item = { &out, { &elem1, &elem2, &elem3 } };
    EXPECT_EQ(in.expectedReturnVal, ProcessDevicePrecedence(item));
    EXPECT_STREQ(item.assign->c_str(), in.expectedOutput.c_str());
  }
}

TEST_F(ProjMgrWorkerUnitTests, GetBoardItem) {
  std::map<std::string, BoardItem> input = {
    // {input, expected output}
    {"Vendor::Name", {"Vendor", "Name"}},
    {"Name",         {"", "Name"}},
    {"::Name",       {"", "Name"}},
    {"",             {"", ""}},
  };

  for (const auto& in : input) {
    BoardItem item;
    GetBoardItem(in.first, item);
    EXPECT_EQ(in.second.name, item.name);
    EXPECT_EQ(in.second.vendor, item.vendor);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ApplyFilter) {
  std::set<std::string> input = { "TestString1", "FilteredString", "TestString2" };
  std::set<std::string> filter = { "String", "Filtered", "" };
  std::set<std::string> expected = { "FilteredString" };
  std::set<std::string> result;
  ApplyFilter(input, filter, result);
  EXPECT_EQ(expected, result);
}
