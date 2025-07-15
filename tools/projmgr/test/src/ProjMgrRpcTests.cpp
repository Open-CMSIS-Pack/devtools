/*
 * Copyright (c) 2020-2025 Arm Limited. All rights reserved.
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

  string CreateLoadRequests(const string& solution, const vector<string>& contextList = RteUtils::EMPTY_STRING_VECTOR);

};

string ProjMgrRpcTests::FormatRequest(const int id, const string& method, const json& params = json()) {
  json request;
  request["jsonrpc"] = "2.0";
  request["id"] = id;
  request["method"] = method;
  if (!params.is_null()) {
    request["params"] = params;
  }
  return request.dump();
}

string ProjMgrRpcTests::CreateLoadRequests(const string& solution, const vector<string>& contextList)
{
  auto csolutionPath = testinput_folder + solution;
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
  return FormatRequest(1, "LoadPacks") + FormatRequest(2, "LoadSolution", json({ { "solution", csolutionPath } }));
}

vector<json> ProjMgrRpcTests::RunRpcMethods(const string& strIn) {
  StdStreamRedirect streamRedirect;
  streamRedirect.SetInString(strIn);
  char* argv[] = { (char*)"csolution", (char*)"rpc" };
  EXPECT_EQ(0, RunProjMgr(2, argv, 0));
  string line;
  vector<json> responses;
  istringstream iss(streamRedirect.GetOutString());
  while (getline(iss, line)) {
    responses.push_back(json::parse(line));
  }
  return responses;
}

string ProjMgrRpcTests::RunRpcMethodsWithContent(const string& strIn) {
  StdStreamRedirect streamRedirect;
  streamRedirect.SetInString(strIn);
  char* argv[] = { (char*)"csolution", (char*)"rpc", (char*)"--content-length" };
  EXPECT_EQ(0, RunProjMgr(3, argv, 0));
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
}

TEST_F(ProjMgrRpcTests, RpcGetVersionWithContent) {
  const auto& requests = "Content-Length:46\n\n" + FormatRequest(1, "GetVersion");
  const auto& responses = RunRpcMethodsWithContent(requests);
  EXPECT_TRUE(responses.find(CrossPlatformUtils::Crlf() + CrossPlatformUtils::Crlf() + "{") != string::npos);
}

TEST_F(ProjMgrRpcTests, RpcLoadSolution) {
  const auto& requests = CreateLoadRequests("/TestRpc/minimal.csolution.yml");
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
  const auto& requests = FormatRequest(1, "LoadSolution", json({ { "solution", csolutionPath } }));
  const auto& responses = RunRpcMethods(requests);
  EXPECT_FALSE(responses[0]["result"]["success"]);
  string msg = responses[0]["result"]["message"];
  EXPECT_EQ(msg, "Packs must be loaded before loading solution");
}

TEST_F(ProjMgrRpcTests, RpcLoadSolution_UnknownComponent) {
  const string& csolution = testinput_folder + "/TestRpc/unknown-component.csolution.yml";
  auto requests = CreateLoadRequests("/TestRpc/unknown-component.csolution.yml") +
    FormatRequest(3, "GetLogMessages");

  const auto& responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]["success"]);
  EXPECT_TRUE(responses[1]["result"]["success"]);
  EXPECT_EQ("no component was found with identifier 'ARM::UNKNOWN:COMPONENT'",
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
  auto requests = CreateLoadRequests("/Validation/dependencies.csolution.yml", contextList);
  int id = 3;
  for (const auto& context : contextList) {
    requests += FormatRequest(id++, "ValidateComponents", json({ { "context", context } }));
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
  auto requests = CreateLoadRequests("/Validation/dependencies.csolution.yml", contextList);
  int id = 3;
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

  auto requests = CreateLoadRequests("/Validation/dependencies.csolution.yml", contextList);
  int id = 3;
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
  string res = responses[3]["result"]["classes"][2]["result"];
  EXPECT_EQ(res, "SELECTABLE");

  EXPECT_TRUE(responses[4]["result"]["success"]); // components resolved
  EXPECT_EQ(responses[5]["result"]["result"], "FULFILLED");
  EXPECT_FALSE(responses[5]["result"].contains("validation"));
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

  auto requests = CreateLoadRequests("/Validation/dependencies.csolution.yml", contextList);
  int id = 3;
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


TEST_F(ProjMgrRpcTests, RpcGetUsedItems) {
  string context = "selectable+CM0";
  vector<string> contextList = {
    context
  };
  RpcArgs::Options opt{ "corelayer.yml", "@>=0.1.0", true};
  json param;
  param["context"] = context;
  param["id"] = "ARM::RteTest:CORE";
  param["count"] = 1;
  RpcArgs::to_json(param["options"], opt);

  auto requests = CreateLoadRequests("/Validation/dependencies.csolution.yml", contextList);
  int id = 3;
  requests += FormatRequest(3, "GetUsedItems", json({{ "context", context }}));
  requests += FormatRequest(4, "SelectComponent", param);
  requests += FormatRequest(5, "Apply", param);
  requests += FormatRequest(6, "GetUsedItems", json({{ "context", context }}));

  const auto& responses = RunRpcMethods(requests);

  EXPECT_TRUE(responses[2]["result"]["success"]);
  auto components = responses[2]["result"]["components"];
  auto packs = responses[2]["result"]["packs"];
  EXPECT_EQ(packs[0]["id"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(components[0]["id"], "Device:Startup&RteTest Startup");
  EXPECT_EQ(components[0]["resolvedComponent"]["id"], "ARM::Device:Startup&RteTest Startup@2.0.3");

  EXPECT_TRUE(responses[4]["result"]["success"]); // apply successful

  EXPECT_TRUE(responses[5]["result"]["success"]); // select successful
  components = responses[5]["result"]["components"];
  packs = responses[5]["result"]["packs"];
  EXPECT_EQ(packs[0]["id"], "ARM::RteTest_DFP@0.2.0");
  EXPECT_EQ(components[0]["id"], "Device:Startup&RteTest Startup");
  EXPECT_EQ(components[0]["resolvedComponent"]["id"], "ARM::Device:Startup&RteTest Startup@2.0.3");

  EXPECT_EQ(components[1]["id"], "ARM::RteTest:CORE@>=0.1.0");
  EXPECT_EQ(components[1]["resolvedComponent"]["id"], "ARM::RteTest:CORE@0.1.1");
  EXPECT_EQ(components[1]["options"]["layer"], "corelayer.yml");
  EXPECT_EQ(components[1]["options"]["explicitVersion"], "@>=0.1.0");
  EXPECT_TRUE(components[1]["options"]["explicitVendor"]);
}


// end of ProjMgrRpcTests.cpp
