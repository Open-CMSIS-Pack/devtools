/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRWORKER_H
#define PROJMGRWORKER_H

#include "ProjMgrKernel.h"
#include "ProjMgrParser.h"
#include "ProjMgrUtils.h"

/**
 * @brief interfaces validation result containing
 *        boolean valid,
 *        conflicted interfaces,
 *        overflowed interfaces,
 *        incompatible interfaces,
 *        provided interfaces
*/
struct InterfacesValidationResult {
  bool valid;
  StrVec conflicts;
  StrPairVec overflows;
  StrPairVec incompatibles;
  StrPairPtrVec provides;
};

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
 * @brief pack info containing
 *        pack name,
 *        pack vendor,
 *        pack version
*/
struct PackInfo {
  std::string name;
  std::string vendor;
  std::string version;
};

/**
 * @brief package item containing
 *        pack information pack,
 *        path to pack     path
*/
struct PackageItem {
  PackInfo    pack;
  std::string path;
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
  std::string revision;
};

/**
 * @brief target item containing
 *        target-type board,
 *        target-type device name,
*/
struct TargetItem {
  std::string board;
  std::string device;
};

/**
 * @brief selected component item containing
 *        pointer to component instance
 *        pointer to input component item
 *        generator identifier
*/
struct SelectedComponentItem {
  RteComponentInstance* instance;
  ComponentItem* item;
  std::string generator;
};

/**
 * @brief component file item containing
 *        file name
 *        file attribute
 *        file category
 *        file version
*/
struct ComponentFileItem {
  std::string name;
  std::string attr;
  std::string category;
  std::string version;
};

/**
 * @brief translation control item containing
 *        final context controls after processing,
 *        csolution controls,
 *        cproject controls,
 *        project setup controls,
 *        target-type controls,
 *        build-type controls,
 *        layers translation controls,
*/
struct TranslationControl {
  BuildType processed;
  BuildType csolution;
  BuildType cproject;
  BuildType setup;
  BuildType target;
  BuildType build;
  std::map<std::string, BuildType> clayers;
};

/**
 * @brief project context item containing
 *        pointer to csolution,
 *        pointer to cproject,
 *        map of pointers to clayers,
 *        pointer to rte project,
 *        pointer to rte target,
 *        pointer to rte filtered model,
 *        map of project dependencies,
 *        translation controls,
 *        target-type item,
 *        parent csolution target properties,
 *        directories,
 *        build-type/target-type pair,
 *        project name,
 *        project description,
 *        output type,
 *        device selection,
 *        board selection,
 *        list of package requirements,
 *        map of required pdsc files and optionally its local path
 *        list of component requirements,
 *        compiler string in short syntax,
 *        toolchain with parsed name and version,
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
  CdefaultItem* cdefault = nullptr;
  CsolutionItem* csolution = nullptr;
  CprojectItem* cproject = nullptr;
  std::map<std::string, ClayerItem*> clayers;
  RteProject* rteActiveProject = nullptr;
  RteTarget* rteActiveTarget = nullptr;
  RteModel* rteFilteredModel = nullptr;
  RteItem* rteComponents;
  TranslationControl controls;
  TargetItem targetItem;
  DirectoriesItem directories;
  TypePair type;
  std::string name;
  std::string description;
  std::string outputType;
  std::string device;
  std::string board;
  std::vector<PackageItem> packRequirements;
  std::map<std::string, std::pair<std::string, std::string>> pdscFiles;
  std::vector<PackInfo>missingPacks;
  std::vector<std::pair<ComponentItem, std::string>> componentRequirements;
  std::string compiler;
  ToolchainItem toolchain;
  std::map<std::string, std::string> targetAttributes;
  std::map<std::string, RtePackage*> packages;
  std::map<std::string, SelectedComponentItem> components;
  std::vector<std::tuple<RteItem::ConditionResult, std::string, std::set<std::string>, std::set<std::string>>> validationResults;
  std::map<std::string, std::map<std::string, RteFileInstance*>> configFiles;
  std::map<std::string, std::vector<ComponentFileItem>> componentFiles;
  std::vector<GroupNode> groups;
  std::map<std::string, std::string> filePaths;
  std::map<std::string, RteGenerator*> generators;
  std::map<std::string, std::pair<std::string, std::string>> gpdscs;
  StrVecMap compatibleLayers;
  std::string linkerScript;
  bool precedences;
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
  * @brief set policy for packs loading [latest|all|required]
*/
enum class LoadPacksPolicy
{
  DEFAULT = 0,
  LATEST,
  ALL,
  REQUIRED
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
   * @brief set parser
   * @param pointer to parser
  */
  void SetParser(ProjMgrParser* parser);

