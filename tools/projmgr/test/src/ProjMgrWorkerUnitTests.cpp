/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrLogger.h"
#include "ProjMgrTestEnv.h"

#include "RteFsUtils.h"

#include "CrossPlatformUtils.h"

#include "gtest/gtest.h"

using namespace std;

class ProjMgrWorkerUnitTests : public ProjMgrWorker, public ::testing::Test {
protected:
  ProjMgrParser parser;
  ProjMgrWorkerUnitTests() : ProjMgrWorker(&parser, nullptr) {}
  virtual ~ProjMgrWorkerUnitTests() {}

  void SetCsolutionPacks(CsolutionItem* csolution, std::vector<std::string> packs, std::string targetType);
};

void ProjMgrWorkerUnitTests::SetCsolutionPacks(CsolutionItem* csolution, std::vector<std::string> packs, std::string targetType) {
  ContextItem context;
  static CprojectItem cproject;
  context.cproject = &cproject;
  for (auto& pack : packs) {
    csolution->packs.push_back(PackItem{pack, {}});
  }
  context.csolution = csolution;
  context.type.target = targetType;
  CollectionUtils::PushBackUniquely(m_ymlOrderedContexts, targetType);
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

TEST_F(ProjMgrWorkerUnitTests, ProcessToolchainNoToolchainRegistered) {
  // Required to not pick up the in-tree cmake files
  m_compilerRoot = "/path/that/does/not/exist";
  EXPECT_FALSE(RteFsUtils::Exists(m_compilerRoot));

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
  EXPECT_TRUE(std::any_of(m_toolchainErrors.begin(), m_toolchainErrors.end(),
    [](const std::pair<MessageType, StrSet>& log) {
      if (log.first == MessageType::Error) {
        return std::any_of(log.second.begin(), log.second.end(),
          [](const std::string& msg) {
            return msg.find("no toolchain cmake files found") != std::string::npos;
          });
      }
      return false;
    }));
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
    CsolutionItem csolution;
    context.csolution = &csolution;
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
  // Device 'configurable' endianess must not be set among target attributes
  const auto& processor = context.rteFilteredModel->GetDevice(context.deviceItem.name, context.deviceItem.vendor)->GetProcessor(context.deviceItem.pname);
  EXPECT_EQ(processor->GetAttribute("Dendian"), "Configurable");
  EXPECT_TRUE(context.targetAttributes.find("Dendian") == context.targetAttributes.end());
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDeviceUndefLayerVar) {
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  // undefined layer variables
  this->m_undefLayerVars.insert("Board-Layer");
  // multi-core device with pname
  context.device = "RteTest_ARMCM0_Dual:cm0_core0";
  context.targetAttributes.clear();
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_EQ("cm0_core0", context.targetAttributes["Pname"]);
  // multi-core device without pname
  context.device = "RteTest_ARMCM0_Dual";
  context.targetAttributes.clear();
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_TRUE(context.targetAttributes["Pname"].empty());
  // no undefined layer variables
  this->m_undefLayerVars.clear();
  EXPECT_FALSE(ProcessDevice(context));
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

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_EmptyVariant) {
  set<string> expected = {
    "ARM::RteTest:Dependency:Variant@0.9.9",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test exact partial component identifier match with Cvendor
  const string& filename = testinput_folder + "/TestProject/test_component_empty_variant_CM3.cproject.yml";
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

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_EmptyVariantDefault) {
  set<string> expected = {
    "ARM::RteTest:Dependency:Variant&Compatible@0.9.9",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test exact partial component identifier match with Cvendor
  const string& filename = testinput_folder + "/TestProject/test_component_empty_variant_CM0.cproject.yml";
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



TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_Latest_From_MultipleMatches1) {
  set<string> expected = {
    "ARM::Device:Test variant 2@3.3.3",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches, different versions
  const string& filename = testinput_folder + "/TestProject/test_component_latest_match1.cproject.yml";
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

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_Latest_From_MultipleMatches2) {
  set<string> expected = {
    "ARM::Device:Test variant 2@3.3.3",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches
  const string& filename = testinput_folder + "/TestProject/test_component_latest_match2.cproject.yml";
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

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_ExactMatch) {
  set<string> expected = {
    "ARM::Device:Test variant 2@2.2.2",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches
  const string& filename = testinput_folder + "/TestProject/test_component_exact_match.cproject.yml";
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

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_ExactMatch_NotFound) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches
  const string& filename = testinput_folder + "/TestProject/test_component_exact_match_notfound.cproject.yml";
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

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_Highest_Version_Match) {
  set<string> expected = {
    "ARM::Device:Test variant 2@3.3.3",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches
  const string& filename = testinput_folder + "/TestProject/test_component_highest_version_match.cproject.yml";
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

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_Equal_Version_Match) {
  set<string> expected = {
    "ARM::Device:Test variant 2@3.3.3",
    "ARM::RteTest:CORE@0.1.1",
  };
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches
  const string& filename = testinput_folder + "/TestProject/test_component_equal_version_match.cproject.yml";
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

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_Higher_Version_NotFound) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  // Test multiple component identifier matches
  const string& filename = testinput_folder + "/TestProject/test_component_higher_version_notfound.cproject.yml";
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

TEST_F(ProjMgrWorkerUnitTests, ProcessComponents_Csub) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestProject/test_component_csub.cproject.yml";
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
  EXPECT_TRUE(context.components.find("ARM::Board:Test:Rev1@1.1.1") != context.components.end());
  EXPECT_EQ("no component was found with identifier 'Board:Test'", ProjMgrLogger::Get().GetErrors()["test_component_csub"][0]);
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
  const string& filename = testinput_folder + "/TestSolution/TestProject4/test-dependency.cproject.yml";
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
  EXPECT_TRUE(ProcessGpdsc(context));
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
    "/TestSolution/TestProject4/test_device_pname_unavailable_in_board.cproject.yml";
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
  EXPECT_EQ("ARM::RteTest_DFP@0.2.0", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadDuplicatePacksFromDifferentPaths) {
  std::list<std::string> pdscFiles = { // Important that these 2 files contain the *same* vendor/name/version!
    testcmsispack_folder + "/ARM/RteTest_DFP/0.2.0/ARM.RteTest_DFP.pdsc",
    testinput_folder + "/SolutionSpecificPack/ARM.RteTest_DFP.pdsc"
  };

  m_kernel = ProjMgrKernel::Get();
  ASSERT_TRUE(m_kernel);
  m_model = m_kernel->GetGlobalModel();
  ASSERT_TRUE(m_kernel);

  EXPECT_TRUE(m_kernel->LoadAndInsertPacks(m_loadedPacks, pdscFiles));

  // Check if only one pack is loaded
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM::RteTest_DFP@0.2.0", (*m_loadedPacks.begin())->GetPackageID());

  // Check that warning is issued
  EXPECT_FALSE(m_model->Validate());
  const auto& errors = m_model->GetErrors();
  EXPECT_EQ(errors.size(), 1);
  EXPECT_NE(string::npos, errors.front().find("warning #500:"));
}

TEST_F(ProjMgrWorkerUnitTests, LoadRequiredPacks) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::RteTest_DFP@0.2.0"}, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  // Check if only one pack is loaded
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM::RteTest_DFP@0.2.0", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadExactPackVersion) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::RteTest_DFP@0.1.1" }, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  // Check if only one pack is loaded
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM::RteTest_DFP@0.1.1", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadPacksNoPackage) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, {}, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  // by default latest packs available should be loaded
  EXPECT_EQ(8, m_loadedPacks.size());
}

TEST_F(ProjMgrWorkerUnitTests, LoadFilteredPack_1) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::*Gen*" }, "Test");
  ContextItem context;
  EXPECT_TRUE(LoadPacks(context));
  // Check if only one pack is loaded
  ASSERT_EQ(1, m_loadedPacks.size());
  EXPECT_EQ("ARM::RteTestGenerator@0.1.0", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadFilteredPack_2) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM" }, "Test");

  // get list of available packs
  vector<string> availablePacks;
  EXPECT_TRUE(ParseContextSelection({ "Test" }));
  EXPECT_TRUE(ListPacks(availablePacks, false));
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
  EXPECT_EQ("ARM::RteTest_DFP@0.2.0", (*m_loadedPacks.begin())->GetPackageID());
}

TEST_F(ProjMgrWorkerUnitTests, LoadFilteredPack_4) {
  CsolutionItem csolution;
  SetCsolutionPacks(&csolution, { "ARM::*" }, "Test");

  // get list of available packs
  vector<string> availablePacks;
  EXPECT_TRUE(ParseContextSelection({ "Test" }));
  EXPECT_TRUE(ListPacks(availablePacks, false));
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
  EXPECT_EQ("ARM::RteTest_DFP@0.2.0", (*m_loadedPacks.begin())->GetPackageID());
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

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Invalid_Device_Name) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder + "/TestSolution/TestProject4/test_device_unknown.cproject.yml";
  const string& expectedErrStr = R"(error csolution: specified device 'RteTest_ARM_UNKNOWN' was not found among the installed packs.
use 'cpackget' utility to install software packs.)";
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
  EXPECT_NE(string::npos, errStr.find(expectedErrStr));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Invalid_Device_Vendor) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestSolution/TestProject4/test_device_unknown_vendor.cproject.yml";
  const string& expectedErrStr = R"(error csolution: specified device 'TEST::RteTest_ARMCM0' was not found among the installed packs.
use 'cpackget' utility to install software packs.)";
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
  EXPECT_NE(string::npos, errStr.find(expectedErrStr));
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_PName) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestSolution/TestProject4/test_device_unknown_processor.cproject.yml";
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
    "/TestSolution/TestProject4/test_board_and_device.cproject.yml";

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
  const string& filename = testinput_folder + "/TestSolution/TestProject4/test_only_board.cproject.yml";
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
    "/TestSolution/TestProject4/test_board_vendor_invalid.cproject.yml";
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
    "/TestSolution/TestProject4/test_board_name_invalid.cproject.yml";
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

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Exact_Board_From_Multiple_Matches) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestProject/test_exact_board_match.cproject.yml";
  const string& expectedBoard = "Keil::RteTest board test revision:Rev1";

  EXPECT_TRUE(parser.ParseCproject(filename, true));
  EXPECT_TRUE(AddContexts(parser, descriptor, filename));
  map<string, ContextItem>* contexts;
  GetContexts(contexts);
  ContextItem context = contexts->begin()->second;
  EXPECT_TRUE(LoadPacks(context));
  EXPECT_TRUE(ProcessPrecedences(context));
  EXPECT_TRUE(ProcessDevice(context));
  EXPECT_STREQ(context.board.c_str(), expectedBoard.c_str());
}

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Board_Not_Found) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestProject/test_board_not_found.cproject.yml";
  const string& expected = "error csolution: board 'Keil::RteTest Dummy board:Rev10' was not found";
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

