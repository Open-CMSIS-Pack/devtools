/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
  void CheckCPInstallFile        (const TestParam& param);

  void GetDirectoryItems         (const std::string& inPath, std::set<std::string> &Result, const std::string& ignoreDir);
};