  /**
   * @brief process context
   * @param reference to context
   * @param loadGpdsc boolean automatically load gpdsc, default true
   * @param resolveDependencies boolean automatically resolve dependencies, default true
   * @param updateRteFiles boolean update RTE files, default true
   * @return true if executed successfully
  */
  bool ProcessContext(ContextItem& context, bool loadGpdsc = true, bool resolveDependencies = true, bool updateRteFiles = true);

  /**
   * @brief list available packs
   * @param reference to list of packs
   * @param list only missing packs
   * @param filter words to filter results
   * @return true if executed successfully
  */
  bool ListPacks(std::vector<std::string>& packs, bool missingPacks, const std::string& filter = RteUtils::EMPTY_STRING);

  /**
   * @brief list available boards
   * @param reference to list of boards
   * @param filter words to filter results
   * @return true if executed successfully
  */
  bool ListBoards(std::vector<std::string>& boards, const std::string& filter = RteUtils::EMPTY_STRING);

  /**
   * @brief list available devices
   * @param reference to list of devices
   * @param filter words to filter results
   * @return true if executed successfully
  */
  bool ListDevices(std::vector<std::string>& devices, const std::string& filter = RteUtils::EMPTY_STRING);

  /**
   * @brief list available components
   * @param reference to list of components
   * @param filter words to filter results
   * @return true if executed successfully
  */
  bool ListComponents(std::vector<std::string>& components, const std::string& filter = RteUtils::EMPTY_STRING);

  /**
   * @brief list available dependencies
   * @param reference to list of dependencies
   * @param filter words to filter results
   * @return true if executed successfully
  */
  bool ListDependencies(std::vector<std::string>& dependencies, const std::string& filter = RteUtils::EMPTY_STRING);

  /**
   * @brief list contexts
   * @param reference to list of contexts
   * @param filter words to filter results
   * @return true if executed successfully
  */
  bool ListContexts(std::vector<std::string>& contexts, const std::string& filter = RteUtils::EMPTY_STRING);

  /**
   * @brief list generators of a given context
   * @param reference to list of generators
   * @return true if executed successfully
  */
  bool ListGenerators(std::vector<std::string>& generators);

  /**
  * @brief list available, referenced or compatible layers
  * @param reference to list of layers
  * @return true if executed successfully
  */
  bool ListLayers(std::vector<std::string>& layers);

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
   * @brief set output directory
   * @param reference to output directory
  */
  void SetOutputDir(const std::string& outputDir);

  /**
   * @brief set check schema
   * @param boolean check schema
  */
  void SetCheckSchema(bool checkSchema);

  /**
   * @brief set load packs policy
   * @param reference to load packs policy
  */
  void SetLoadPacksPolicy(const LoadPacksPolicy& policy);

  /**
   * @brief execute generator of a given context
   * @param generator identifier
   * @return true if executed successfully
  */
  bool ExecuteGenerator(std::string& generatorId);

  /**
   * @brief initialize model
   * @return true if executed successfully
  */
  bool InitializeModel(void);

  /**
   * @brief load all relevant packs
   * @return true if executed successfully
  */
  bool LoadAllRelevantPacks(void);

  /**
   * @brief parse context selection
   * @param contexts pattern (wildcards are allowed)
   * @return true if executed successfully
  */
  bool ParseContextSelection(const std::string& contextSelection);

  /**
   * @brief check if context is selected
   * @param context name
   * @return true if it is selected
  */
  bool IsContextSelected(const std::string& context);

protected:
  ProjMgrParser* m_parser = nullptr;
  ProjMgrKernel* m_kernel = nullptr;
  RteGlobalModel* m_model = nullptr;
  std::list<RtePackage*> m_loadedPacks;
  std::map<std::string, ContextItem> m_contexts;
  std::map<std::string, ContextItem>* m_contextsPtr;
  std::list<std::string> m_selectedContexts;
  std::string m_outputDir;
  std::string m_packRoot;
  LoadPacksPolicy m_loadPacksPolicy;
  bool m_checkSchema;