TEST_F(ProjMgrWorkerUnitTests, ProcessDevice_Multiple_Board_Matches) {
  ProjMgrParser parser;
  ContextDesc descriptor;
  const string& filename = testinput_folder +
    "/TestProject/test_board_with_multiple_matches.cproject.yml";
  const string& expected = "error csolution: multiple boards were found for identifier 'Keil::RteTest board test revision'";
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
    {"Vendor::Name",          {"Vendor", "Name"}},
    {"Name",                  {"", "Name"}},
    {"::Name",                {"", "Name"}},
    {"",                      {"", ""}},
    {"Vendor::Name:Revision", {"Vendor", "Name", "Revision"}},
    {"Name:Revision",         {"", "Name", "Revision"}},
    {"::Name:Revision",       {"", "Name", "Revision"}},
    {":Revision",             {"", "", "Revision"}},
  };

  for (const auto& in : input) {
    BoardItem item;
    GetBoardItem(in.first, item);
    EXPECT_EQ(in.second.name, item.name);
    EXPECT_EQ(in.second.vendor, item.vendor);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ProcessComponentFilesEmpty) {
  // test ProcessComponentFiles over a component without files
  ContextItem context;
  LoadPacks(context);
  InitializeTarget(context);
  RtePackage* pack = new RtePackage(NULL);
  RteComponent* c = new RteComponent(pack);
  RteComponentInstance* ci = new RteComponentInstance(c);
  ci->InitInstance(c);
  ci->SetAttributes({ {"Cclass" , "Class"}, {"Cgroup" , "Group"} });
  context.components.insert({ "Class:Group", { ci } });
  EXPECT_TRUE(ProcessComponentFiles(context));

  // test ProcessComponentFiles over component with a non-filtered file
  ci->SetResolvedComponent(c, context.rteActiveTarget->GetName());
  RteFile file = RteFile(c);
  file.SetAttributes({ {"category" , "source"}, {"name" , "path/"} });
  context.rteActiveTarget->AddFile(&file, ci);
  EXPECT_TRUE(ProcessComponentFiles(context));
}

TEST_F(ProjMgrWorkerUnitTests, GetAllCombinations) {
  const string strOrangeA = "OrangeA";
  const string strOrangeB = "OrangeB";
  const string strOrangeC = "OrangeC";
  const string strAnanasA = "AnanasA";
  const string strBananaA = "BananaA";
  const string strBananaB = "BananaB";
  ConnectionsCollection OrangeA = { strOrangeA, RteUtils::EMPTY_STRING };
  ConnectionsCollection OrangeB = { strOrangeB, RteUtils::EMPTY_STRING };
  ConnectionsCollection OrangeC = { strOrangeC, RteUtils::EMPTY_STRING };
  ConnectionsCollection AnanasA = { strAnanasA, RteUtils::EMPTY_STRING };
  ConnectionsCollection BananaA = { strBananaA, RteUtils::EMPTY_STRING };
  ConnectionsCollection BananaB = { strBananaB, RteUtils::EMPTY_STRING };

  ConnectionsCollectionMap connections = {
    {"Orange", {OrangeA, OrangeB, OrangeC}},
    {"Ananas", {AnanasA}},
    {"Banana", {BananaA, BananaB}},
  };
  const vector<ConnectionsCollectionVec> expected = {
    {AnanasA, BananaA, OrangeA},
    {AnanasA, BananaA, OrangeB},
    {AnanasA, BananaA, OrangeC},
    {AnanasA, BananaB, OrangeA},
    {AnanasA, BananaB, OrangeB},
    {AnanasA, BananaB, OrangeC},
  };
  vector<ConnectionsCollectionVec> combinations;
  GetAllCombinations(connections, connections.begin(), combinations);

  auto it = combinations.begin();
  for (const auto& expectedItem : expected) {
    EXPECT_EQ(expectedItem.front().filename, (*it++).front().filename);
  }
}

