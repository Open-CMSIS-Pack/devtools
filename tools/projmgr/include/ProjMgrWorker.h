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
 * @brief project context item containing
 *        pointer to csolution,
 *        pointer to cproject,
 *        map of pointers to clayers,
 *        pointer to rte project,
 *        pointer to rte target,
 *        map of project dependencies
 *        build-type properties,
 *        target-type properties,
 *        parent csolution target properties,
 *        project name,
 *        project directory,
 *        rte directory,
 *        build-type/target-type pair,
 *        project description,
 *        output type,
 *        device selection,
 *        trustzone selection,
 *        fpu selection,
 *        endianess selection,
 *        list of package requirements,
 *        list of component requirements,
 *        compiler string in short syntax,
 *        toolchain with parsed name and version,
 *        list of defines
 *        list of include paths
 *        list of toolchain flags
 *        map of target attributes,
 *        map of used packages,
 *        map of used components,
 *        map of unresolved dependencies,
 *        map of config files,
 *        list of user groups,
 *        map of absolute file paths,
 *        linker script,
*/
struct ContextItem {
  CsolutionItem* csolution = nullptr;
  CprojectItem* cproject = nullptr;
  std::map<std::string, ClayerItem*> clayers;
  RteProject* rteActiveProject = nullptr;
  RteTarget* rteActiveTarget = nullptr;
  std::map<std::string, std::vector<std::string>> prjDeps;
  BuildType buildType;
  TargetType targetType;
  TargetType csolutionTarget;
  std::string name;
  std::string cprojectDir;
  std::string rteDir;
  TypePair type;
  std::string description;
  std::string outputType;
  std::string device;
  std::string trustzone;
  std::string fpu;
  std::string endian;
  std::vector<PackageItem> packRequirements;
  std::vector<ComponentItem> componentRequirements;
  std::string compiler;
  ToolchainItem toolchain;
  std::vector<std::string> defines;
  std::vector<std::string> includes;
  std::vector<MiscItem> misc;
  std::map<std::string, std::string> targetAttributes;
  std::map<std::string, RtePackage*> packages;
  std::map<std::string, std::pair<RteComponent*, ComponentItem*>> components;
  std::map<std::string, RteComponentContainer*> dependencies;
  std::map<std::string, std::map<std::string, RteFileInstance*>> configFiles;
  std::vector<GroupNode> groups;
  std::map<std::string, std::string> filePaths;
  std::string linkerScript;
};

/**
 * @brief string collection containing
 *        pointer to destination element
 *        vector with pointers to source elements
*/
struct StringCollection {
  std::string* assign;
  std::vector<std::string*> elements;
};

/**
 * @brief string vector pair containing
 *        items to add
 *        items to remove
*/
struct StringVectorPair {
  std::vector<std::string>* add;
  std::vector<std::string>* remove;
};

/**
 * @brief string vector collection containing
 *        pointer to destination
 *        vector with items to add/remove
*/
struct StringVectorCollection {
  std::vector<std::string>* assign;
  std::vector <StringVectorPair> pair;
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
   * @brief process context
   * @param reference to context
   * @param resolveDependencies boolean automatically resolve dependencies, default false
   * @return true if executed successfully
  */
  bool ProcessContext(ContextItem& context, bool resolveDependencies = false);

  /**
   * @brief process project dependencies
   * @param reference to context
   * @param reference to output directory
   * @return true if executed successfully
  */
  bool ProcessProjDeps(ContextItem& context, const std::string& outputDir);

  /**
   * @brief list available packs
   * @param filter words to filter results
   * @param packs reference to list of packs
   * @return true if executed successfully
  */
  bool ListPacks(const std::string& filter, std::set<std::string>& packs);

  /**
   * @brief list available devices
   * @param filter words to filter results
   * @param packs reference to list of packs
   * @return true if executed successfully
  */
  bool ListDevices(const std::string& filter, std::set<std::string>& devices);

