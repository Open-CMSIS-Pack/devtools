/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRWORKER_H
#define PROJMGRWORKER_H

#include "ProjMgrKernel.h"
#include "ProjMgrParser.h"

/**
 * @brief toolchain item containing
 *        toolchain name,
 *        toolchain version
*/
struct ToolchainItem {
  std::string name;
  std::string version;
};

/**
 * @brief package item containing
 *        package name,
 *        package vendor,
 *        package version
*/
struct PackageItem {
  std::string name;
  std::string vendor;
  std::string version;
};

/**
 * @brief project manager build-subset item containing
 *        cproject tree,
 *        rte project,
 *        rte target,
 *        project name,
 *        project description,
 *        output type,
 *        list of package requirements,
 *        toolchain item,
 *        map of target attributes,
 *        map of used packages,
 *        map of used components,
 *        map of unresolved dependencies,
 *        list of user groups
*/
struct ProjMgrProjectItem {
  CprojectItem cproject;
  RteProject* rteActiveProject = nullptr;
  RteTarget* rteActiveTarget = nullptr;
  std::string name;
  std::string description;
  std::string outputType;
  std::vector<PackageItem> packRequirements;
  ToolchainItem toolchain;
  std::map<std::string, std::string> targetAttributes;
  std::map<std::string, RtePackage*> packages;
  std::map<std::string, RteComponent*> components;
  std::map<std::string, RteComponentContainer*> dependencies;
  std::vector<GroupNode> groups;
};

/**
 * @brief projmgr worker class responsible for processing requests and orchestrating parser and generator calls
*/
class ProjMgrWorker {
public:
  /**
   * @brief class constructor
  */
  ProjMgrWorker(void);

  /**
   * @brief class destructor
  */
  ~ProjMgrWorker(void);

  /**
   * @brief process project
   * @param project reference to project item
   * @param resolveDependencies boolean automatically resolve dependencies, default false
   * @return true if executed successfully
  */
  bool ProcessProject(ProjMgrProjectItem& project, bool resolveDependencies = false);

  /**
   * @brief list available packs
   * @param filter words to filter results
   * @param packs reference to list of packs
   * @return true if executed successfully
  */
  bool ListPacks(const std::string& filter, std::set<std::string>& packs);

  /**
   * @brief list available devices
   * @param project reference to project item
   * @param filter words to filter results
   * @param packs reference to list of packs
   * @return true if executed successfully
  */
  bool ListDevices(ProjMgrProjectItem& project, const std::string& filter, std::set<std::string>& devices);

  /**
   * @brief list available components
   * @param project reference to project item
   * @param filter words to filter results
   * @param packs reference to list of packs
   * @return true if executed successfully
  */
  bool ListComponents(ProjMgrProjectItem& project, const std::string& filter, std::set<std::string>& components);

  /**
   * @brief list available dependencies
   * @param project reference to project item
   * @param filter words to filter results
   * @param packs reference to list of packs
   * @return true if executed successfully
  */
  bool ListDependencies(ProjMgrProjectItem& project, const std::string& filter, std::set<std::string>& dependencies);

protected:
  ProjMgrKernel* m_kernel = nullptr;
  RteGlobalModel* m_model = nullptr;
  std::list<RtePackage*> m_installedPacks;

  bool LoadPacks(void);
  bool CheckRteErrors(void);
  bool SetTargetAttributes(ProjMgrProjectItem& project, std::map<std::string, std::string>& attributes);
  bool ProcessDevice(ProjMgrProjectItem& project);
  bool ProcessToolchain(ProjMgrProjectItem& project);
  bool ProcessPackages(ProjMgrProjectItem& project);
  bool ProcessComponents(ProjMgrProjectItem& project);
  bool ProcessDependencies(ProjMgrProjectItem& project);

  static std::set<std::string> SplitArgs(const std::string& args);
  static void ApplyFilter(const std::set<std::string>& origin, const std::set<std::string>& filter, std::set<std::string>& result);
};

#endif  // PROJMGRWORKER_H