TEST_F(ProjMgrWorkerUnitTests, GetAllSelectCombinations) {
  ConnectItem connectA = { "A" };
  ConnectItem connectB = { "B" };
  ConnectItem connectC = { "C" };
  ConnectItem connectD = { "D" };
  ConnectItem connectX = { "X" };
  ConnectItem connectY = { "Y" };
  const ConnectItem* A = { &connectA };
  const ConnectItem* B = { &connectB };
  const ConnectItem* C = { &connectC };
  const ConnectItem* D = { &connectD };
  const ConnectItem* X = { &connectX };
  const ConnectItem* Y = { &connectY };
  ConnectPtrMap connections = {
    {"0001", { X    } },
    {"0002", { Y    } },
    {"set1", { A, B } },
    {"set2", { C, D } },
  };
  const vector<ConnectPtrVec> expected = {
    {X, Y, A, C},
    {X, Y, A, D},
    {X, Y, B, C},
    {X, Y, B, D},
  };
  vector<ConnectPtrVec> combinations;
  GetAllSelectCombinations(connections, connections.begin(), combinations);

  auto it = combinations.begin();
  for (const auto& expectedItem : expected) {
    EXPECT_EQ(expectedItem.front()->connect, (*it++).front()->connect);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ValidateConnections) {
  ConnectionsValidationResult result;

  // valid connections
  StrPairVec consumedList = {
    {"Orange", "3"},
    {"Grape Fruit", ""},
    {"Peach", ""},
    {"Lime", "+98"},
    {"Lime", "+2"},
    {"Lemon", "+150"},
    {"Lemon", "+20"},
  };
  StrPairVec providedList = {
    {"Orange", "3"},                  // both key and value exact match
    {"Grape Fruit", "999"},           // key exact match, consumed value is empty
    {"Peach", ""},                    // key exact match, both values empty
    {"Lemon", "200"},                 // added consumed values are less than provided
    {"Lime", "100"},                  // added consumed values are equal to provided
  };

  ConnectItem validConnectItem = { RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, providedList, consumedList };
  ConnectionsCollection validCollection = { ".cproject.yml", RteUtils::EMPTY_STRING, {&validConnectItem} };
  result = ValidateConnections({ validCollection });
  EXPECT_TRUE(result.valid);

  // invalid connections
  // same interface is provided multiple times with non identical values
  consumedList = {
    {"Lemon", "+150"},
    {"Lemon", "+20"},
    {"Ananas", "98"},
    {"Grape Fruit", "1"},
  };
  providedList = {
    {"Ananas", "97"},                 // consumed interface doesn't match provided one
    {"Grape Fruit", ""},              // consumed interface doesn't match empty provided one
    {"Lemon", "160"},                 // sum of consumed added values is higher than provided value
    {"Ananas", "2"}, {"Ananas", "2"}, // same interface is provided multiple times with identical values
    {"Orange", "3"}, {"Orange", "4"}, // same interface is provided multiple times with non identical values
    {"Banana", ""}, {"Banana", "0"},  // same interface is provided multiple times with non identical values
  };
  StrVec expectedConflicts = { "Ananas", "Orange", "Banana" };
  StrPairVec expectedOverflow = {{"Lemon", "170 > 160"}};
  StrPairVec expectedIncompatibles = {{"Ananas", "98"}, {"Grape Fruit", "1"}};
  ConnectItem invalidConnectItem = { RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, providedList, consumedList };
  ConnectionsCollection invalidCollection = { ".cproject.yml", RteUtils::EMPTY_STRING, {&invalidConnectItem} };
  result = ValidateConnections({ invalidCollection });
  EXPECT_FALSE(result.valid);
  EXPECT_EQ(result.conflicts, expectedConflicts);
  EXPECT_EQ(result.overflows, expectedOverflow);
  EXPECT_EQ(result.incompatibles, expectedIncompatibles);
}

TEST_F(ProjMgrWorkerUnitTests, ValidateActiveConnects) {
  StrPairVec consumedList1 = {{ "Ananas", "" }};
  StrPairVec consumedList2 = {{ "Banana", "" }};
  StrPairVec consumedList3 = {{ "Cherry", "" }};
  StrPairVec providedList1 = consumedList1;
  StrPairVec providedList2 = consumedList2;
  StrPairVec providedList3 = consumedList3;
  ConnectItem empty               = { RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, StrPairVec() , StrPairVec()  };
  ConnectItem consumed1           = { RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, StrPairVec() , consumedList1 };
  ConnectItem provided1           = { RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, providedList1, StrPairVec()  };
  ConnectItem consumed2           = { RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, StrPairVec() , consumedList2 };
  ConnectItem provided2           = { RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, providedList2, StrPairVec()  };
  ConnectItem consumed2_provided1 = { RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, providedList1, consumedList2 };
  ConnectItem consumed2_provided3 = { RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, RteUtils::EMPTY_STRING, providedList3, consumedList2 };

  // test with active cproject connect
  ConnectionsCollectionVec collectionVec1 = {
    { "0.cproject.yml", RteUtils::EMPTY_STRING, { &consumed1           }},
    { "1.clayer.yml"  , RteUtils::EMPTY_STRING, { &consumed2_provided1 }},
    { "2.clayer.yml"  , RteUtils::EMPTY_STRING, { &provided2           }},
  };
  ConnectionsValidationResult result1 = ValidateConnections(collectionVec1);
  EXPECT_TRUE(result1.valid);
  EXPECT_TRUE(result1.activeConnectMap[&consumed1]);
  EXPECT_TRUE(result1.activeConnectMap[&consumed2_provided1]);
  EXPECT_TRUE(result1.activeConnectMap[&provided2]);

  // test with active cproject connect and inactive layer connect
  ConnectionsCollectionVec collectionVec2 = {
    { "0.cproject.yml", RteUtils::EMPTY_STRING, { &consumed1                       }},
    { "1.clayer.yml"  , RteUtils::EMPTY_STRING, { &consumed2_provided3, &provided1 }},
  };
  ConnectionsValidationResult result2 = ValidateConnections(collectionVec2);
  EXPECT_TRUE(result2.valid);
  EXPECT_TRUE(result2.activeConnectMap[&consumed1]);
  EXPECT_TRUE(result2.activeConnectMap[&provided1]);

  // test with empty cproject connect and active layer and matching connects
  ConnectionsCollectionVec collectionVec3 = {
    { "0.cproject.yml", RteUtils::EMPTY_STRING, { &empty               }},
    { "1.clayer.yml"  , RteUtils::EMPTY_STRING, { &consumed1           }},
    { "2.clayer.yml"  , RteUtils::EMPTY_STRING, { &consumed2_provided1 }},
    { "3.clayer.yml"  , RteUtils::EMPTY_STRING, { &provided2           }},
  };
  ConnectionsValidationResult result3 = ValidateConnections(collectionVec3);
  EXPECT_TRUE(result3.valid);
  EXPECT_TRUE(result3.activeConnectMap[&empty]);
  EXPECT_TRUE(result3.activeConnectMap[&consumed1]);
  EXPECT_TRUE(result3.activeConnectMap[&consumed2_provided1]);
  EXPECT_TRUE(result3.activeConnectMap[&provided2]);

  // test with empty cproject connect and mismatching provided vs consumed
  ConnectionsCollectionVec collectionVec4 = {
    { "0.cproject.yml", RteUtils::EMPTY_STRING, { &empty     }},
    { "1.clayer.yml"  , RteUtils::EMPTY_STRING, { &consumed1 }},
    { "2.clayer.yml"  , RteUtils::EMPTY_STRING, { &provided2 }},
  };
  ConnectionsValidationResult result4 = ValidateConnections(collectionVec4);
  EXPECT_FALSE(result4.valid);
  EXPECT_EQ(consumedList1.front(), result4.incompatibles.front());
  EXPECT_EQ(providedList2.front(), result4.missedCollections.front().connections.front()->provides.front());
}

TEST_F(ProjMgrWorkerUnitTests, CollectLayersFromPacks) {
  // test CollectLayersFromPacks with an non-existent clayer file
  InitializeModel();
  ContextItem context;
  InitializeTarget(context);
  const map<string, string> packAttributes = {
    {"vendor" , "Vendor"  },
    {"name"   , "Name"    },
    {"version", "8.8.8"   }
  };
  const map<string, string> clayerAttributes = {
    {"name"   , "TestVariant"  },
    {"type"   , "TestVariant"  },
    {"file"   , "Invalid/Path" }
  };
  RteModel* model = context.rteActiveTarget->GetModel();
  RtePackage* pack = new RtePackage(model, packAttributes);
  RteItem* clayersItem = pack->CreateChild("csolution");
  RteItem* clayerItem = clayersItem->CreateChild("clayer");
  clayerItem->SetAttributes(clayerAttributes);
  model->InsertPacks(list<RtePackage*>{pack});
  context.rteActiveTarget->UpdateFilterModel();
  StrVecMap clayers;
  EXPECT_FALSE(CollectLayersFromPacks(context, clayers));
}

TEST_F(ProjMgrWorkerUnitTests, ListToolchains) {
  const string& cmsisPackRoot = CrossPlatformUtils::GetEnv("CMSIS_COMPILER_ROOT");

  StrVec envVars = {
  "AC6_TOOLCHAIN_6_18_0=" + cmsisPackRoot,
  "AC6_TOOLCHAIN_6_18_1=" + cmsisPackRoot + "/non-existent",
  "AC6_TOOLCHAIN_6_19_0=" + cmsisPackRoot,
  "AC6_TOOLCHAIN_6_6_0=" + cmsisPackRoot,
  "GCC_TOOLCHAIN_11_3_1=" + cmsisPackRoot,
  };
  SetEnvironmentVariables(envVars);

  // list all configured toolchains
  GetRegisteredToolchains();
  vector<ToolchainItem> expected {
    {"AC6", "6.18.0", "", "", cmsisPackRoot, cmsisPackRoot + "/AC6.6.18.0.cmake"},
    {"AC6", "6.19.0", "", "", cmsisPackRoot, cmsisPackRoot + "/AC6.6.18.0.cmake"},
    {"GCC", "11.3.1", "", "", cmsisPackRoot, cmsisPackRoot + "/GCC.11.2.1.cmake"}
  };
  ASSERT_EQ(m_toolchains.size(), 3);
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(m_toolchains[i].name, expected[i].name);
    EXPECT_EQ(m_toolchains[i].version, expected[i].version);
    EXPECT_EQ(m_toolchains[i].root, expected[i].root);
    EXPECT_EQ(m_toolchains[i].config, expected[i].config);
  }
  // with empty cmsis compiler root
  CrossPlatformUtils::SetEnv("CMSIS_COMPILER_ROOT", "");
  m_compilerRoot.clear();
  m_toolchains.clear();
  m_toolchainConfigFiles.clear();
  GetRegisteredToolchains();
  EXPECT_TRUE(m_toolchains.empty());

  // list latest toolchains
  CrossPlatformUtils::SetEnv("CMSIS_COMPILER_ROOT", cmsisPackRoot);
  m_compilerRoot.clear();
  m_toolchains.clear();
  m_toolchainConfigFiles.clear();
  ToolchainItem latestToolchainInfo;
  latestToolchainInfo.name = "AC6";
  GetLatestToolchain(latestToolchainInfo);
  EXPECT_EQ(latestToolchainInfo.version, "6.19.0");
  EXPECT_EQ(latestToolchainInfo.config, cmsisPackRoot + "/AC6.6.18.0.cmake");
  latestToolchainInfo.name = "GCC";
  GetLatestToolchain(latestToolchainInfo);
  EXPECT_EQ(latestToolchainInfo.version, "11.3.1");
  EXPECT_EQ(latestToolchainInfo.config, cmsisPackRoot + "/GCC.11.2.1.cmake");
}