  bool LoadPacks(ContextItem& context);
  bool GetRequiredPdscFiles(ContextItem& context, const std::string& packRoot, std::set<std::string>& errMsgs);
  bool CheckRteErrors(void);
  bool CheckCompiler(const std::vector<std::string>& forCompiler, const std::string& selectedCompiler);
  bool CheckType(const TypeFilter& typeFilter, const TypePair& type);
  bool GetTypeContent(ContextItem& context);
  bool GetProjectSetup(ContextItem& context);
  bool InitializeTarget(ContextItem& context);
  bool SetTargetAttributes(ContextItem& context, std::map<std::string, std::string>& attributes);
  bool ProcessPrecedences(ContextItem& context);
  bool ProcessPrecedence(StringCollection& item);
  bool ProcessDevice(ContextItem& context);
  bool ProcessDevicePrecedence(StringCollection& item);
  bool ProcessBoardPrecedence(StringCollection& item);
  bool ProcessToolchain(ContextItem& context);
  bool ProcessPackages(ContextItem& context);
  bool ProcessComponents(ContextItem& context);
  bool ProcessGpdsc(ContextItem& context);
  bool ProcessConfigFiles(ContextItem& context);
  bool ProcessComponentFiles(ContextItem& context);
  bool ProcessGroups(ContextItem& context);
  bool ProcessInterfaces(ContextItem& context);
  bool ProcessSequencesRelatives(ContextItem& context);
  bool ProcessSequencesRelatives(ContextItem& context, std::vector<std::string>& src, const std::string& ref = std::string());
  bool ProcessSequencesRelatives(ContextItem& context, BuildType& build, const std::string& ref = std::string());
  bool ProcessSequenceRelative(ContextItem& context, std::string& item, const std::string& ref = std::string());
  bool AddContext(ProjMgrParser& parser, ContextDesc& descriptor, const TypePair& type, const std::string& cprojectFile, ContextItem& parentContext);
  bool ValidateContext(ContextItem& context);
  bool FormatValidationResults(std::set<std::string>& results, const ContextItem& context);
  void UpdateMisc(std::vector<MiscItem>& vec, const std::string& compiler);
  void AddMiscUniquely(MiscItem& dst, std::vector<std::vector<MiscItem>*>& srcVec);
  void AddMiscUniquely(MiscItem& dst, std::vector<MiscItem>& srcVec);
  void AddStringItemsUniquely(std::vector<std::string>& dst, const std::vector<std::string>& src);
  void RemoveStringItems(std::vector<std::string>& dst, std::vector<std::string>& src);
  bool GetAccessSequence(size_t& offset, const std::string& src, std::string& sequence, const char start, const char end);
  void MergeStringVector(StringVectorCollection& item);
  bool AddGroup(const GroupNode& src, std::vector<GroupNode>& dst, ContextItem& context, const std::string root);
  bool AddFile(const FileNode& src, std::vector<FileNode>& dst, ContextItem& context, const std::string root);
  bool AddComponent(const ComponentItem& src, const std::string& layer, std::vector<std::pair<ComponentItem, std::string>>& dst, TypePair type);
  static std::set<std::string> SplitArgs(const std::string& args, const std::string& delimiter = " ");
  static void ApplyFilter(const std::set<std::string>& origin, const std::set<std::string>& filter, std::set<std::string>& result);
  static bool FullMatch(const std::set<std::string>& installed, const std::set<std::string>& required);
  bool AddRequiredComponents(ContextItem& context);
  void GetDeviceItem(const std::string& element, DeviceItem& device) const;
  void GetBoardItem (const std::string& element, BoardItem& board) const;
  bool GetPrecedentValue(std::string& outValue, const std::string& element) const;
  std::string GetDeviceInfoString(const std::string& vendor, const std::string& name, const std::string& processor) const;
  std::string GetBoardInfoString(const std::string& vendor, const std::string& name, const std::string& revision) const;
  std::vector<PackageItem> GetFilteredPacks(const PackageItem& packItem, const std::string& rtePath) const;
  ToolchainItem GetToolchain(const std::string& compiler);
  bool IsPreIncludeByTarget(const RteTarget* activeTarget, const std::string& preInclude);
  InterfacesValidationResult ValidateInterfaces(ContextItem& context, const StrVec& genericLayers = StrVec());
  void PrintInterfaceValidation(InterfacesValidationResult result);
  bool CollectLayersFromPacks(ContextItem& context, StrVecMap& clayers);
  bool DiscoverMatchingLayers(ContextItem& context);
  void GetAllCombinations(const StrVecMap& src, const StrVecMap::iterator it, std::vector<StrVec>& combinations, const StrVec& previous = StrVec());
};

#endif  // PROJMGRWORKER_H
