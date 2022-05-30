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
   * @param cprjPack pointer to RTE components
   * @param rtePath path to local pack repository
   * @param optionAttributes list of target attributes
   * @param toolchain name of the build toolchain
   * @return
  */
  const bool CreateTarget(const std::string& targetName, const RtePackage *cprjPack,
                    const std::string& rtePath, const std::map<std::string, std::string> &optionAttributes,
                    const std::string&  toolchain);

  /**
   * @brief check pack requirements for cprj project
   * @param cprjPack pointer to RTE components
   * @param rtePath path to local pack repository
   * @param urlList output list of pack url(s) to be needed by project
   * @return true if the pack requirements are resolved, otherwise false
  */
  static const bool CheckPackRequirements(const RtePackage *cprjPack, const std::string& rtePath, std::vector<CbuildPackItem> &packList);

protected:
  RteDevice* GetDeviceLeaf(const std::string& fullDeviceName, const std::string& deviceVendor, const std::string& targetName);
  const bool AddAdditionalAttributes(std::map<std::string, std::string> &attributes, const std::string& targetName);
  const bool UpdateTarget(const RteItem* components, const std::map<std::string, std::string> &attributes, const std::string& targetName);

  static void SetToolchain(const std::string& toolchain, std::map<std::string, std::string> &attributes);
  static const std::string GetLocalRepoVersion(const std::string& rtePath, const std::string& name, const std::string& vendor, const std::string& versionRange, std::string& url);
  static bool GetUrlFromLocalRepository(const std::string& rtePath, const std::string& name, const std::string& vendor, std::string& versionRange, std::string& url);
  static bool GetUrl(const std::string& path, const std::string& name, const std::string& vendor, std::string& version, std::string& url);
  static RteGeneratorModel* ReadGpdscFile(const std::string& gpdsc);

  RteCprjProject* m_project;

};

#endif /* CBUILDPROJECT_H */