TEST_F(ProjMgrWorkerUnitTests, CheckBoardLayer) {
  ContextItem context;
  context.board = "BoardVendor::BoardName:BoardRevision";

  // test valid forBoard values
  const vector<string> forBoardValues = {
    "",
    "BoardName",
    "BoardName:BoardRevision",
    "BoardVendor::BoardName",
    "BoardVendor::BoardName:BoardRevision",
  };
  for (const auto& forBoard : forBoardValues) {
    ClayerItem clayer;
    clayer.forBoard = forBoard;
    EXPECT_TRUE(CheckBoardDeviceInLayer(context, clayer));
  }

  // test invalid forBoard values
  const vector<string> invalidForBoardValues = {
    "InvalidBoardName",
    "InvalidBoardName:BoardRevision",
    "BoardName:InvalidBoardRevision",
    "InvalidBoardVendor::BoardName",
    "BoardVendor::InvalidBoardName",
    "InvalidBoardVendor::BoardName:BoardRevision",
    "BoardVendor::InvalidBoardName:BoardRevision",
    "BoardVendor::BoardName:InvalidBoardRevision",
  };
  for (const auto& forBoard : invalidForBoardValues) {
    ClayerItem clayer;
    clayer.forBoard = forBoard;
    EXPECT_FALSE(CheckBoardDeviceInLayer(context, clayer));
  }
};