  /**
   * @brief list available components
   * @param filter words to filter results
   * @param packs reference to list of packs
   * @return true if executed successfully
  */
  bool ListComponents(const std::string& filter, std::set<std::string>& components);

  /**
   * @brief list available dependencies
   * @param filter words to filter results
   * @param packs reference to list of packs
   * @return true if executed successfully
  */
  bool ListDependencies(const std::string& filter, std::set<std::string>& dependencies);

  /**
   * @brief list contexts
   * @param filter words to filter results
   * @param reference list of contexts
   * @return true if executed successfully
  */
  bool ListContexts(const std::string& filter, std::set<std::string>& contexts);

  /**
   * @brief add contexts for a given descriptor
   * @param reference to parser
   * @param reference to descriptor
   * @param cproject filename
   * @return true if executed successfully
  */
  bool AddContexts(ProjMgrParser& parser, ContextDesc& descriptor, const std::string& cprojectFile);

  /**
   * @brief get context map
   * @return context map
  */
  std::map<std::string, ContextItem>& GetContexts(void);

  /**
   * @brief copy context files into output directory
   * @param reference to context
   * @param reference to output directory
   * @param boolean output empty
   * @return true if executed successfully
  */
  bool CopyContextFiles(ContextItem& context, const std::string& outputDir, bool outputEmpty);

protected:
  ProjMgrKernel* m_kernel = nullptr;
  RteGlobalModel* m_model = nullptr;
  std::list<RtePackage*> m_installedPacks;
  std::map<std::string, ContextItem> m_contexts;

  bool LoadPacks(void);
  bool CheckRteErrors(void);
  bool CheckType(TypeFilter typeFilter, TypePair type);
  bool GetTypeContent(ContextItem& context);
  bool SetTargetAttributes(ContextItem& context, std::map<std::string, std::string>& attributes);
  bool ProcessPrecedences(ContextItem& context);
  bool ProcessPrecedence(StringCollection& item);
  bool ProcessDevice(ContextItem& context);
  bool ProcessToolchain(ContextItem& context);
  bool ProcessPackages(ContextItem& context);
  bool ProcessComponents(ContextItem& context);
  bool ProcessDependencies(ContextItem& context);
  bool ProcessConfigFiles(ContextItem& context);
  bool ProcessGroups(ContextItem& context);
  bool AddContext(ProjMgrParser& parser, ContextDesc& descriptor, const TypePair& type, const std::string& cprojectFile, ContextItem& parentContext);
  void AddMiscUniquely(std::vector<MiscItem>& dst, std::vector<std::vector<MiscItem>*>& srcVec);
  void AddStringItemsUniquely(std::vector<std::string>& dst, const std::vector<std::string>& src);
  void RemoveStringItems(std::vector<std::string>& dst, std::vector<std::string>& src);
  void PushBackUniquely(std::vector<std::string>& vec, const std::string& value);
  void MergeStringVector(StringVectorCollection& item);
  void MergeMiscCPP(std::vector<MiscItem>& vec);
  bool AddGroup(const GroupNode& src, std::vector<GroupNode>& dst, ContextItem& context, const std::string root);
  bool AddFile(const FileNode& src, std::vector<FileNode>& dst, ContextItem& context, const std::string root);
  bool AddComponent(const ComponentItem& src, std::vector<ComponentItem>& dst, TypePair type);
  static std::set<std::string> SplitArgs(const std::string& args, const std::string& delimiter = " ");
  static void ApplyFilter(const std::set<std::string>& origin, const std::set<std::string>& filter, std::set<std::string>& result);
  static bool FullMatch(const std::set<std::string>& installed, const std::set<std::string>& required);
  std::string GetComponentID(RteItem* component) const;
  std::string GetComponentAggregateID(RteItem* component) const;
  std::string GetPackageID(RteItem* pack) const;
  std::string ConstructID(const std::vector<std::pair<const char*, const std::string&>>& elements) const;
};

#endif  // PROJMGRWORKER_H
