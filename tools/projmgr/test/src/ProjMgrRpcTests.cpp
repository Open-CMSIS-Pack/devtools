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
  EXPECT_EQ(string(VERSION_STRING), responses[0]["result"]);
}

TEST_F(ProjMgrRpcTests, RpcGetVersionWithContent) {
  const auto& requests = "Content-Length:46\n\n" + FormatRequest(1, "GetVersion");
  const auto& responses = RunRpcMethodsWithContent(requests);
  EXPECT_TRUE(responses.find(CrossPlatformUtils::Crlf() + CrossPlatformUtils::Crlf() + "{") != string::npos);
}

TEST_F(ProjMgrRpcTests, RpcLoadSolution) {
  const string& csolution = testinput_folder + "/TestRpc/minimal.csolution.yml";
  const auto& requests =
    FormatRequest(1, "LoadPacks") +
    FormatRequest(2, "LoadSolution", json({{ "solution", csolution }}));

  const auto& responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]);
  EXPECT_TRUE(responses[1]["result"]);
}

TEST_F(ProjMgrRpcTests, RpcLoadSolution_UnknownComponent) {
  const string& csolution = testinput_folder + "/TestRpc/unknown-component.csolution.yml";
  const auto& requests =
    FormatRequest(1, "LoadPacks") +
    FormatRequest(2, "LoadSolution", json({ { "solution", csolution } })) +
    FormatRequest(3, "GetLogMessages");

  const auto& responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]);
  EXPECT_TRUE(responses[1]["result"]);
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
  YAML::Node cbuildset;
  cbuildset["cbuild-set"]["generated-by"] = "ProjMrgUnitTests";
  for (const auto& context : contextList) {
    cbuildset["cbuild-set"]["contexts"].push_back(map<string, string>{ { "context", context } });
  }
  const string& cbuildsetPath = testinput_folder + "/Validation/dependencies.cbuild-set.yml";

  ofstream cbuildsetFile;
  cbuildsetFile.open(cbuildsetPath, fstream::trunc);
  cbuildsetFile << cbuildset << std::endl;
  cbuildsetFile.close();

  const string& csolution = testinput_folder + "/Validation/dependencies.csolution.yml";
  auto requests =
    FormatRequest(1, "LoadPacks") +
    FormatRequest(2, "LoadSolution", json({ { "solution", csolution } }));
  int id = 3;
  for (const auto& context : contextList) {
    requests += FormatRequest(id++, "ValidateComponents", json({ { "context", context } }));
  }

  const auto& responses = RunRpcMethods(requests);
  EXPECT_TRUE(responses[0]["result"]);
  EXPECT_TRUE(responses[1]["result"]);

  // selectable
  auto validation = responses[2]["result"]["validation"][0];
  EXPECT_EQ("ARM::Device:Startup&RteTest Startup@2.0.3", validation["id"]);
  EXPECT_EQ("SELECTABLE", validation["result"]);
  EXPECT_EQ("require RteTest:CORE", validation["conditions"][0]["expression"]);
  EXPECT_EQ("ARM::RteTest:CORE", validation["conditions"][0]["aggregates"][0]);

  // missing
  validation = responses[3]["result"]["validation"][0];
  EXPECT_EQ("ARM::RteTest:Check:Missing@0.9.9", validation["id"]);
  EXPECT_EQ("MISSING", validation["result"]);
  EXPECT_EQ("require RteTest:Dependency:Missing", validation["conditions"][0]["expression"]);

  // conflict
  validation = responses[4]["result"]["validation"][0];
  EXPECT_EQ("RteTest:ApiExclusive@1.0.0", validation["id"]);
  EXPECT_EQ("CONFLICT", validation["result"]);
  EXPECT_EQ("ARM::RteTest:ApiExclusive:S1", validation["aggregates"][0]);
  EXPECT_EQ("ARM::RteTest:ApiExclusive:S2", validation["aggregates"][1]);

  // incompatible
  validation = responses[5]["result"]["validation"][0];
  EXPECT_EQ("ARM::RteTest:Check:Incompatible@0.9.9", validation["id"]);
  EXPECT_EQ("INCOMPATIBLE", validation["result"]);
  EXPECT_EQ("deny RteTest:Dependency:Incompatible_component", validation["conditions"][0]["expression"]);
  EXPECT_EQ("ARM::RteTest:Dependency:Incompatible_component", validation["conditions"][0]["aggregates"][0]);

  // incompatible variant
  validation = responses[6]["result"]["validation"][0];
  EXPECT_EQ("ARM::RteTest:Check:IncompatibleVariant@0.9.9", validation["id"]);
  EXPECT_EQ("INCOMPATIBLE_VARIANT", validation["result"]);
  EXPECT_EQ("require RteTest:Dependency:Variant&Compatible", validation["conditions"][0]["expression"]);
  EXPECT_EQ("ARM::RteTest:Dependency:Variant", validation["conditions"][0]["aggregates"][0]);
}

// end of ProjMgrRpcTests.cpp