TEST_F(ProjMgrWorkerUnitTests, CheckDeviceLayer) {
  ContextItem context;
  context.device = "DeviceVendor::DeviceName:DevicePname";

  // test valid forDevice values
  const vector<string> forDeviceValues = {
    "",
    "DeviceName"
    ":DevicePname",
    "DeviceName:DevicePname",
    "DeviceVendor::DeviceName",
    "DeviceVendor::DeviceName:DevicePname",
  };
  for (const auto& forDevice : forDeviceValues) {
    ClayerItem clayer;
    clayer.forDevice = forDevice;
    EXPECT_TRUE(CheckBoardDeviceInLayer(context, clayer));
  }

  // test invalid forDevice values
  const vector<string> invalidForDeviceValues = {
    "InvalidDeviceName"
    ":InvalidDevicePname",
    "InvalidDeviceName:DevicePname",
    "DeviceName:InvalidDevicePname",
    "InvalidDeviceVendor::DeviceName",
    "DeviceVendor::InvalidDeviceName",
    "InvalidDeviceVendor::DeviceName:DevicePname",
    "DeviceVendor::InvalidDeviceName:DevicePname",
    "DeviceVendor::DeviceName:InvalidDevicePname",
  };
  for (const auto& forDevice : invalidForDeviceValues) {
    ClayerItem clayer;
    clayer.forDevice = forDevice;
    EXPECT_FALSE(CheckBoardDeviceInLayer(context, clayer));
  }
};

TEST_F(ProjMgrWorkerUnitTests, RemoveRedundantSubsets) {
  ConnectItem connect1 = { "connect1" };
  ConnectItem connect2 = { "connect2" };
  ConnectItem connect3 = { "connect3" };
  const ConnectItem* c1 = { &connect1 };
  const ConnectItem* c2 = { &connect2 };
  const ConnectItem* c3 = { &connect3 };
  ConnectionsCollection A = { "filenameA", RteUtils::EMPTY_STRING, {c1, c2, c3} };
  ConnectionsCollection As = { "filenameA", RteUtils::EMPTY_STRING, {c3, c1} };
  ConnectionsCollection B = { "filenameB", RteUtils::EMPTY_STRING, {c1, c2, c3} };
  ConnectionsCollection Bs = { "filenameB", RteUtils::EMPTY_STRING, {c2} };
  ConnectionsCollection C = { "filenameC", RteUtils::EMPTY_STRING };
  vector<ConnectionsCollectionVec> validConnections = { { Bs }, { A, B }, { A }, { B, A }, { B }, { A, B }, { As }, { C }, { Bs } };
  vector<ConnectionsCollectionVec> expected = { { A }, { B }, { A, B }, { C } };
  RemoveRedundantSubsets(validConnections);
  auto it = validConnections.begin();
  for (const auto& expectedItem : expected) {
    auto it2 = (*it++).begin();
    for (const auto& expectedElement : expectedItem) {
      EXPECT_EQ(expectedElement.filename, (*it2++).filename);
    }
  }
};

TEST_F(ProjMgrWorkerUnitTests, ProcessLinkerOptions) {
  string linkerScriptFile = "path/to/linkerScript_$Compiler$.sct";
  string linkerRegionsFile = "path/to/linkerRegions_$Compiler$.h";
  ContextItem context;
  CprojectItem cproject;
  context.cproject = &cproject;
  cproject.directory = testoutput_folder;
  context.directories.cprj = cproject.directory;
  LinkerItem linker;
  linker.script = linkerScriptFile;
  linker.regions = linkerRegionsFile;
  cproject.linker.push_back(linker);
  context.compiler = "AC6";
  context.variables[RteConstants::AS_COMPILER] = context.compiler;
  string expectedLinkerScriptFile = "path/to/linkerScript_AC6.sct";
  string expectedLinkerRegionsFile = "path/to/linkerRegions_AC6.h";
  EXPECT_TRUE(ProcessLinkerOptions(context));
  EXPECT_EQ(expectedLinkerScriptFile, context.linker.script);
  EXPECT_EQ(expectedLinkerRegionsFile, context.linker.regions);
};

TEST_F(ProjMgrWorkerUnitTests, ProcessLinkerOptions_MultipleCompilers) {
  ContextItem context;
  CprojectItem cproject;
  context.cproject = &cproject;
  ClayerItem clayer;
  context.clayers.insert({ "layer", &clayer });
  SetupItem setup;
  context.cproject->setups.push_back(setup);
  LinkerItem linker;

  string linkerScriptFile = "path/to/linkerScript_AC6.sct";
  string linkerRegionsFile = "path/to/linkerRegions_AC6.h";
  linker.script = linkerScriptFile;
  linker.regions = linkerRegionsFile;
  linker.forCompiler = { "AC6" };
  cproject.linker.push_back(linker);
  cproject.setups.front().linker.push_back(linker);
  clayer.linker.push_back(linker);

  linkerScriptFile = "path/to/linkerScript_GCC.sct";
  linkerRegionsFile = "path/to/linkerRegions_GCC.h";
  linker.script = linkerScriptFile;
  linker.regions = linkerRegionsFile;
  linker.forCompiler = { "GCC" };
  cproject.linker.push_back(linker);
  cproject.setups.front().linker.push_back(linker);
  clayer.linker.push_back(linker);

  context.compiler = "AC6";
  string expectedLinkerScriptFile = "path/to/linkerScript_AC6.sct";
  string expectedLinkerRegionsFile = "path/to/linkerRegions_AC6.h";
  EXPECT_TRUE(ProcessLinkerOptions(context));
  EXPECT_EQ(expectedLinkerScriptFile, context.linker.script);
  EXPECT_EQ(expectedLinkerRegionsFile, context.linker.regions);

  context.linker.script.clear();
  context.linker.regions.clear();
  context.compiler = "GCC";
  expectedLinkerScriptFile = "path/to/linkerScript_GCC.sct";
  expectedLinkerRegionsFile = "path/to/linkerRegions_GCC.h";
  EXPECT_TRUE(ProcessLinkerOptions(context));
  EXPECT_EQ(expectedLinkerScriptFile, context.linker.script);
  EXPECT_EQ(expectedLinkerRegionsFile, context.linker.regions);
};

TEST_F(ProjMgrWorkerUnitTests, ProcessLinkerOptions_Redefinition) {
  string linkerScriptFile = "path/to/linkerScript.sct";
  ContextItem context;
  CprojectItem cproject;
  context.cproject = &cproject;
  cproject.directory = testoutput_folder;
  context.directories.cprj = cproject.directory;
  ClayerItem clayer;
  context.clayers.insert({ "layer", &clayer });
  clayer.directory = testoutput_folder + "/layer";
  SetupItem setup;
  context.cproject->setups.push_back(setup);
  LinkerItem linker;
  linker.script = linkerScriptFile;
  cproject.linker.push_back(linker);
  cproject.setups.front().linker.push_back(linker);
  clayer.linker.push_back(linker);
  StdStreamRedirect streamRedirect;

  string expectedErrStr = "error csolution: redefinition from 'path/to/linkerScript.sct' into 'layer/path/to/linkerScript.sct' is not allowed\n";
  EXPECT_FALSE(ProcessLinkerOptions(context));
  string errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errStr, regex(expectedErrStr)));

  context.linker.script.clear();
  context.linker.regions.clear();
  string linkerRegionsFile = "path/to/linkerRegions.h";
  linker.script.clear();
  linker.regions = linkerRegionsFile;
  cproject.linker.clear();
  cproject.linker.push_back(linker);
  clayer.linker.clear();
  clayer.linker.push_back(linker);

  streamRedirect.ClearStringStreams();
  expectedErrStr = "error csolution: redefinition from 'path/to/linkerRegions.h' into 'layer/path/to/linkerRegions.h' is not allowed\n";
  EXPECT_FALSE(ProcessLinkerOptions(context));
  errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errStr, regex(expectedErrStr)));
};

