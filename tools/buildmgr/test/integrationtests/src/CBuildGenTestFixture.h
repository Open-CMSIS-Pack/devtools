/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CBUILDGENTESTFIXTURE_H
#define CBUILDGENTESTFIXTURE_H

#include "CBuildIntegTestEnv.h"

class CBuildGenTestFixture : public ::testing::Test {
protected:
  void RunCBuildGen              (const TestParam& param, std::string projpath = examples_folder, bool env = true);
  void RunLayerCommand           (int cmdNum, const TestParam& param);
  void RunCBuildGenAux           (const std::string& cmd, bool expect);
  void CheckCMakeLists           (const TestParam& param);
  void CheckDescriptionFiles     (const std::string& filename1, const std::string& filename2);
  void CheckLayerFiles           (const TestParam& param, const std::string& rteFolder);
  void CheckProjectFiles         (const TestParam& param, const std::string& rteFolder);
  void CheckOutputDir            (const TestParam& param, const std::string& outdir);
  void CheckCMakeIntermediateDir (const TestParam& param, const std::string& intdir);
  void CheckCPInstallFile        (const TestParam& param, bool json=false);

  void GetDirectoryItems         (const std::string& inPath, std::set<std::string> &result, const std::string& ignoreDir);

  std::string stdoutStr;
};

#endif  // CBUILDGENTESTFIXTURE_H
