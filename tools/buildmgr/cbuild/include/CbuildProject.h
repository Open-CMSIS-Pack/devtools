/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CBUILDPROJECT_H
#define CBUILDPROJECT_H

#include "CbuildUtils.h"
#include "RteGenerator.h"
#include "RteCprjProject.h"

class CbuildProject {
public:
  CbuildProject(RteCprjProject* project);
  virtual ~CbuildProject();

  /**
   * @brief create RTE target for project
   * @param targetName name of the target to be used
   * @param cprj pointer to CprjFile object
   * @param rtePath path to local pack repository
   * @param toolchain name of the build toolchain
   * @return
  */
  bool CreateTarget(const std::string& targetName, const CprjFile *cprj,
                    const std::string& rtePath, const std::string&  toolchain);

  /**
   * @brief check pack requirements for cprj project
   * @param cprj pointer to CprjFile object
   * @param rtePath path to local pack repository
   * @param urlList output list of pack url(s) to be needed by project
   * @return true if the pack requirements are resolved, otherwise false
  */
  static bool CheckPackRequirements(const CprjFile *cprj, const std::string& rtePath, std::vector<CbuildPackItem> &packList);

protected:
  RteDevice* GetDeviceLeaf(const std::string& fullDeviceName, const std::string& deviceVendor, const std::string& targetName);
  bool AddAdditionalAttributes(std::map<std::string, std::string> &attributes, const std::string& targetName);
  bool UpdateTarget(const RteItem* components, const std::map<std::string, std::string> &attributes, const std::string& targetName);

  static void SetToolchain(const std::string& toolchain, std::map<std::string, std::string> &attributes);
  static RtePackage* ReadGpdscFile(const std::string& gpdsc);

  RteCprjProject* m_project;

};

#endif /* CBUILDPROJECT_H */