TEST_F(ProjMgrWorkerUnitTests, SetDefaultLinkerScript) {
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
  context.directories.cprj = testinput_folder;
  context.directories.rte = "";

  string expectedLinkerScriptFile = "Device/RteTest_ARMCM0/ac6_linker_script.sct.src";
  string expectedLinkerRegionsFile = "Device/RteTest_ARMCM0/regions_RteTest_ARMCM0.h";
  SetDefaultLinkerScript(context);
  EXPECT_EQ(expectedLinkerScriptFile, context.linker.script);
  EXPECT_EQ(expectedLinkerRegionsFile, context.linker.regions);
};

TEST_F(ProjMgrWorkerUnitTests, SetDefaultLinkerScript_UnknownCompiler) {
  StdStreamRedirect streamRedirect;
  ContextItem context;
  context.toolchain.name = "Unknown";
  string expectedErrStr = "warning csolution: linker script template for compiler 'Unknown' was not found\n";
  SetDefaultLinkerScript(context);
  EXPECT_TRUE(context.linker.script.empty());
  string errStr = streamRedirect.GetErrorString();
  EXPECT_EQ(errStr, expectedErrStr);
};

TEST_F(ProjMgrWorkerUnitTests, GenerateRegionsHeader) {
  StdStreamRedirect streamRedirect;
  ContextItem context;
  LoadPacks(context);
  InitializeTarget(context);

  // Generation fails
  string generatedRegionsFile;
  EXPECT_FALSE(GenerateRegionsHeader(context, generatedRegionsFile));
  EXPECT_TRUE(generatedRegionsFile.empty());
  string expectedErrStr = "warning csolution: regions header file generation failed\n";
  string errStr = streamRedirect.GetErrorString();
  EXPECT_EQ(errStr, expectedErrStr);

  // Generation succeeds
  context.targetAttributes["Dvendor"] = "ARM";
  context.targetAttributes["Dname"] = "RteTest_ARMCM0";
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  context.directories.cprj = testoutput_folder;
  context.directories.rte = "./RTE";
  EXPECT_TRUE(GenerateRegionsHeader(context, generatedRegionsFile));
  EXPECT_TRUE(fs::equivalent(generatedRegionsFile, testoutput_folder + "/RTE/Device/RteTest_ARMCM0/regions_RteTest_ARMCM0.h"));
};

TEST_F(ProjMgrWorkerUnitTests, CheckAndGenerateRegionsHeader) {
  StdStreamRedirect streamRedirect;
  string expectedErrStr, expectedOutStr, errStr, outStr;
  ContextItem context;
  LoadPacks(context);
  InitializeTarget(context);

  // Generation fails
  context.directories.cprj = testoutput_folder;
  context.linker.regions = "regions_RteTest_ARMCM0.h";
  CheckAndGenerateRegionsHeader(context);
  expectedErrStr = "\
warning csolution: regions header file generation failed\n\
.*/regions_RteTest_ARMCM0.h - warning csolution: specified regions header was not found\n\
";
  errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(errStr, regex(expectedErrStr)));

  // Generation succeeds but specified file does not exist
  context.targetAttributes["Dvendor"] = "ARM";
  context.targetAttributes["Dname"] = "RteTest_ARMCM0";
  EXPECT_TRUE(SetTargetAttributes(context, context.targetAttributes));
  streamRedirect.ClearStringStreams();
  CheckAndGenerateRegionsHeader(context);
  expectedOutStr = ".*/Device/RteTest_ARMCM0/regions_RteTest_ARMCM0.h - info csolution: regions header generated successfully\n";
  expectedErrStr = ".*/regions_RteTest_ARMCM0.h - warning csolution: specified regions header was not found\n";
  outStr = streamRedirect.GetOutString();
  errStr = streamRedirect.GetErrorString();
  EXPECT_TRUE(regex_match(outStr, regex(expectedOutStr)));
  EXPECT_TRUE(regex_match(errStr, regex(expectedErrStr)));

  // Region header already exists, does nothing
  streamRedirect.ClearStringStreams();
  context.linker.regions = "./Device/RteTest_ARMCM0/regions_RteTest_ARMCM0.h";
  CheckAndGenerateRegionsHeader(context);
  EXPECT_TRUE(streamRedirect.GetOutString().empty());
  EXPECT_TRUE(streamRedirect.GetErrorString().empty());
};

TEST_F(ProjMgrWorkerUnitTests, GetGeneratorDir) {
  const string& gpdscFile = testinput_folder + "/TestGenerator/RTE/Device/RteTestGen_ARMCM0/RteTest.gpdsc";
  bool validGpdsc;
  const RteGenerator* generator = ProjMgrUtils::ReadGpdscFile(gpdscFile, validGpdsc)->GetFirstGenerator();

  m_contexts[""] = ContextItem();
  ContextItem& context = m_contexts["project.Build+Target"];
  CsolutionItem csolution;
  CprojectItem cproject;
  ClayerItem clayer;
  const string layerName = "layerName";
  const string generatorId = "RteTestGeneratorIdentifier";
  context.csolution = &csolution;
  context.cproject = &cproject;
  context.clayers[layerName] = &clayer;
  csolution.directory = testoutput_folder + "/SolutionDirectory";
  cproject.directory = csolution.directory + "/ProjectDirectory";
  clayer.directory = cproject.directory + "/LayerDirectory";
  context.directories.cprj = testoutput_folder;
  context.variables["Compiler"] = csolution.target.build.compiler = "AC6";
  context.name = "project.Build+Target";
  cproject.target.device = "RteTest_ARMCM0";
  string genDir;

  // base directory
  csolution.generators.baseDir = "BaseRelativeToSolution";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "../BaseRelativeToSolution/RteTestGeneratorIdentifier");

  cproject.generators.baseDir = "BaseRelativeToProject";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "BaseRelativeToProject/RteTestGeneratorIdentifier");

  clayer.generators.baseDir = "BaseRelativeToLayer";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "LayerDirectory/BaseRelativeToLayer/RteTestGeneratorIdentifier");

  clayer.generators.baseDir = "$SolutionDir()$/$Compiler$";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "../AC6/RteTestGeneratorIdentifier");

  clayer.generators.baseDir = "$ProjectDir(UnknownContext)$";
  EXPECT_FALSE(GetGeneratorDir(generator, context, layerName, genDir));

  // custom options directory
  csolution.generators.options[generatorId].path = "CustomRelativeToSolution";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "../CustomRelativeToSolution");

  cproject.generators.options[generatorId].path = "CustomRelativeToProject";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "CustomRelativeToProject");

  clayer.generators.options[generatorId].path = "CustomRelativeToLayer";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "LayerDirectory/CustomRelativeToLayer");

  clayer.generators.options[generatorId].path = "$SolutionDir()$/$Compiler$";
  EXPECT_TRUE(GetGeneratorDir(generator, context, layerName, genDir));
  EXPECT_EQ(genDir, "../AC6");

  clayer.generators.options[generatorId].path = "$ProjectDir(UnknownContext)$";
  EXPECT_FALSE(GetGeneratorDir(generator, context, layerName, genDir));

  m_contexts.clear();
};

