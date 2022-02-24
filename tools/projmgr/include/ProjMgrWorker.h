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
 * @brief directories item containing
 *        cproject directory,
 *        intdir directory,
 *        outdir directory,
*/
struct DirectoriesItem {
  std::string cproject;
  std::string intdir;
  std::string outdir;
  std::string cprj;
};

/**
 * @brief device item containing
 *        device name,
 *        device vendor,
 *        device processor name,
*/
struct DeviceItem {
  std::string vendor;
  std::string name;
  std::string pname;
};

/**
 * @brief board item containing
 *        board vendor,
 *        board name,
*/
struct BoardItem {
  std::string vendor;
  std::string name;
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
 *        directories,
 *        build-type/target-type pair,
 *        project name,
 *        project description,
 *        output type,
 *        device selection,
 *        board selection,
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
 *        map of generators,
 *        linker script,
*/
struct ContextItem {
  CsolutionItem* csolution = nullptr;
  CprojectItem* cproject = nullptr;
  std::map<std::string, ClayerItem*> clayers;
  RteProject* rteActiveProject = nullptr;
  RteTarget* rteActiveTarget = nullptr;
  BuildType buildType;
  TargetType targetType;
  TargetType csolutionTarget;
  DirectoriesItem directories;
  TypePair type;
  std::string name;
  std::string description;
  std::string outputType;
  std::string device;
  std::string board;
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
  std::map<std::string, std::set<std::string>> dependencies;
  std::map<std::string, std::map<std::string, RteFileInstance*>> configFiles;
  std::vector<GroupNode> groups;
  std::map<std::string, std::string> filePaths;
  std::map<std::string, RteGenerator*> generators;
  std::map<std::string, std::pair<std::string, std::string>> gpdscs;
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
   * @brief list available packs
   * @param reference to list of packs
   * @param filter words to filter results
   * @return true if executed successfully
  */
  bool ListPacks(std::vector<std::string>& packs, const std::string& contextName, const std::string& filter = RteUtils::EMPTY_STRING);

  /**
   * @brief list available devices
   * @param reference to list of devices
   * @param reference to context name
   * @param filter words to filter results
   * @return true if executed successfully
  */
  bool ListDevices(std::vector<std::string>& devices, const std::string& contextName, const std::string& filter = RteUtils::EMPTY_STRING);

  /**
   * @brief list available components
   * @param reference to list of components
   * @param reference to context name
   * @param filter words to filter results
   * @return true if executed successfully
  */
  bool ListComponents(std::vector<std::string>& components, const std::string& contextName, const std::string& filter = RteUtils::EMPTY_STRING);

  /**
   * @brief list available dependencies
   * @param reference to list of dependencies
   * @param reference to context name
   * @param filter words to filter results
   * @return true if executed successfully
  */
  bool ListDependencies(std::vector<std::string>& dependencies, const std::string& contextName, const std::string& filter = RteUtils::EMPTY_STRING);

  /**
   * @brief list contexts
   * @param reference to list of contexts
   * @param filter words to filter results
   * @return true if executed successfully
  */
  bool ListContexts(std::vector<std::string>& contexts, const std::string& filter = RteUtils::EMPTY_STRING);

  /**
 * @brief list generators of a given context
 * @param context name
 * @param reference to list of generators
 * @return true if executed successfully
*/
  bool ListGenerators(const std::string& context, std::vector<std::string>& generators);

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
   * @param pointer to context map
  */
  void GetContexts(std::map<std::string, ContextItem>* &contexts);

  /**
   * @brief copy context files into output directory
   * @param reference to context
   * @param reference to output directory
   * @param boolean output empty
   * @return true if executed successfully
  */
  bool CopyContextFiles(ContextItem& context, const std::string& outputDir, bool outputEmpty);

  /**
   * @brief set output directory
   * @param reference to output directory
  */
  void SetOutputDir(const std::string& outputDir);

  /**
   * @brief execute generator of a given context
   * @param context name
   * @param generator identifier
   * @return true if executed successfully
  */
  bool ExecuteGenerator(const std::string& context, std::string& generatorId);

protected:
  ProjMgrKernel* m_kernel = nullptr;
  RteGlobalModel* m_model = nullptr;
  std::list<RtePackage*> m_installedPacks;
  std::map<std::string, ContextItem> m_contexts;
  std::string m_outputDir;

  bool LoadPacks(void);
  bool GetRequiredPdscFiles(const std::string& packRoot, std::set<std::string>& pdscFiles);
  bool CheckRteErrors(void);
  bool CheckType(TypeFilter typeFilter, TypePair type);
  bool GetTypeContent(ContextItem& context);
  bool SetTargetAttributes(ContextItem& context, std::map<std::string, std::string>& attributes);
  bool ProcessPrecedences(ContextItem& context);
  bool ProcessPrecedence(StringCollection& item);
  bool ProcessDevice(ContextItem& context);
  bool ProcessDevicePrecedence(StringCollection& item);
  bool ProcessBoardPrecedence(StringCollection& item);
  bool ProcessToolchain(ContextItem& context);
  bool ProcessPackages(ContextItem& context);
  bool ProcessComponents(ContextItem& context);
  bool ProcessDependencies(ContextItem& context);
  bool ProcessConfigFiles(ContextItem& context);
  bool ProcessGroups(ContextItem& context);
  bool ProcessAccessSequences(ContextItem& context);
  bool AddContext(ProjMgrParser& parser, ContextDesc& descriptor, const TypePair& type, const std::string& cprojectFile, ContextItem& parentContext);
  void AddMiscUniquely(std::vector<MiscItem>& dst, std::vector<std::vector<MiscItem>*>& srcVec);
  void AddStringItemsUniquely(std::vector<std::string>& dst, const std::vector<std::string>& src);
  void RemoveStringItems(std::vector<std::string>& dst, std::vector<std::string>& src);
  bool GetAccessSequence(size_t& offset, std::string& src, std::string& sequence, const char start, const char end);
  void InsertVectorPointers(std::vector<std::string*>& dst, std::vector<std::string>& src);
  void InsertFilesPointers(std::vector<std::string*>& dst, std::vector<GroupNode>& groups);
  void PushBackUniquely(std::vector<std::string>& vec, const std::string& value);
  void MergeStringVector(StringVectorCollection& item);
  void MergeMiscCPP(std::vector<MiscItem>& vec);
  bool AddGroup(const GroupNode& src, std::vector<GroupNode>& dst, ContextItem& context, const std::string root);
  bool AddFile(const FileNode& src, std::vector<FileNode>& dst, ContextItem& context, const std::string root);
  bool AddComponent(const ComponentItem& src, std::vector<ComponentItem>& dst, TypePair type);
  static std::set<std::string> SplitArgs(const std::string& args, const std::string& delimiter = " ");
  static void ApplyFilter(const std::set<std::string>& origin, const std::set<std::string>& filter, std::set<std::string>& result);
  static bool FullMatch(const std::set<std::string>& installed, const std::set<std::string>& required);
  bool AddRequiredComponents(ContextItem& context);
  void GetDeviceItem(const std::string& element, DeviceItem& device) const;
  void GetBoardItem (const std::string& element, BoardItem& board) const;
  bool GetPrecedentValue(std::string& outValue, const std::string& element) const;
  std::string GetDeviceInfoString(const std::string& vendor, const std::string& name, const std::string& processor) const;
};

#endif  // PROJMGRWORKER_H
