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


using namespace std;

class ProjMgrRpcTests : public ProjMgr, public ::testing::Test {
protected:
  ProjMgrRpcTests() {}
  virtual ~ProjMgrRpcTests() {}
};

TEST_F(ProjMgrRpcTests, Load_Solution) {



}

// end of ProjMgrRpcTests.cpp