TEST_F(ProjMgrWorkerUnitTests, GetGeneratorDirDefault) {
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

  const string& gpdscFile = testinput_folder + "/TestGenerator/RTE/Device/RteTestGen_ARMCM0/RteTestNoWorkingDir.gpdsc";
  bool validGpdsc;
  const RteGenerator* generator = ProjMgrUtils::ReadGpdscFile(gpdscFile, validGpdsc)->GetFirstGenerator();
  string genDir;

  // default: $ProjectDir()$/generated/<generator-id>
  EXPECT_TRUE(GetGeneratorDir(generator, context, "", genDir));
  EXPECT_EQ(genDir, "generated/RteTestGeneratorIdentifier");
};

TEST_F(ProjMgrWorkerUnitTests, CheckDeviceAttributes) {
  ContextItem context;
  context.device = "TestDevice";
  ProcessorItem userSelection;
  StrMap targetAttributes;
  StdStreamRedirect streamRedirect;

  userSelection.branchProtection = RteConstants::YAML_BP_BTI;
  userSelection.dsp = RteConstants::YAML_ON;
  userSelection.endian = RteConstants::YAML_ENDIAN_BIG;
  userSelection.fpu = RteConstants::YAML_FPU_DP;
  userSelection.mve = RteConstants::YAML_MVE_FP;
  userSelection.trustzone = RteConstants::YAML_TZ_SECURE;
  targetAttributes[RteConstants::RTE_DPACBTI] = RteConstants::RTE_NO_PACBTI;
  targetAttributes[RteConstants::RTE_DDSP] = RteConstants::RTE_NO_DSP;
  targetAttributes[RteConstants::RTE_DENDIAN] = RteConstants::RTE_ENDIAN_LITTLE;
  targetAttributes[RteConstants::RTE_DFPU] = RteConstants::RTE_SP_FPU;
  targetAttributes[RteConstants::RTE_DMVE] = RteConstants::RTE_NO_MVE;
  targetAttributes[RteConstants::RTE_DTZ] = RteConstants::RTE_NO_TZ;

  CheckDeviceAttributes(context, userSelection, targetAttributes);
  auto errStr = streamRedirect.GetErrorString();

  EXPECT_TRUE(errStr.find("warning csolution: device 'TestDevice' does not support 'endian: big'") != string::npos);
  EXPECT_TRUE(errStr.find("warning csolution: device 'TestDevice' does not support 'fpu: dp'") != string::npos);
  EXPECT_TRUE(errStr.find("warning csolution: device 'TestDevice' does not support 'dsp: on'") != string::npos);
  EXPECT_TRUE(errStr.find("warning csolution: device 'TestDevice' does not support 'mve: fp'") != string::npos);
  EXPECT_TRUE(errStr.find("warning csolution: device 'TestDevice' does not support 'trustzone: secure'") != string::npos);
  EXPECT_TRUE(errStr.find("warning csolution: device 'TestDevice' does not support 'branch-protection: bti'") != string::npos);
};

TEST_F(ProjMgrWorkerUnitTests, FindMatchingPacksInCbuildPack) {
  ResolvedPackItem match;

  const ResolvedPackItem pack1 = {"ARM::CMSIS@5.8.0", {"ARM::CMSIS@5.8.0", "ARM::CMSIS", "ARM", "ARM::CMSIS@>=5.7.0"}};
  const ResolvedPackItem pack2 = {"Test::PackTest@1.2.3", {"Test::PackTest@1.2.3"}};
  const ResolvedPackItem pack3 = {"Test::PackTest@2.0.0", {"Test::PackTest"}};
  const ResolvedPackItem pack4 = {"Test::Pack4@0.9.0", {"Test::Pack4"}};
  const ResolvedPackItem pack5 = {"SomeVendor::Pack5@0.9.0", {"SomeVendor::Pack5@0.9.0"}};
  const ResolvedPackItem pack6 = {"SomeVendor::Pack6@0.9.1", {"SomeVendor::Pack6@0.9.1"}};
  const vector<ResolvedPackItem> resolvedPacks = {pack1, pack2, pack3, pack4, pack5, pack6};
  vector<string> matches;

  // Should not match anything if pack name is empty
  matches = FindMatchingPackIdsInCbuildPack({}, resolvedPacks);
  EXPECT_EQ(0, matches.size());

  // Project local pack should never match anything
  matches = FindMatchingPackIdsInCbuildPack({"ARM::CMSIS", "/path/to/pack"}, resolvedPacks);
  EXPECT_EQ(0, matches.size());

  // Should match one entry in "selected-by-pack"
  matches = FindMatchingPackIdsInCbuildPack({"ARM::CMSIS"}, resolvedPacks);
  EXPECT_EQ(1, matches.size());
  EXPECT_EQ(matches[0], pack1.pack);

  // Should match one entry (wildcard)
  matches = FindMatchingPackIdsInCbuildPack({"ARM::CM*"}, resolvedPacks);
  EXPECT_EQ(1, matches.size());
  EXPECT_EQ(matches[0], pack1.pack);

  // Vendor matching should match all entries for that vendor
  matches = FindMatchingPackIdsInCbuildPack({"Test"}, resolvedPacks);
  EXPECT_EQ(3, matches.size());
  EXPECT_EQ(matches[0], pack2.pack);
  EXPECT_EQ(matches[1], pack3.pack);
  EXPECT_EQ(matches[2], pack4.pack);

  // Vendor matching with version range should match 2 entries for that vendor
  matches = FindMatchingPackIdsInCbuildPack({"Test@>=1.0.0"}, resolvedPacks);
  EXPECT_EQ(2, matches.size());
  EXPECT_EQ(matches[0], pack2.pack);
  EXPECT_EQ(matches[1], pack3.pack);

  // No pack matches this needle
  matches = FindMatchingPackIdsInCbuildPack({"Test@>=3.0.0"}, resolvedPacks);
  EXPECT_EQ(0, matches.size());

  // More than one match on wildcard that is not in selected-by-pack-list
  matches = FindMatchingPackIdsInCbuildPack({"SomeVendor"}, resolvedPacks);
  EXPECT_EQ(2, matches.size());
  EXPECT_EQ(matches[0], pack5.pack);
  EXPECT_EQ(matches[1], pack6.pack);

  // More than one match on wildcard that is not in selected-by-pack-list
  matches = FindMatchingPackIdsInCbuildPack({"SomeVendor::Pack*"}, resolvedPacks);
  EXPECT_EQ(2, matches.size());
  EXPECT_EQ(matches[0], pack5.pack);
  EXPECT_EQ(matches[1], pack6.pack);
}

