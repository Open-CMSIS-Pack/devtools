/*
 * Copyright (c) 2025-2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrTestEnv.h"
#include "ProjMgrRpcServer.h"
#include "ProjMgrRpcServerData.h"
#include "ProjMgrLogger.h"

#include "CrossPlatformUtils.h"
#include "ProductInfo.h"
#include "RteFsUtils.h"
#include "yaml-cpp/yaml.h"

#include <fstream>

using namespace std;

class ProjMgrRpcTests : public ProjMgr, public ::testing::Test {
protected:
  ProjMgrRpcTests() {}
  virtual ~ProjMgrRpcTests() {}
  string FormatRequest(const int id, const string& method, const json& params);
  vector<json> RunRpcMethods(const string& strIn);
  string RunRpcMethodsWithContent(const string& strIn);

  string CreateLoadRequests(const string& solution,
    const string& activeTarget = RteUtils::EMPTY_STRING,
    const vector<string>& contextList = RteUtils::EMPTY_STRING_VECTOR
  );
  bool CompareRpcResponse(const json& response, const string& ref);

};

string ProjMgrRpcTests::FormatRequest(const int id, const string& method, const json& params = json()) {
  json request;
  request["jsonrpc"] = "2.0";
  request["id"] = id;
  request["method"] = method;
  if(!params.is_null()) {
    request["params"] = params;
  }
  return request.dump();
}

string ProjMgrRpcTests::CreateLoadRequests(const string& solution, const string& activeTarget, const vector<string>& contextList)
{
  string loadSolutionRequest;
  if(!solution.empty()) {
    auto csolutionPath = testinput_folder + solution;
    loadSolutionRequest = FormatRequest(2, "LoadSolution", json({{ "solution", csolutionPath }, { "activeTarget", activeTarget }}));
    if(!contextList.empty()) {
      YAML::Node cbuildset;
      cbuildset["cbuild-set"]["generated-by"] = "ProjMrgUnitTests";
      for(const auto& context : contextList) {
        cbuildset["cbuild-set"]["contexts"].push_back(map<string, string>{ { "context", context } });
      }
      auto cbuildsetPath = csolutionPath;
      RteUtils::ReplaceAll(cbuildsetPath, ".csolution.yml", ".cbuild-set.yml");

      ofstream cbuildsetFile;
      cbuildsetFile.open(cbuildsetPath, fstream::trunc);
      cbuildsetFile << cbuildset << std::endl;
      cbuildsetFile.close();
    }
  }
  return FormatRequest(1, "LoadPacks") + loadSolutionRequest;
}

bool ProjMgrRpcTests::CompareRpcResponse(const json& response, const string& ref) {
  ifstream fileRef(ref);
  json jsonRef;
  fileRef >> jsonRef;
  return ProjMgrTestEnv::StripAbsoluteFunc(response.dump()) == jsonRef.dump();
}

vector<json> ProjMgrRpcTests::RunRpcMethods(const string& strIn) {
  StdStreamRedirect streamRedirect;
  streamRedirect.SetInString(strIn);
  char* argv[] = {(char*)"csolution", (char*)"rpc"};
  EXPECT_EQ(0, RunProjMgr(2, argv, m_envp));
  string line;
  vector<json> responses;
  istringstream iss(streamRedirect.GetOutString());
  while(getline(iss, line)) {
    responses.push_back(json::parse(line));
  }
  return responses;
}

string ProjMgrRpcTests::RunRpcMethodsWithContent(const string& strIn) {
  StdStreamRedirect streamRedirect;
  streamRedirect.SetInString(strIn);
  char* argv[] = {(char*)"csolution", (char*)"rpc", (char*)"--content-length"};
  EXPECT_EQ(0, RunProjMgr(3, argv, m_envp));
  return streamRedirect.GetOutString();
}

TEST_F(ProjMgrRpcTests, ContentLength) {
  StdStreamRedirect streamRedirect;
  ProjMgrRpcServer server(*this);
  const auto& request = FormatRequest(1, "GetVersion");

  auto requestWithHeader = "Content-Length:46\n\n" + request;
  streamRedirect.SetInString(requestWithHeader);
  auto parsedRequest = server.GetRequestFromStdinWithLength();
  EXPECT_EQ(request, parsedRequest);

  requestWithHeader = "Content-Length:46\r\n\r\n" + request;
  streamRedirect.SetInString(requestWithHeader);
  parsedRequest = server.GetRequestFromStdinWithLength();
  EXPECT_EQ(request, parsedRequest);
}

TEST_F(ProjMgrRpcTests, RpcGetVersion) {
  const auto& requests = FormatRequest(1, "GetVersion");
  const auto& responses = RunRpcMethods(requests);
  EXPECT_EQ("2.0", responses[0]["jsonrpc"]);
  EXPECT_EQ(1, responses[0]["id"]);
  EXPECT_TRUE(responses[0]["result"]["success"]);
  EXPECT_EQ(string(VERSION_STRING), responses[0]["result"]["version"]);
  EXPECT_EQ(string(RPC_API_VERSION), responses[0]["result"]["apiVersion"]);
}

TEST_F(ProjMgrRpcTests, RpcGetVersionWithContent) {
  const auto& requests = "Content-Length:46\n\n" + FormatRequest(1, "GetVersion");
  const auto& responses = RunRpcMethodsWithContent(requests);
  EXPECT_TRUE(responses.find(CrossPlatformUtils::Crlf() + CrossPlatformUtils::Crlf() + "{") != string::npos);
}

TEST_F(ProjMgrRpcTests, RpcLoadSolution) {
  const auto& requests = CreateLoadRequests("/TestRpc/minimal.csolution.yml", "TestHW");
  const auto& responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]["success"]);
  EXPECT_TRUE(responses[1]["result"]["success"]);
}

TEST_F(ProjMgrRpcTests, RpcLoadUndefinedSolution) {
  const auto& requests = CreateLoadRequests("/TestRpc/undefined.csolution.yml");
  const auto& responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]["success"]);
  EXPECT_FALSE(responses[1]["result"]["success"]);
  string msg = responses[1]["result"]["message"];
  EXPECT_TRUE(msg.find("failed to load and process solution") == 0);
}

TEST_F(ProjMgrRpcTests, RpcLoadNotSolution) {
  const auto& requests = CreateLoadRequests("/TestRpc/undefined.yml");
  const auto& responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]["success"]);
  EXPECT_FALSE(responses[1]["result"]["success"]);
  string msg = responses[1]["result"]["message"];
  EXPECT_TRUE(msg.find("is not a *.csolution.yml file") != string::npos);
}

TEST_F(ProjMgrRpcTests, RpcLoadSolutionNoPacks) {
  auto csolutionPath = testinput_folder + "/TestRpc/minimal.csolution.yml";
  const auto& requests = FormatRequest(1, "LoadSolution", json({{ "solution", csolutionPath }, { "activeTarget", "TestHW" }}));
  const auto& responses = RunRpcMethods(requests);
  EXPECT_FALSE(responses[0]["result"]["success"]);
  string msg = responses[0]["result"]["message"];
  EXPECT_EQ(msg, "Packs must be loaded before loading solution");
}

TEST_F(ProjMgrRpcTests, RpcDeviceListNoPacks) {
  const auto requests = FormatRequest(1, "GetDeviceList", json({{"context", ""},{ "namePattern", ""}, {"vendor", ""}})) +
    FormatRequest(2, "GetDeviceInfo", json({{ "id", "ARM::RteTest_ARMCM0"}}));
  const auto& responses = RunRpcMethods(requests);
  EXPECT_FALSE(responses[0]["result"]["success"]);
  string msg = responses[0]["result"]["message"];
  EXPECT_EQ(msg, "Packs must be loaded before accessing device info");
  EXPECT_FALSE(responses[1]["result"]["success"]);
  msg = responses[1]["result"]["message"];
  EXPECT_EQ(msg, "Packs must be loaded before accessing device info");
}

TEST_F(ProjMgrRpcTests, RpcDeviceListNoContext) {
  auto requests = CreateLoadRequests(""); // no solution, no context
  // all devices
  requests += FormatRequest(2, "GetDeviceList", json({{"context", ""},{ "namePattern", ""}, {"vendor", ""}}));
  // filtered devices
  requests += FormatRequest(3, "GetDeviceList", json({{"context", ""},{ "namePattern", "*CM0"}, {"vendor", "ARM"}}));
  // filtered devices wrong vendor
  requests += FormatRequest(4, "GetDeviceList", json({{"context", ""},{ "namePattern", ""}, {"vendor", "foo"}}));

  const auto responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]["success"]);

  EXPECT_TRUE(responses[1]["result"]["success"]);
  auto deviceList = responses[1]["result"]["devices"];
  EXPECT_EQ(deviceList.size(), 8);
  auto d0 = deviceList[0];
  EXPECT_EQ(d0["id"], "ARM::RteTest_ARMCM0");
  EXPECT_EQ(d0["family"], "RteTest ARM Cortex M");
  EXPECT_EQ(d0["subFamily"], "RteTest ARM Cortex M0");
  EXPECT_EQ(d0["pack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_FALSE(d0.contains("description"));
  EXPECT_FALSE(d0.contains("processors"));
  EXPECT_FALSE(d0.contains("memories"));

  EXPECT_TRUE(responses[2]["result"]["success"]);
  deviceList = responses[2]["result"]["devices"];
  EXPECT_EQ(deviceList.size(), 2);
  auto d1 = deviceList[1];
  EXPECT_EQ(d1["id"], "ARM::RteTestGen_ARMCM0");
  EXPECT_EQ(d1["family"], "RteTestGen ARM Cortex M");
  EXPECT_EQ(d1["subFamily"], "RteTestGen ARM Cortex M0");
  EXPECT_EQ(d1["pack"], "ARM::RteTestGenerator@0.1.0");

  EXPECT_TRUE(responses[3]["result"]["success"]);
  deviceList = responses[3]["result"]["devices"];
  EXPECT_EQ(deviceList.size(), 0);
}

TEST_F(ProjMgrRpcTests, RpcDeviceListContext) {
  string context = "selectable+CM0";
  vector<string> contextList = {
    context
  };
  auto requests = CreateLoadRequests("/Validation/dependencies.csolution.yml", "", contextList);
  // all devices
  requests += FormatRequest(3, "GetDeviceList", json({{"context", "selectable+CM0"},{ "namePattern", ""}, {"vendor", ""}}));
  requests += FormatRequest(4, "GetDeviceList", json({{"context", "selectable+CM0"},{ "namePattern", "*Dual*"}, {"vendor", ""}}));
  const auto responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]["success"]);
  EXPECT_TRUE(responses[1]["result"]["success"]);

  EXPECT_TRUE(responses[2]["result"]["success"]);
  auto deviceList = responses[2]["result"]["devices"];
  EXPECT_EQ(deviceList.size(), 7);
  auto d0 = deviceList[0];
  EXPECT_EQ(d0["id"], "ARM::RteTest_ARMCM0");

  EXPECT_TRUE(responses[3]["result"]["success"]);
  deviceList = responses[3]["result"]["devices"];
  EXPECT_EQ(deviceList.size(), 1);
  d0 = deviceList[0];
  EXPECT_EQ(d0["id"], "ARM::RteTest_ARMCM0_Dual");
}

TEST_F(ProjMgrRpcTests, RpcDeviceInfo) {
  auto requests = CreateLoadRequests(""); // no solution, no context
  // device with name and vendor
  requests += FormatRequest(2, "GetDeviceInfo", json({{ "id", "ARM::RteTest_ARMCM0_Dual"}}));
  // device with name, no vendor
  requests += FormatRequest(3, "GetDeviceInfo", json({{ "id", "RteTest_ARMCM0_Dual"}}));
  // device with name, wrong vendor
  requests += FormatRequest(4, "GetDeviceInfo", json({{ "id", "foo::RteTest_ARMCM0"}}));
  // device with wrong name, no vendor
  requests += FormatRequest(5, "GetDeviceInfo", json({{ "id", "RteTest_Unknown"}}));
  //  empty id
  requests += FormatRequest(6, "GetDeviceInfo", json({{ "id", ""}}));
  //  only vendor
  requests += FormatRequest(7, "GetDeviceInfo", json({{ "id", "ARM::"}}));

  const auto responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]["success"]);
  EXPECT_TRUE(responses[1]["result"]["success"]);
  EXPECT_TRUE(responses[2]["result"]["success"]);
  EXPECT_FALSE(responses[3]["result"]["success"]);
  EXPECT_EQ("Device 'foo::RteTest_ARMCM0' not found", responses[3]["result"]["message"]);
  EXPECT_FALSE(responses[4]["result"]["success"]);
  EXPECT_EQ("Device 'RteTest_Unknown' not found", responses[4]["result"]["message"]);
  EXPECT_FALSE(responses[5]["result"]["success"]);
  EXPECT_EQ("Invalid device ID: ''", responses[5]["result"]["message"]);
  EXPECT_FALSE(responses[6]["result"]["success"]);
  EXPECT_EQ("Invalid device ID: 'ARM::'", responses[6]["result"]["message"]);

  auto d1 = responses[1]["result"]["device"];
  EXPECT_EQ(d1["id"], "ARM::RteTest_ARMCM0_Dual");
  EXPECT_EQ(d1["family"], "RteTest ARM Cortex M");
  EXPECT_EQ(d1["subFamily"], "RteTest ARM Cortex M0");
  EXPECT_EQ(d1["pack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_TRUE(d1.contains("description"));
  EXPECT_FALSE(d1["description"].empty());
  EXPECT_EQ(d1["processors"].size(), 2);
  auto p0 = d1["processors"][0];
  EXPECT_EQ(p0["name"], "cm0_core0");
  EXPECT_EQ(p0["core"], "Cortex-M0");
  RpcArgs::Processor proc;
  from_json(p0, proc);
  EXPECT_TRUE(proc.attributes.has_value());
  XmlItem attributes(proc.attributes.value());
  EXPECT_EQ(attributes.GetAttributesString(),
    "Dclock=10000000 Dcore=Cortex-M0 DcoreVersion=r0p0 Dendian=Configurable Dfpu=NO_FPU Dmpu=NO_MPU Pname=cm0_core0");

  EXPECT_EQ(d1["memories"].size(), 4);
  auto m0 = d1["memories"][0];
  EXPECT_EQ(m0["name"], "FLASH_DUAL");
  EXPECT_EQ(m0["size"], "0x00080000");
  EXPECT_EQ(m0["access"], "rx");

  d1 = responses[2]["result"]["device"];
  EXPECT_EQ(d1["id"], "ARM::RteTest_ARMCM0_Dual");
}

TEST_F(ProjMgrRpcTests, RpcBoardListNoPacks) {
  const auto requests = FormatRequest(1, "GetBoardList", json({{"context", ""},{ "namePattern", ""}, {"vendor", ""}})) +
    FormatRequest(2, "GetBoardInfo", json({{ "id", "ARM::RteTest_ARMCM0"}}));
  const auto& responses = RunRpcMethods(requests);
  EXPECT_FALSE(responses[0]["result"]["success"]);
  string msg = responses[0]["result"]["message"];
  EXPECT_EQ(msg, "Packs must be loaded before accessing board info");
  EXPECT_FALSE(responses[1]["result"]["success"]);
  msg = responses[1]["result"]["message"];
  EXPECT_EQ(msg, "Packs must be loaded before accessing board info");
}

TEST_F(ProjMgrRpcTests, RpcBoardListNoContext) {
  auto requests = CreateLoadRequests(""); // no solution, no context
  // all boards
  requests += FormatRequest(2, "GetBoardList", json({{"context", ""},{ "namePattern", ""}, {"vendor", ""}}));
  // filtered boards
  requests += FormatRequest(3, "GetBoardList", json({{"context", ""},{ "namePattern", "*CM4*"}, {"vendor", "Keil"}}));
  // filtered boards wrong vendor
  requests += FormatRequest(4, "GetBoardList", json({{"context", ""},{ "namePattern", ""}, {"vendor", "foo"}}));

  const auto responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]["success"]);

  EXPECT_TRUE(responses[1]["result"]["success"]);
  auto boardList = responses[1]["result"]["boards"];
  EXPECT_EQ(boardList.size(), 15);
  auto b0 = boardList[1];
  EXPECT_EQ(b0["id"], "Keil::RteTest board listing:Rev.C");
  EXPECT_EQ(b0["pack"], "ARM::RteTestBoard@0.1.0");
  EXPECT_FALSE(b0.contains("description"));
  EXPECT_FALSE(b0.contains("devices"));
  EXPECT_FALSE(b0.contains("memories"));

  EXPECT_TRUE(responses[2]["result"]["success"]);
  boardList = responses[2]["result"]["boards"];
  EXPECT_EQ(boardList.size(), 1);
  b0 = boardList[0];
  EXPECT_EQ(b0["id"], "Keil::RteTest CM4 board:Rev.C");
  EXPECT_EQ(b0["pack"], "ARM::RteTestBoard@0.1.0");

  EXPECT_TRUE(responses[3]["result"]["success"]);
  boardList = responses[3]["result"]["boards"];
  EXPECT_EQ(boardList.size(), 0);
}

TEST_F(ProjMgrRpcTests, RpcBoardListContext) {

  string context = "selectable+CM0";
  vector<string> contextList = {
    context
  };
  auto requests = CreateLoadRequests("/Validation/dependencies.csolution.yml", "", contextList);

  // all boards
  requests += FormatRequest(2, "GetBoardList", json({{"context", "selectable+CM0"},{ "namePattern", ""}, {"vendor", ""}}));

  const auto responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]["success"]);
  EXPECT_TRUE(responses[1]["result"]["success"]);

  EXPECT_TRUE(responses[2]["result"]["success"]);
  auto boardList = responses[2]["result"]["boards"];
  EXPECT_EQ(boardList.size(), 12);
  auto b0 = boardList[1];
  EXPECT_EQ(b0["id"], "Keil::RteTest board test revision:Rev1");
  EXPECT_EQ(b0["pack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_FALSE(b0.contains("description"));
  EXPECT_FALSE(b0.contains("devices"));
  EXPECT_FALSE(b0.contains("memories"));
}

TEST_F(ProjMgrRpcTests, RpcBoardInfo) {
  auto requests = CreateLoadRequests(""); // no solution, no context
  // board with name, vendor and revision
  requests += FormatRequest(2, "GetBoardInfo", json({{ "id", "Keil::RteTest Test board:1.1.1"}}));
  // board with name, no vendor
  requests += FormatRequest(3, "GetBoardInfo", json({{ "id", "RteTest Test board:1.1.1"}}));
  // board with name, no revision
  requests += FormatRequest(4, "GetBoardInfo", json({{ "id", "Keil::RteTest Test board"}}));
  // board with memory
  requests += FormatRequest(5, "GetBoardInfo", json({{ "id", "RteTest CM4 board:Rev.C" }}));
  //  no device
  requests += FormatRequest(6, "GetBoardInfo", json({{ "id", "RteTest NoMCU board"}}));
  //  only vendor => no ID
  requests += FormatRequest(7, "GetBoardInfo", json({{ "id", "Keil::"}}));
  // board with debugger
  requests += FormatRequest(8, "GetBoardInfo", json({{ "id", "Keil::RteTest-Test-board With.Memory:1.1.1" }}));

  const auto responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]["success"]);
  EXPECT_TRUE(responses[1]["result"]["success"]);

  auto b1 = responses[1]["result"]["board"];
  EXPECT_EQ(b1["id"], "Keil::RteTest Test board:1.1.1");
  EXPECT_EQ(b1["pack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(b1["description"], "uVision Simulator");
  auto devices = b1["devices"];
  EXPECT_EQ(devices.size(), 2);
  EXPECT_FALSE(b1.contains("memories"));
  auto d1 = devices[1];
  EXPECT_EQ(d1["id"], "ARM::RteTest_ARMCM0_Dual");
  EXPECT_EQ(d1["processors"].size(), 2);
  EXPECT_EQ(d1["processors"][0]["name"], "cm0_core0");

  auto d0 = devices[0];
  EXPECT_EQ(d0["id"], "ARM::RteTest_ARMCM3");
  EXPECT_EQ(d0["processors"].size(), 1);
  EXPECT_EQ(d0["processors"][0]["name"], "");

  auto b2 = responses[2]["result"]["board"];
  EXPECT_EQ(b2["id"], "Keil::RteTest Test board:1.1.1");
  EXPECT_EQ(b2["pack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(b2["description"], "uVision Simulator");

  EXPECT_FALSE(responses[3]["result"]["success"]);
  EXPECT_EQ(responses[3]["result"]["message"], "Board 'Keil::RteTest Test board' not found");

  auto b4 = responses[4]["result"]["board"];
  EXPECT_EQ(b4["id"], "Keil::RteTest CM4 board:Rev.C");
  EXPECT_EQ(b4["pack"], "ARM::RteTestBoard@0.1.0");
  EXPECT_EQ(b4["description"], "uVision Simulator");
  EXPECT_TRUE(b4.contains("memories"));
  EXPECT_EQ(b4["memories"][0]["name"], "BoardFLASH");

  auto b5 = responses[5]["result"]["board"];
  EXPECT_EQ(b5["id"], "Keil::RteTest NoMCU board");
  EXPECT_EQ(b5["pack"], "ARM::RteTestBoard@0.1.0");
  EXPECT_EQ(b5["description"], "No device board");
  EXPECT_TRUE(b5.contains("memories"));
  EXPECT_EQ(b5["memories"][1]["name"], "BoardRAM");
  EXPECT_FALSE(b5.contains("devices"));

  EXPECT_FALSE(responses[6]["result"]["success"]);
  EXPECT_EQ(responses[6]["result"]["message"], "Invalid board ID: 'Keil::'");

  auto b7 = responses[7]["result"]["board"];
  EXPECT_EQ(b7["id"], "Keil::RteTest-Test-board With.Memory:1.1.1");
  EXPECT_EQ(b7["pack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(b7["description"], "TestBoard with dots in the name and memory");
  EXPECT_EQ(b7["debugger"]["name"], "CMSIS-DAP");
  EXPECT_EQ(b7["debugger"]["protocol"], "swd");
  EXPECT_EQ(b7["debugger"]["clock"], 30000000);
}

/////////
// components
/////////
TEST_F(ProjMgrRpcTests, RpcLoadSolution_UnknownComponent) {
  const string& csolution = testinput_folder + "/TestRpc/unknown-component.csolution.yml";
  auto requests = CreateLoadRequests("/TestRpc/unknown-component.csolution.yml") +
    FormatRequest(3, "GetLogMessages");

  const auto& responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]["success"]);
  EXPECT_TRUE(responses[1]["result"]["success"]);
  EXPECT_EQ("component 'ARM::UNKNOWN:COMPONENT' not found in included packs",
    responses[2]["result"]["errors"][0]);
}

TEST_F(ProjMgrRpcTests, RpcValidateComponents) {
  vector<string> contextList = {
    "selectable+CM0",
    "missing+CM0",
    "conflict+CM0",
    "incompatible+CM0",
    "incompatible-variant+CM0",
  };
  auto requests = CreateLoadRequests("/Validation/dependencies.csolution.yml", "", contextList);
  int id = 3;
  for(const auto& context : contextList) {
    requests += FormatRequest(id++, "ValidateComponents", json({{ "context", context }}));
  }

  const auto& responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]["success"]);
  EXPECT_TRUE(responses[1]["result"]["success"]);

  // selectable
  auto validation = responses[2]["result"]["validation"][0];
  EXPECT_EQ(responses[2]["result"]["result"], "SELECTABLE");
  EXPECT_EQ("ARM::Device:Startup&RteTest Startup@2.0.3", validation["id"]);
  EXPECT_EQ("SELECTABLE", validation["result"]);
  EXPECT_EQ("require RteTest:CORE", validation["conditions"][0]["expression"]);
  EXPECT_EQ("ARM::RteTest:CORE", validation["conditions"][0]["aggregates"][0]);

  // missing
  validation = responses[3]["result"]["validation"][0];
  EXPECT_EQ(responses[3]["result"]["result"], "MISSING");
  EXPECT_EQ("ARM::RteTest:Check:Missing@0.9.9", validation["id"]);
  EXPECT_EQ("MISSING", validation["result"]);
  EXPECT_EQ("require RteTest:Dependency:Missing", validation["conditions"][0]["expression"]);

  // conflict
  validation = responses[4]["result"]["validation"][0];
  EXPECT_EQ(responses[4]["result"]["result"], "CONFLICT");
  EXPECT_EQ("RteTest:ApiExclusive@1.0.0", validation["id"]);
  EXPECT_EQ("CONFLICT", validation["result"]);
  EXPECT_EQ("ARM::RteTest:ApiExclusive:S1", validation["aggregates"][0]);
  EXPECT_EQ("ARM::RteTest:ApiExclusive:S2", validation["aggregates"][1]);

  // incompatible
  validation = responses[5]["result"]["validation"][0];
  EXPECT_EQ(responses[5]["result"]["result"], "INCOMPATIBLE");
  EXPECT_EQ("ARM::RteTest:Check:Incompatible@0.9.9", validation["id"]);
  EXPECT_EQ("INCOMPATIBLE", validation["result"]);
  EXPECT_EQ("deny RteTest:Dependency:Incompatible_component", validation["conditions"][0]["expression"]);
  EXPECT_EQ("ARM::RteTest:Dependency:Incompatible_component", validation["conditions"][0]["aggregates"][0]);

  // incompatible variant
  validation = responses[6]["result"]["validation"][0];
  EXPECT_EQ(responses[6]["result"]["result"], "INCOMPATIBLE_VARIANT");
  EXPECT_EQ("ARM::RteTest:Check:IncompatibleVariant@0.9.9", validation["id"]);
  EXPECT_EQ("INCOMPATIBLE_VARIANT", validation["result"]);
  EXPECT_EQ("require RteTest:Dependency:Variant&Compatible", validation["conditions"][0]["expression"]);
  EXPECT_EQ("ARM::RteTest:Dependency:Variant", validation["conditions"][0]["aggregates"][0]);
}

TEST_F(ProjMgrRpcTests, RpcResolveComponents) {
  string context = "selectable+CM0";
  vector<string> contextList = {
    context
  };
  auto requests = CreateLoadRequests("/Validation/dependencies.csolution.yml", "", contextList);
  requests += FormatRequest(3, "ValidateComponents", json({{ "context", context }}));
  requests += FormatRequest(4, "Resolve", json({{ "context", context }}));
  requests += FormatRequest(5, "ValidateComponents", json({{ "context", context }}));

  const auto& responses = RunRpcMethods(requests);
  // selectable
  auto validation = responses[2]["result"]["validation"][0];
  EXPECT_EQ("ARM::Device:Startup&RteTest Startup@2.0.3", validation["id"]);
  EXPECT_EQ("SELECTABLE", validation["result"]);
  EXPECT_EQ("require RteTest:CORE", validation["conditions"][0]["expression"]);
  EXPECT_EQ("ARM::RteTest:CORE", validation["conditions"][0]["aggregates"][0]);

  EXPECT_TRUE(responses[3]["result"]["success"]); // components resolved

  EXPECT_EQ(responses[4]["result"]["result"], "FULFILLED");
  EXPECT_FALSE(responses[4]["result"].contains("validation"));
}

TEST_F(ProjMgrRpcTests, RpcSelectComponent) {
  string context = "selectable+CM0";
  vector<string> contextList = {
    context
  };
  RpcArgs::Options opt{""};
  json param;
  param["context"] = context;
  param["id"] = "ARM::RteTest:CORE";
  param["count"] = 1;
  param["options"] = json::object();

  auto requests = CreateLoadRequests("/Validation/dependencies.csolution.yml", "", contextList);
  requests += FormatRequest(3, "ValidateComponents", json({{ "context", context }}));
  requests += FormatRequest(4, "GetComponentsTree", json({{ "context", context }, {"all", false}}));
  requests += FormatRequest(5, "SelectComponent", param);
  requests += FormatRequest(6, "ValidateComponents", json({{ "context", context }}));

  const auto& responses = RunRpcMethods(requests);
  // selectable
  auto validation = responses[2]["result"]["validation"][0];
  EXPECT_EQ("ARM::Device:Startup&RteTest Startup@2.0.3", validation["id"]);
  EXPECT_EQ("SELECTABLE", validation["result"]);
  EXPECT_EQ("require RteTest:CORE", validation["conditions"][0]["expression"]);
  EXPECT_EQ("ARM::RteTest:CORE", validation["conditions"][0]["aggregates"][0]);

  EXPECT_FALSE(responses[3]["result"]["classes"][0].contains("result"));
  string res = responses[3]["result"]["classes"][1]["result"];
  EXPECT_EQ(res, "SELECTABLE");

  EXPECT_TRUE(responses[4]["result"]["success"]); // components resolved
  EXPECT_EQ(responses[5]["result"]["result"], "FULFILLED");
  EXPECT_FALSE(responses[5]["result"].contains("validation"));
}

TEST_F(ProjMgrRpcTests, RpcSelectComponentMissing) {
  string context = "missing-component+CM0";
  vector<string> contextList = {
    context
  };
  RpcArgs::Options opt{""};
  json param;
  param["context"] = context;
  param["id"] = "RteTest:Missing:Component";
  param["count"] = 0;
  param["options"] = json::object();

  auto requests = CreateLoadRequests("/Validation/dependencies.csolution.yml", "", contextList);
  requests += FormatRequest(3, "ValidateComponents", json({{ "context", context }}));
  requests += FormatRequest(4, "GetComponentsTree", json({{ "context", context }, {"all", false}}));
  requests += FormatRequest(5, "GetUsedItems", json({{ "context", context }}));
  // unselect component
  requests += FormatRequest(6, "SelectComponent", param);
  requests += FormatRequest(7, "ValidateComponents", json({{ "context", context }}));
  // select component again
  param["count"] = 1;
  requests += FormatRequest(8, "SelectComponent", param);
  requests += FormatRequest(9, "ValidateComponents", json({{ "context", context }}));
  // finally unselect and apply
  param["count"] = 0;
  requests += FormatRequest(10, "SelectComponent", param);
  requests += FormatRequest(11, "ValidateComponents", json({{ "context", context }}));
  requests += FormatRequest(12, "Apply", json({{ "context", context }}));
  requests += FormatRequest(13, "GetUsedItems", json({{ "context", context }}));

  const auto& responses = RunRpcMethods(requests);
  // selectable
  auto validation = responses[2]["result"]["validation"][0];
  EXPECT_EQ("RteTest:Missing:Component", validation["id"]);
  EXPECT_EQ("MISSING", validation["result"]);

  EXPECT_TRUE(responses[3]["result"]["classes"][2].contains("result"));
  string res = responses[3]["result"]["classes"][2]["result"];
  EXPECT_EQ(res, "MISSING");

  auto components = responses[4]["result"]["components"];
  EXPECT_EQ(components.size(), 1);
  EXPECT_EQ(components[0]["id"], "RteTest:Missing:Component");
  EXPECT_EQ(components[0]["selectedCount"], 1);

  EXPECT_TRUE(responses[5]["result"]["success"]);
  EXPECT_EQ(responses[6]["result"]["result"], "IGNORED");
  EXPECT_FALSE(responses[6]["result"].contains("validation"));

  EXPECT_EQ(responses[8]["result"]["result"], "MISSING");

  EXPECT_EQ(responses[10]["result"]["result"], "IGNORED");

  components = responses[12]["result"]["components"];
  EXPECT_EQ(components.size(), 0);
}


TEST_F(ProjMgrRpcTests, RpcSelectVariant) {
  string context = "incompatible-variant+CM0";
  vector<string> contextList = {
    context
  };
  json param;
  param["context"] = context;
  param["id"] = "ARM::RteTest:Dependency:Variant";
  param["variant"] = "Compatible";

  auto requests = CreateLoadRequests("/Validation/dependencies.csolution.yml", "", contextList);
  requests += FormatRequest(3, "ValidateComponents", json({{ "context", context }}));
  requests += FormatRequest(4, "SelectVariant", param);
  requests += FormatRequest(5, "ValidateComponents", json({{ "context", context }}));
  param["variant"] = "undefined";
  requests += FormatRequest(6, "SelectVariant", param);

  const auto& responses = RunRpcMethods(requests);
  // incompatible variant
  auto validation = responses[2]["result"]["validation"][0];
  EXPECT_EQ("ARM::RteTest:Check:IncompatibleVariant@0.9.9", validation["id"]);
  EXPECT_EQ("INCOMPATIBLE_VARIANT", validation["result"]);
  EXPECT_EQ("require RteTest:Dependency:Variant&Compatible", validation["conditions"][0]["expression"]);
  EXPECT_EQ("ARM::RteTest:Dependency:Variant", validation["conditions"][0]["aggregates"][0]);

  EXPECT_TRUE(responses[3]["result"]["success"]); // variant changed

  EXPECT_EQ(responses[4]["result"]["result"], "FULFILLED");
  EXPECT_FALSE(responses[4]["result"].contains("validation"));

  EXPECT_FALSE(responses[5]["result"]["success"]); // variant not changed
  EXPECT_EQ(responses[5]["result"]["message"], "Variant 'undefined' is not found for component ARM::RteTest:Dependency:Variant"); // variant not found
}

TEST_F(ProjMgrRpcTests, RpcSelectBundle) {
  string context = "selectable+CM0";
  vector<string> contextList = {
    context
  };
  json param;
  param["context"] = context;
  param["cclass"] = "RteTestBundle";
  param["bundle"] = "BundleTwo";

  auto requests = CreateLoadRequests("/Validation/dependencies.csolution.yml", "", contextList);
  requests += FormatRequest(3, "SelectBundle", param);
  requests += FormatRequest(4, "SelectBundle", param);
  param["bundle"] = "undefined";
  requests += FormatRequest(5, "SelectBundle", param);
  param["bundle"] = "";
  requests += FormatRequest(6, "SelectBundle", param);
  param["cclass"] = "UnknownCclass";
  requests += FormatRequest(7, "SelectBundle", param);

  const auto& responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[2]["result"]["success"]); // bundle changed
  EXPECT_FALSE(responses[3]["result"]["success"]);   // bundle not changed
  EXPECT_FALSE(responses[4]["result"]["success"]);   // bundle not found
  EXPECT_EQ(responses[4]["result"]["message"], "Bundle 'undefined' is not found for component class 'RteTestBundle'");
  EXPECT_TRUE(responses[5]["result"]["success"]);   // bundle '' found
  EXPECT_EQ(responses[6]["error"]["message"], "UnknownCclass: component class not found");
}

TEST_F(ProjMgrRpcTests, RpcGetContextInfoSingleCoreDevice) {
  string context = "project+CM0";
  vector<string> contextList = {
    context
  };

  auto requests = CreateLoadRequests("/Examples/solution.csolution.yml", "CM0", contextList);
  requests += FormatRequest(3, "GetContextInfo", json({{ "context", context }}));

  const auto& responses = RunRpcMethods(requests);

  EXPECT_TRUE(responses[2]["result"]["success"]);
  auto components = responses[2]["result"]["components"];
  auto packs = responses[2]["result"]["packs"];
  EXPECT_EQ(packs.size(), 2);
  EXPECT_EQ(packs[0]["pack"], "ARM::RteTest_DFP");
  EXPECT_EQ(packs[0]["resolvedPack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(components[0]["id"], "Device:Startup&RteTest Startup");
  EXPECT_EQ(components[0]["resolvedComponent"]["id"], "ARM::Device:Startup&RteTest Startup@2.0.3");

  map<string, string> vars = responses[2]["result"]["variables"];
  EXPECT_EQ(vars["BuildType"], "");
  EXPECT_EQ(vars["Compiler"], "AC6");
  EXPECT_EQ(vars["Dname"], "RteTest_ARMCM0");
  EXPECT_EQ(vars["Dpack"], testcmsispack_folder + "/ARM/RteTest_DFP/0.2.0/");
  EXPECT_EQ(vars["Pname"], "");
  EXPECT_EQ(vars["Project"], "project");
  EXPECT_EQ(vars["Solution"], "solution");
  EXPECT_EQ(vars["TargetType"], "CM0");

  EXPECT_FALSE(responses[2]["result"].contains("board"));
  EXPECT_EQ(responses[2]["result"]["pname"], "");

  auto d0 = responses[2]["result"]["device"];
  EXPECT_EQ(d0["id"], "ARM::RteTest_ARMCM0");
  EXPECT_EQ(d0["family"], "RteTest ARM Cortex M");
  EXPECT_EQ(d0["subFamily"], "RteTest ARM Cortex M0");
  EXPECT_EQ(d0["pack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(d0["processors"].size(), 1);
  EXPECT_EQ(d0["processors"][0]["name"], "");
  EXPECT_EQ(d0["memories"].size(), 2);
  EXPECT_TRUE(d0.contains("description"));

  map<string, string> attrs = responses[2]["result"]["attributes"];
  EXPECT_EQ(attrs["Tcompiler"], "ARMCC");
  EXPECT_EQ(attrs["Toptions"], "AC6");
  EXPECT_EQ(attrs["Dname"], "RteTest_ARMCM0");
  EXPECT_EQ(attrs["Dcore"], "Cortex-M0");
  EXPECT_EQ(attrs["Dfpu"], "NO_FPU");
  EXPECT_EQ(attrs["Pname"], "");
}

TEST_F(ProjMgrRpcTests, RpcGetContextInfoMultiCoreDevice) {
  string context = "project+CM0_Dual";
  vector<string> contextList = {
    context
  };

  auto requests = CreateLoadRequests("/Examples/solution.csolution.yml", "CM0_Dual", contextList);
  requests += FormatRequest(3, "GetContextInfo", json({{ "context", context }}));

  const auto& responses = RunRpcMethods(requests);

  EXPECT_TRUE(responses[2]["result"]["success"]);
  auto components = responses[2]["result"]["components"];
  auto packs = responses[2]["result"]["packs"];
  EXPECT_EQ(packs.size(), 2);
  EXPECT_EQ(packs[0]["pack"], "ARM::RteTest_DFP");
  EXPECT_EQ(packs[0]["resolvedPack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(components[0]["id"], "Device:Startup&RteTest Startup");
  EXPECT_EQ(components[0]["resolvedComponent"]["id"], "ARM::Device:Startup&RteTest Startup@2.0.3");

  map<string, string> vars = responses[2]["result"]["variables"];
  EXPECT_EQ(vars["BuildType"], "");
  EXPECT_EQ(vars["Compiler"], "AC6");
  EXPECT_EQ(vars["Dname"], "RteTest_ARMCM0_Dual");
  EXPECT_EQ(vars["Dpack"], testcmsispack_folder + "/ARM/RteTest_DFP/0.2.0/");
  EXPECT_EQ(vars["Pname"], "cm0_core0");
  EXPECT_EQ(vars["Project"], "project");
  EXPECT_EQ(vars["Solution"], "solution");
  EXPECT_EQ(vars["TargetType"], "CM0_Dual");

  EXPECT_FALSE(responses[2]["result"].contains("board"));
  EXPECT_EQ(responses[2]["result"]["pname"], "cm0_core0");

  auto d0 = responses[2]["result"]["device"];
  EXPECT_EQ(d0["id"], "ARM::RteTest_ARMCM0_Dual");
  EXPECT_EQ(d0["family"], "RteTest ARM Cortex M");
  EXPECT_EQ(d0["subFamily"], "RteTest ARM Cortex M0");
  EXPECT_EQ(d0["pack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(d0["processors"].size(), 2);
  EXPECT_EQ(d0["processors"][0]["name"], "cm0_core0");
  EXPECT_EQ(d0["memories"].size(), 4);
  EXPECT_TRUE(d0.contains("description"));

  map<string, string> attrs = responses[2]["result"]["attributes"];
  EXPECT_EQ(attrs["Tcompiler"], "ARMCC");
  EXPECT_EQ(attrs["Toptions"], "AC6");
  EXPECT_EQ(attrs["Dname"], "RteTest_ARMCM0_Dual");
  EXPECT_EQ(attrs["Dcore"], "Cortex-M0");
  EXPECT_EQ(attrs["Dfpu"], "NO_FPU");
  EXPECT_EQ(attrs["Pname"], "cm0_core0");
}


TEST_F(ProjMgrRpcTests, RpcGetContextInfoBoard) {
  string context = "project+TestBoard";
  vector<string> contextList = {
    context
  };

  auto requests = CreateLoadRequests("/Examples/solution.csolution.yml", "TestBoard", contextList);
  requests += FormatRequest(3, "GetContextInfo", json({{ "context", context }}));

  const auto& responses = RunRpcMethods(requests);

  EXPECT_TRUE(responses[2]["result"]["success"]);
  auto components = responses[2]["result"]["components"];
  auto packs = responses[2]["result"]["packs"];
  EXPECT_EQ(packs.size(), 2);
  EXPECT_EQ(packs[0]["pack"], "ARM::RteTest_DFP");
  EXPECT_EQ(packs[0]["resolvedPack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(components[0]["id"], "Device:Startup&RteTest Startup");
  EXPECT_EQ(components[0]["resolvedComponent"]["id"], "ARM::Device:Startup&RteTest Startup@2.0.3");

  map<string, string> vars = responses[2]["result"]["variables"];
  EXPECT_EQ(vars["BuildType"], "");
  EXPECT_EQ(vars["Compiler"], "AC6");
  EXPECT_EQ(vars["Dname"], "RteTest_ARMCM0_Dual");
  EXPECT_EQ(vars["Dpack"], testcmsispack_folder + "/ARM/RteTest_DFP/0.2.0/");
  EXPECT_EQ(vars["Pname"], "cm0_core0");
  EXPECT_EQ(vars["Project"], "project");
  EXPECT_EQ(vars["Solution"], "solution");
  EXPECT_EQ(vars["TargetType"], "TestBoard");

  EXPECT_EQ(responses[2]["result"]["pname"], "cm0_core0");
  auto d0 = responses[2]["result"]["device"];
  EXPECT_EQ(d0["id"], "ARM::RteTest_ARMCM0_Dual");
  EXPECT_EQ(d0["family"], "RteTest ARM Cortex M");
  EXPECT_EQ(d0["subFamily"], "RteTest ARM Cortex M0");
  EXPECT_EQ(d0["pack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(d0["processors"].size(), 2);
  EXPECT_EQ(d0["processors"][0]["name"], "cm0_core0");
  EXPECT_EQ(d0["memories"].size(), 4);
  EXPECT_TRUE(d0.contains("description"));

  auto b1 = responses[2]["result"]["board"];
  EXPECT_EQ(b1["id"], "Keil::RteTest Dummy board:1.2.3");
  EXPECT_EQ(b1["pack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(b1["description"], "uVision Simulator");
  auto devices = b1["devices"];
  EXPECT_EQ(devices.size(), 1);
  EXPECT_FALSE(b1.contains("memories"));
  auto d1 = devices[0];
  EXPECT_EQ(d1["id"], "ARM::RteTest_ARMCM0_Dual");
  EXPECT_EQ(d1["processors"].size(), 2);
  EXPECT_EQ(d1["processors"][0]["name"], "cm0_core0");

  map<string, string> attrs = responses[2]["result"]["attributes"];
  EXPECT_EQ(attrs["Tcompiler"], "ARMCC");
  EXPECT_EQ(attrs["Toptions"], "AC6");
  EXPECT_EQ(attrs["Dname"], "RteTest_ARMCM0_Dual");
  EXPECT_EQ(attrs["Dcore"], "Cortex-M0");
  EXPECT_EQ(attrs["Dfpu"], "NO_FPU");
  EXPECT_EQ(attrs["Pname"], "cm0_core0");
}

TEST_F(ProjMgrRpcTests, RpcGetUsedItems) {
  string context = "selectable+CM0";
  vector<string> contextList = {
    context
  };
  RpcArgs::Options opt{"core.clayer.yml", "@>=0.1.0", true};
  json param;
  param["context"] = context;
  param["id"] = "ARM::RteTest:CORE";
  param["count"] = 1;
  RpcArgs::to_json(param["options"], opt);

  auto requests = CreateLoadRequests("/Validation/dependencies.csolution.yml", "", contextList);
  requests += FormatRequest(3, "GetUsedItems", json({{ "context", context }}));
  requests += FormatRequest(4, "SelectComponent", param);
  requests += FormatRequest(5, "Apply", json({{ "context", context }}));
  requests += FormatRequest(6, "GetUsedItems", json({{ "context", context }}));

  const auto& responses = RunRpcMethods(requests);

  EXPECT_TRUE(responses[2]["result"]["success"]);
  auto components = responses[2]["result"]["components"];
  auto packs = responses[2]["result"]["packs"];
  EXPECT_EQ(packs.size(), 2);
  EXPECT_EQ(packs[0]["pack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(packs[0]["resolvedPack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(components[0]["id"], "Device:Startup&RteTest Startup");
  EXPECT_EQ(components[0]["resolvedComponent"]["id"], "ARM::Device:Startup&RteTest Startup@2.0.3");

  EXPECT_TRUE(responses[3]["result"]["success"]); // select successful

  EXPECT_TRUE(responses[4]["result"]["success"]); // apply successful

  components = responses[5]["result"]["components"];
  packs = responses[5]["result"]["packs"];
  EXPECT_EQ(packs.size(), 3); // added reference to layer file
  EXPECT_EQ(packs[0]["pack"], "ARM::RteTest_DFP@0.2.0");
  string origin = packs[0]["origin"];
  EXPECT_TRUE(origin.find(".csolution.yml") != string::npos);
  EXPECT_TRUE(!!packs[0]["selected"]);
  EXPECT_EQ(packs[2]["pack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(packs[2]["origin"], "core.clayer.yml");
  EXPECT_TRUE(!!packs[2]["selected"]);

  EXPECT_EQ(components[0]["id"], "Device:Startup&RteTest Startup");
  EXPECT_EQ(components[0]["resolvedComponent"]["id"], "ARM::Device:Startup&RteTest Startup@2.0.3");

  string id = components[1]["id"];
  EXPECT_EQ(id, "ARM::RteTest:CORE@>=0.1.0");
  EXPECT_EQ(RteUtils::ExtractPrefix(id, "::"), "ARM");
  EXPECT_EQ(RteUtils::ExtractSuffix(id, "@", true), "@>=0.1.0");
  EXPECT_EQ(components[1]["resolvedComponent"]["id"], "ARM::RteTest:CORE@0.1.1");
  EXPECT_EQ(components[1]["options"]["layer"], "core.clayer.yml");
  EXPECT_EQ(components[1]["options"]["explicitVersion"], "@>=0.1.0");
  EXPECT_TRUE(components[1]["options"]["explicitVendor"]);
}

TEST_F(ProjMgrRpcTests, RpcGetPacksInfoSimple) {
  string context = "selectable+CM0";
  vector<string> contextList = {
    context
  };

  auto requests = CreateLoadRequests("/Validation/dependencies.csolution.yml", "", contextList);
  requests += FormatRequest(3, "GetUsedItems", json({{ "context", context }}));
  requests += FormatRequest(4, "GetPacksInfo", json({{ "context", context }, {"all", false}}));
  requests += FormatRequest(5, "GetPacksInfo", json({{ "context", context }, {"all", true}}));

  const auto& responses = RunRpcMethods(requests);

  EXPECT_TRUE(responses[2]["result"]["success"]);
  auto packs = responses[2]["result"]["packs"];
  EXPECT_EQ(packs[0]["pack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(packs[0]["resolvedPack"], "ARM::RteTest_DFP@0.2.0");
  string origin = packs[0]["origin"];
  EXPECT_TRUE(origin.find(".csolution.yml") != string::npos);

  EXPECT_TRUE(responses[3]["result"]["success"]); // get pack infos
  auto packInfos = responses[3]["result"]["packs"];
  EXPECT_EQ(packInfos.size(), 2);
  auto& pack = packInfos[1];

  EXPECT_EQ(pack["id"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(pack["doc"], testcmsispack_folder + "/ARM/RteTest_DFP/0.2.0/Doc/overview.md");

  auto& refs = pack["references"];
  EXPECT_EQ(refs.size(), 1);

  EXPECT_TRUE(!pack["description"].empty());

  packInfos = responses[4]["result"]["packs"];
  EXPECT_EQ(packInfos.size(), 8);
  EXPECT_EQ(packInfos[7]["id"], "SomeVendor::RteTest@0.0.1");
  EXPECT_EQ(packInfos[7]["references"].size(), 0);
}

TEST_F(ProjMgrRpcTests, RpcGetPacksInfo) {
  string context = "test1.Release+CM0";
  vector<string> contextList = {
    context
  };

  auto requests = CreateLoadRequests("/TestSolution/test_pack_requirements.csolution.yml", "", contextList);
  requests += FormatRequest(3, "GetUsedItems", json({{ "context", context }}));
  requests += FormatRequest(4, "GetPacksInfo", json({{ "context", context }, {"all", false}}));
  requests += FormatRequest(5, "GetPacksInfo", json({{ "context", context }, {"all", true}}));

  const auto& responses = RunRpcMethods(requests);

  EXPECT_TRUE(responses[2]["result"]["success"]);
  auto packs = responses[2]["result"]["packs"];
  EXPECT_EQ(packs[0]["pack"], "ARM::RteTest_DFP@>=0.2.0");
  EXPECT_EQ(packs[0]["resolvedPack"], "ARM::RteTest_DFP@0.2.0");

  EXPECT_TRUE(responses[3]["result"]["success"]); // get pack infos
  auto packInfos = responses[3]["result"]["packs"];
  EXPECT_EQ(packInfos.size(), 2);
  EXPECT_EQ(packInfos[1]["id"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_TRUE(packInfos[1]["used"]);

  packInfos = responses[4]["result"]["packs"];
  EXPECT_EQ(packInfos.size(), 8);
  EXPECT_EQ(packInfos[7]["id"], "SomeVendor::RteTest@0.0.1");
  EXPECT_FALSE(packInfos[7].contains("used"));
}

TEST_F(ProjMgrRpcTests, RpcGetPacksInfoLayer) {
  string context = "packs.CompatibleLayers+RteTest_ARMCM3";
  vector<string> contextList = {
    context
  };

  auto requests = CreateLoadRequests("/TestLayers/packs.csolution.yml", "", contextList);
  requests += FormatRequest(3, "GetUsedItems", json({{ "context", context }}));
  requests += FormatRequest(4, "GetPacksInfo", json({{ "context", context }, {"all", false}}));
  requests += FormatRequest(5, "GetPacksInfo", json({{ "context", context }, {"all", true}}));

  const auto& responses = RunRpcMethods(requests);

  EXPECT_TRUE(responses[2]["result"]["success"]);
  auto packs = responses[2]["result"]["packs"];
  EXPECT_EQ(packs.size(), 4);
  EXPECT_EQ(packs[0]["pack"], "ARM::RteTest_DFP@^0.2.0-0");
  EXPECT_EQ(packs[0]["resolvedPack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(RteUtils::ExtractFileName(packs[0]["origin"]), "packs.clayer.yml");
  EXPECT_EQ(RteUtils::ExtractFileName(packs[1]["origin"]), "packs.csolution.yml");
  EXPECT_EQ(RteUtils::ExtractFileName(packs[2]["origin"]), "packs.cproject.yml");
  EXPECT_EQ(RteUtils::ExtractFileName(packs[3]["origin"]), "packs.clayer.yml");

  EXPECT_TRUE(responses[3]["result"]["success"]); // get pack infos
  auto packInfos = responses[3]["result"]["packs"];
  EXPECT_EQ(packInfos.size(), 3);
  EXPECT_EQ(packInfos[2]["id"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_TRUE(packInfos[2].contains("used"));

  packInfos = responses[4]["result"]["packs"];
  EXPECT_EQ(packInfos.size(), 8);
  EXPECT_EQ(packInfos[7]["id"], "SomeVendor::RteTest@0.0.1");
  EXPECT_FALSE(packInfos[7].contains("used"));
}



TEST_F(ProjMgrRpcTests, RpcSelectPack) {
  string context = "test1.Release+CM0";
  vector<string> contextList = {
    context
  };

  auto requests = CreateLoadRequests("/TestSolution/test_pack_requirements.csolution.yml", "", contextList);
  requests += FormatRequest(3, "GetUsedItems", json({{ "context", context }}));
  requests += FormatRequest(4, "GetPacksInfo", json({{ "context", context }, {"all", true}}));

  // unselect pack
  RpcArgs::PackReference ref;
  ref.pack = "ARM::RteTestRequired";
  ref.selected = false;
  requests += FormatRequest(5, "SelectPack", json({{ "context", context }, {"pack", ref}}));

  // select in another origin
  ref.origin = "my.clayer.yml";
  ref.resolvedPack = "ARM::RteTestRequired@1.0.1-local"; // pretend we keep resolved value
  ref.selected = true;
  requests += FormatRequest(6, "SelectPack", json({{ "context", context }, {"pack", ref}}));

  // select another pack
  ref.pack = "SomeVendor::RteTest@^0.0.1";
  ref.resolvedPack = "SomeVendor::RteTest@0.0.1";
  ref.selected = true;
  requests += FormatRequest(7, "SelectPack", json({{ "context", context }, {"pack", ref}})); // select in another origin

  // select non-existing pack with path
  ref.pack = "Foo::Bar";
  ref.origin = "my.cproject.yml";
  ref.resolvedPack = "";
  ref.selected = true;
  ref.path = "path/To/Foo/Bar";
  requests += FormatRequest(8, "SelectPack", json({{ "context", context }, {"pack", ref}})); // select in another origin

  requests += FormatRequest(9, "GetUsedItems", json({{ "context", context }}));
  requests += FormatRequest(10, "Apply", json({{ "context", context }}));
  requests += FormatRequest(11, "GetUsedItems", json({{ "context", context }})); // should purge non-selected
  requests += FormatRequest(12, "GetPacksInfo", json({{ "context", context }, {"all", false}}));

  const auto& responses = RunRpcMethods(requests);

  EXPECT_TRUE(responses[2]["result"]["success"]);
  auto packs = responses[2]["result"]["packs"];
  EXPECT_EQ(packs.size(), 2);

  EXPECT_EQ(packs[0]["pack"], "ARM::RteTest_DFP@>=0.2.0");
  EXPECT_EQ(packs[0]["resolvedPack"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(packs[1]["pack"], "ARM::RteTestRequired");
  EXPECT_EQ(packs[1]["resolvedPack"], "ARM::RteTestRequired@1.0.1-local");

  string origin = packs[1]["origin"];
  EXPECT_TRUE(origin.find(".csolution.yml") != string::npos);

  EXPECT_TRUE(responses[3]["result"]["success"]); // get pack infos
  auto packInfos = responses[3]["result"]["packs"];

  packInfos = responses[3]["result"]["packs"];
  EXPECT_EQ(packInfos.size(), 8);
  EXPECT_EQ(packInfos[3]["id"], "ARM::RteTestRequired@1.0.1-local");
  EXPECT_FALSE(packInfos[3].contains("used"));
  EXPECT_EQ(packInfos[5]["id"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_TRUE(packInfos[5]["used"]);
  EXPECT_EQ(packInfos[7]["id"], "SomeVendor::RteTest@0.0.1");
  EXPECT_FALSE(packInfos[7].contains("used"));

// before apply
  packs = responses[8]["result"]["packs"];
  EXPECT_EQ(packs.size(), 5);
  EXPECT_EQ(packs[0]["pack"], "ARM::RteTest_DFP@>=0.2.0");
  EXPECT_EQ(packs[0]["resolvedPack"], "ARM::RteTest_DFP@0.2.0");

  EXPECT_EQ(packs[1]["pack"], "ARM::RteTestRequired");
  EXPECT_EQ(packs[1]["resolvedPack"], "ARM::RteTestRequired@1.0.1-local");
  EXPECT_FALSE(packs[1]["selected"]);

  EXPECT_EQ(packs[2]["pack"], "ARM::RteTestRequired");
  EXPECT_EQ(packs[2]["resolvedPack"], "ARM::RteTestRequired@1.0.1-local");
  EXPECT_EQ(packs[2]["origin"], "my.clayer.yml");
  EXPECT_TRUE(packs[2]["selected"]);

  EXPECT_EQ(packs[3]["pack"], "SomeVendor::RteTest@^0.0.1");
  EXPECT_EQ(packs[3]["resolvedPack"], "SomeVendor::RteTest@0.0.1");
  EXPECT_EQ(packs[3]["origin"], "my.clayer.yml");
  EXPECT_TRUE(packs[3]["selected"]);

  EXPECT_EQ(packs[4]["pack"], "Foo::Bar");
  string resolved = packs[4].contains("resolvedPack") ? packs[4]["resolvedPack"] : "";
  EXPECT_TRUE(resolved.empty());
  EXPECT_EQ(packs[4]["origin"], "my.cproject.yml");
  EXPECT_TRUE(packs[4]["selected"]);

// after apply
  packs = responses[10]["result"]["packs"];
  EXPECT_EQ(packs.size(), 4);
  EXPECT_EQ(packs[0]["pack"], "ARM::RteTest_DFP@>=0.2.0");
  EXPECT_EQ(packs[0]["resolvedPack"], "ARM::RteTest_DFP@0.2.0");

  EXPECT_EQ(packs[1]["pack"], "ARM::RteTestRequired");
  EXPECT_EQ(packs[1]["resolvedPack"], "ARM::RteTestRequired@1.0.1-local");
  EXPECT_EQ(packs[1]["origin"], "my.clayer.yml");
  EXPECT_TRUE(packs[1]["selected"]);

  EXPECT_EQ(packs[2]["pack"], "SomeVendor::RteTest@^0.0.1");
  EXPECT_EQ(packs[2]["resolvedPack"], "SomeVendor::RteTest@0.0.1");
  EXPECT_EQ(packs[2]["origin"], "my.clayer.yml");
  EXPECT_TRUE(packs[2]["selected"]);

  EXPECT_EQ(packs[3]["pack"], "Foo::Bar");
  resolved = packs[3].contains("resolvedPack") ? packs[3]["resolvedPack"] : "";
  EXPECT_TRUE(resolved.empty());
  EXPECT_EQ(packs[3]["origin"], "my.cproject.yml");
  EXPECT_TRUE(packs[3]["selected"]);

  // list packs
  // TODO: update test when unresolved packs will be listed
  packInfos = responses[11]["result"]["packs"];
  EXPECT_EQ(packInfos.size(), 3);
}

TEST_F(ProjMgrRpcTests, RpcGetDraftProjects) {
  // filter 'board'
  auto requests =
    FormatRequest(1, "LoadPacks") +
    FormatRequest(2, "GetDraftProjects", json{{ "filter", {{ "board", "RteTest Dummy board" }}}});
  auto responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[1]["result"]["success"]);
  auto examples = responses[1]["result"]["examples"];
  auto templates = responses[1]["result"]["templates"];
  EXPECT_EQ(2, examples.size());
  EXPECT_EQ(1, templates.size());
  EXPECT_EQ(examples[0]["name"], "PreInclude");
  EXPECT_EQ(examples[1]["name"], "PreIncludeEnvFolder");
  EXPECT_EQ(templates[0]["name"], "Board3");

  // filter 'device', no board
  requests =
    FormatRequest(1, "LoadPacks") +
    FormatRequest(2, "GetDraftProjects", json{{ "filter", {{ "device", "RteTest_ARMCM0_Dual" }}}});
  responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[1]["result"]["success"]);
  examples = responses[1]["result"]["examples"];
  templates = responses[1]["result"]["templates"];
  EXPECT_EQ(2, examples.size());
  EXPECT_EQ(0, templates.size());
  EXPECT_EQ(examples[0]["name"], "PreInclude");
  EXPECT_EQ(examples[1]["name"], "PreIncludeEnvFolder");

  // filter 'device', with board
  requests =
    FormatRequest(1, "LoadPacks") +
    FormatRequest(2, "GetDraftProjects", json{{ "filter", {{ "device", "RteTest_ARMCM0_Dual" },{"board", "RteTest Test board"}}}});
  responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[1]["result"]["success"]);
  examples = responses[1]["result"]["examples"];
  templates = responses[1]["result"]["templates"];
  EXPECT_EQ(0, examples.size());
  EXPECT_EQ(1, templates.size());
  EXPECT_EQ(templates[0]["name"], "Board1Template");

  // filter 'device' that's not mounted on any board
  requests =
    FormatRequest(1, "LoadPacks") +
    FormatRequest(2, "GetDraftProjects", json{{ "filter", {{ "device", "RteTestGen_ARMCM0" }}}});
  responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[1]["result"]["success"]);
  EXPECT_FALSE(responses[1]["result"].contains("examples"));

  // filter 'environment'
  requests =
    FormatRequest(1, "LoadPacks") +
    FormatRequest(2, "GetDraftProjects", json{{ "filter", {{ "environments", { "csolution" }}}}});
  responses = RunRpcMethods(requests);
  EXPECT_FALSE(responses[1]["result"].contains("examples"));
  templates = responses[1]["result"]["templates"];
  EXPECT_EQ(3, templates.size());
  EXPECT_EQ(templates[0]["name"], "Board1Template");
  EXPECT_EQ(templates[1]["name"], "Board2");
  EXPECT_EQ(templates[2]["name"], "Board3");

  // empty filter
  requests =
    FormatRequest(1, "LoadPacks") +
    FormatRequest(2, "GetDraftProjects", json{{ "filter", json::object() }});
  responses = RunRpcMethods(requests);
  examples = responses[1]["result"]["examples"];
  templates = responses[1]["result"]["templates"];
  EXPECT_EQ(2, examples.size());
  EXPECT_EQ(3, templates.size());

  // unknown board
  requests =
    FormatRequest(1, "LoadPacks") +
    FormatRequest(2, "GetDraftProjects", json{{ "filter", {{ "board", "UNKNOWN" }}}});
  responses = RunRpcMethods(requests);
  EXPECT_FALSE(responses[1]["result"]["success"]);
  EXPECT_EQ(responses[1]["result"]["message"], "Board or device processing failed");

  // without loading packs
  requests = FormatRequest(1, "GetDraftProjects", json{{ "filter", json::object() }});
  responses = RunRpcMethods(requests);
  EXPECT_FALSE(responses[0]["result"]["success"]);
  EXPECT_EQ(responses[0]["result"]["message"], "Packs must be loaded before retrieving draft projects");
}

TEST_F(ProjMgrRpcTests, RpcConvertSolution) {
  auto csolutionPath = testinput_folder + "/TestRpc/minimal.csolution.yml";
  auto requests = FormatRequest(1, "ConvertSolution",
    json({{ "solution", csolutionPath }, { "activeTarget", "TestHW" }, { "updateRte", true }}));
  auto responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]["success"]);
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestRpc/minimal.cbuild-idx.yml"));
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestRpc/minimal.cbuild-pack.yml"));
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestRpc/out/minimal+TestHW.cbuild-run.yml"));
  EXPECT_TRUE(RteFsUtils::Exists(testinput_folder + "/TestRpc/out/minimal/TestHW/minimal+TestHW.cbuild.yml"));

  // convert fail
  csolutionPath = testinput_folder + "/TestRpc/unknown-component.csolution.yml";
  requests = FormatRequest(1, "ConvertSolution",
    json({{ "solution", csolutionPath }, { "activeTarget", "" }, { "updateRte", true }}));
  responses = RunRpcMethods(requests);
  EXPECT_FALSE(responses[0]["result"]["success"]);

  // undefined compiler
  csolutionPath = testinput_folder + "/TestSolution/SelectableToolchains/select-compiler.csolution.yml";
  requests = FormatRequest(1, "ConvertSolution",
    json({{ "solution", csolutionPath }, { "activeTarget", "" }, { "updateRte", true }}));
  responses = RunRpcMethods(requests);
  EXPECT_FALSE(responses[0]["result"]["success"]);
  EXPECT_EQ(responses[0]["result"]["selectCompiler"][0], "AC6@>=6.0.0");
  EXPECT_EQ(responses[0]["result"]["selectCompiler"][1], "GCC@>=8.0.0");

  // undefined layer
  csolutionPath = testinput_folder + "/TestLayers/variables-notdefined.csolution.yml";
  requests = FormatRequest(1, "ConvertSolution",
    json({{ "solution", csolutionPath }, { "activeTarget", "" }, { "updateRte", true }}));
  responses = RunRpcMethods(requests);
  EXPECT_FALSE(responses[0]["result"]["success"]);
  EXPECT_EQ(responses[0]["result"]["undefinedLayers"][0], "NotDefined");
}

TEST_F(ProjMgrRpcTests, RpcDiscoverLayers) {
  auto csolutionPath = testinput_folder + "/TestLayers/config.csolution.yml";
  auto requests = FormatRequest(1, "DiscoverLayers",
    json({{ "solution", csolutionPath }, { "activeTarget", "" }}));
  auto responses = RunRpcMethods(requests);

  // valid configurations: compare response with golden reference
  EXPECT_TRUE(CompareRpcResponse(responses[0], testinput_folder + "/TestLayers/ref/rpc-discover-layers.json"))
    << "json response:\n" << ProjMgrTestEnv::StripAbsoluteFunc(responses[0].dump(2));

  // unknown active target set
  csolutionPath = testinput_folder + "/TestLayers/config.csolution.yml";
  requests = FormatRequest(1, "DiscoverLayers",
    json({{ "solution", csolutionPath }, { "activeTarget", "unknown" }}));
  responses = RunRpcMethods(requests);
  EXPECT_FALSE(responses[0]["result"]["success"]);
  EXPECT_EQ(responses[0]["result"]["message"], "Setup of solution contexts failed");

  // no compatible layer combination
  csolutionPath = testinput_folder + "/TestLayers/variables-notdefined.csolution.yml";
  requests = FormatRequest(1, "DiscoverLayers",
    json({{ "solution", csolutionPath }, { "activeTarget", "" }}));
  responses = RunRpcMethods(requests);
  EXPECT_FALSE(responses[0]["result"]["success"]);
  EXPECT_EQ(responses[0]["result"]["message"], "No compatible software layer found. Review required connections of the project");
}

TEST_F(ProjMgrRpcTests, RpcListMissingPacks) {
  auto csolutionPath = testinput_folder + "/TestSolution/pack_missing.csolution.yml";
  auto requests =
    FormatRequest(1, "ListMissingPacks", json({{ "solution", csolutionPath }, { "activeTarget", "CM0" }})) +
    FormatRequest(2, "ListMissingPacks", json({{ "solution", csolutionPath }, { "activeTarget", "Gen" }})) +
    FormatRequest(3, "ListMissingPacks", json({{ "solution", csolutionPath }, { "activeTarget", "Unknown" }}));
  auto responses = RunRpcMethods(requests);

  // valid responses
  EXPECT_TRUE(responses[0]["result"]["success"]);
  EXPECT_EQ(responses[0]["result"]["packs"][0], "ARM::Missing_DFP@0.0.9");
  EXPECT_TRUE(responses[1]["result"]["success"]);
  EXPECT_EQ(responses[1]["result"]["packs"][0], "ARM::Missing_PACK@0.0.1");

  // unknown active target set
  EXPECT_FALSE(responses[2]["result"]["success"]);
  EXPECT_EQ(responses[2]["result"]["message"], "Setup of solution contexts failed");
}

TEST_F(ProjMgrRpcTests, RpcGetVariables) {
  vector<string> contextList = {
    "variables.BuildType1+TargetType1",
  };

  auto requests = CreateLoadRequests("/TestLayers/variables.csolution.yml", "TargetType1", contextList);
  int id = 3;
  for(const auto& context : contextList) {
    requests += FormatRequest(id++, "GetVariables", json({{ "context", context }}));
  }

  const auto& responses = RunRpcMethods(requests);

  EXPECT_TRUE(responses[2]["result"]["success"]);
  map<string, string> vars = responses[2]["result"]["variables"];

  EXPECT_EQ(vars["BuildType"], "BuildType1");
  EXPECT_EQ(vars["Compiler"], "AC6");
  EXPECT_EQ(vars["Dname"], "RteTest_ARMCM0");
  EXPECT_EQ(vars["Dpack"], testcmsispack_folder + "/ARM/RteTest_DFP/0.2.0/");
  EXPECT_EQ(vars["Pname"], "");
  EXPECT_EQ(vars["Project"], "variables");
  EXPECT_EQ(vars["Solution"], "variables");
  EXPECT_EQ(vars["TargetType"], "TargetType1");
  EXPECT_EQ(vars["VarBuildLayer"], "./variables/build1.clayer.yml");
  EXPECT_EQ(vars["VarSolution"], "./variables/app.clayer.yml");
  EXPECT_EQ(vars["VarSolutionDir"], testinput_folder + "/TestLayers/variables/solutionDir.clayer.yml");
  EXPECT_EQ(vars["VarTargetLayer"], "./variables/target1.clayer.yml");
}


// end of ProjMgrRpcTests.cpp
