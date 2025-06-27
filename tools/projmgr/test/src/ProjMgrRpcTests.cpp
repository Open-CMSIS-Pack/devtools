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

// end of ProjMgrRpcTests.cpp