TEST_F(ProjMgrWorkerUnitTests, PrintContextErrors) {
  StdStreamRedirect streamRedirect;
  string context1("project.build+target1"), context2("project.build+target2");

  // print errors message specific to context
  m_contextErrMap["project.build+target1"].insert("error test pack 1 missing");
  m_contextErrMap["project.build+target1"].insert("error test pack 1.1 missing");
  m_contextErrMap["project.build+target2"].insert("error test pack 2 missing");
  PrintContextErrors(context1);
  PrintContextErrors(context2);
  auto errMsg = streamRedirect.GetErrorString();
  EXPECT_NE(string::npos, errMsg.find("error test pack 1 missing"));
  EXPECT_NE(string::npos, errMsg.find("error test pack 1.1 missing"));
  EXPECT_NE(string::npos, errMsg.find("error test pack 2 missing"));

  // when no errors occur, print no error messages
  streamRedirect.ClearStringStreams();
  m_contextErrMap.clear();
  PrintContextErrors(context1);
  EXPECT_TRUE(streamRedirect.GetErrorString().empty());
}

TEST_F(ProjMgrWorkerUnitTests, CheckBoardDeviceInLayer) {

  vector<tuple<string, string, bool>> testDataBoard = {
    {"BoardVendor::BoardName:BoardRevision"  , "OtherVendor::BoardName:BoardRevision"  , false},
    {"BoardVendor::BoardName:BoardRevision"  , "BoardVendor::OtherName:BoardRevision"  , false},
    {"BoardVendor::BoardName:BoardRevision"  , "BoardVendor::BoardName:OtherRevision"  , false},
    {""                                      ,              "BoardName"                , false},
    {"BoardVendor::BoardName:BoardRevision"  , "BoardVendor::BoardName:BoardRevision"  ,  true},
    {"BoardVendor::BoardName:BoardRevision"  , "BoardVendor::BoardName"                ,  true},
    {"BoardVendor::BoardName:BoardRevision"  ,              "BoardName"                ,  true},
    {             "BoardName"                , "BoardVendor::BoardName:BoardRevision"  ,  true},
    {             "BoardName"                , "BoardVendor::BoardName"                ,  true},
    {             "BoardName"                ,              "BoardName"                ,  true},
  };
  for (const auto& [board, forBoard, expect] : testDataBoard) {
    ContextItem context;
    ClayerItem clayer;
    context.board = board;
    clayer.forBoard = forBoard;
    EXPECT_EQ(CheckBoardDeviceInLayer(context, clayer), expect);
  }

  vector<tuple<string, string, bool>> testDataDevice = {
  {"DeviceVendor::DeviceName:ProcessorName", "OtherVendor::DeviceName:ProcessorName" , false},
  {"DeviceVendor::DeviceName:ProcessorName", "DeviceVendor::OtherName:ProcessorName" , false},
  {"DeviceVendor::DeviceName:ProcessorName", "DeviceVendor::DeviceName:OtherName"    , false},
  {""                                      ,               "DeviceName"              , false},
  {"DeviceVendor::DeviceName:ProcessorName", "DeviceVendor::DeviceName:ProcessorName",  true},
  {"DeviceVendor::DeviceName:ProcessorName", "DeviceVendor::DeviceName"              ,  true},
  {"DeviceVendor::DeviceName:ProcessorName",               "DeviceName"              ,  true},
  {              "DeviceName"              , "DeviceVendor::DeviceName:ProcessorName",  true},
  {              "DeviceName"              , "DeviceVendor::DeviceName"              ,  true},
  {              "DeviceName"              ,               "DeviceName"              ,  true},
  };
  for (const auto& [device, forDevice, expect] : testDataDevice) {
    ContextItem context;
    ClayerItem clayer;
    context.device = device;
    clayer.forDevice = forDevice;
    EXPECT_EQ(CheckBoardDeviceInLayer(context, clayer), expect);
  }
}

TEST_F(ProjMgrWorkerUnitTests, ValidateContexts) {
  const string& errMsg1 = "\
error csolution: invalid combination of contexts specified in command line:\n\
  target-type does not match for 'project1.Debug+CM1' and 'project1.Debug+AVH'\n\
  target-type does not match for 'project1.Debug+CM3' and 'project1.Debug+AVH'\n\n";

  const string& errMsg2 = "\
error csolution: invalid combination of contexts specified in command line:\n\
  build-type is not unique in 'project1.Release+AVH' and 'project1.Debug+AVH'\n\
  build-type is not unique in 'project2.Release+AVH' and 'project2.Debug+AVH'\n\n";

  vector<std::tuple<vector<string>, bool, string>> vecTestData = {
    // inputContexts, expectedRetval, expectedErrMsg
    { {"project1.Debug+AVH", "project2.Release+AVH", "project1.Debug+CM1", "project1.Debug+CM3"},   false, errMsg1},
    { {"project1.Debug+AVH", "project1.Release+AVH", "project2.Debug+AVH", "project2.Release+AVH"}, false, errMsg2},
    { {"project1.Debug+AVH", "project1.Debug+AVH", "project2.Debug+AVH", "project3.Release+AVH"},   true, ""},
    { {""}, true, ""},
  };

  for (const auto& [inputContexts, expectedRetval, expectedErrMsg] : vecTestData) {
    string input;
    StdStreamRedirect streamRedirect;
    std::for_each(inputContexts.begin(), inputContexts.end(),
      [&](const std::string& item) { input += item + " "; });
    EXPECT_EQ(expectedRetval, ValidateContexts(inputContexts, false)) << "failed for input \"" << input << "\"";
    auto errStr = streamRedirect.GetErrorString();
    EXPECT_STREQ(errStr.c_str(), expectedErrMsg.c_str()) << "failed for input \"" << input << "\"";
  }
}

TEST_F(ProjMgrWorkerUnitTests, GetToolchainConfig) {
  vector<tuple<string, string, bool>> testData = {
    {"AC6", "6.25.0", true},
    {"AC6", "6.18.0", true},
    {"AC6", "6.17.0", true},
    {"AC6", "6.17.0:6.17.0", true},
    {"AC6", "6.25.0:6.25.0", true},
    {"AC6", "6.15.0:6.15.0", false},
    {"GCC", "11.2.1", false}
  };

  string configPath(RteUtils::EMPTY_STRING);
  string selectedConfigVersion(RteUtils::EMPTY_STRING);
  m_toolchainConfigFiles.push_back("/test/etc/AC6.6.16.2.cmake");
  for (auto& [name, version, expect] : testData) {
    bool retVal = GetToolchainConfig(name, version, configPath, selectedConfigVersion);
    EXPECT_EQ(retVal, expect) <<
      "Failed to validate for name: " + name + ", version: " + version;
  }
}
