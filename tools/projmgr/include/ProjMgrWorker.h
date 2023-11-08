/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRWORKER_H
#define PROJMGRWORKER_H

#include "ProjMgrExtGenerator.h"
#include "ProjMgrKernel.h"
#include "ProjMgrParser.h"
#include "ProjMgrUtils.h"

/**
 * @brief connections validation result containing
 *        boolean valid,
 *        conflicted connections,
 *        overflowed connections,
 *        incompatible connections,
 *        missed provided combined connections,
 *        provided connections
*/
struct ConnectionsValidationResult {
  bool valid;
  StrVec conflicts;
  StrPairVec overflows;
  StrPairVec incompatibles;
  std::vector<ConnectionsCollection> missedCollections;
  StrPairPtrVec provides;
};

/**
 * @brief layers discovering variables
 *        required layer types,
 *        missed required types,
 *        optional type flags,
 *        generic clayers from search path,
 *        generic clayers from packs,
 *        candidate layers,
*/
struct LayersDiscovering {
  StrVec requiredLayerTypes;
  StrVec missedRequiredTypes;
  BoolMap optionalTypeFlags;
  StrVecMap genericClayersFromSearchPath;
  StrVecMap genericClayersFromPacks;
  StrVecMap candidateClayers;
};

/**
 * @brief connections lists
 *        list of consumes
 *        list of provides
*/
struct ConnectionsList {
  StrPairPtrVec consumes;
  StrPairPtrVec provides;
};

/**
 * @brief Environment list
 *        cmsis_pack_root,
 *        cmsis_compiler_root
*/
struct EnvironmentList {
  std::string cmsis_pack_root;
  std::string cmsis_compiler_root;
};

/**
 * @brief toolchain item containing
 *        toolchain name,
 *        toolchain version,
 *        toolchain required version,
 *        toolchain required range in the format <min>[:<max>],
 *        toolchain root,
 *        toolchain config
*/
struct ToolchainItem {
  std::string name;
  std::string version;
  std::string required;
  std::string range;
  std::string root;
  std::string config;
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
 *        file language
 *        file scope
 *        file version
*/
struct ComponentFileItem {
  std::string name;
  std::string attr;
  std::string category;
  std::string language;
  std::string scope;
  std::string version;
};

/**
 * @brief gpdsc item containing
 *        component id
 *        generator id
 *        working directory
*/
struct GpdscItem {
  std::string component;
  std::string generator;
  std::string workingDir;
};

/**
 * @brief translation control item containing
 *        final context controls after processing,
 *        csolution controls,
 *        cproject controls,
 *        target-type controls,
 *        build-type controls,
 *        list of project setup controls,
 *        map of layers translation controls,
*/
struct TranslationControl {
  BuildType processed;
  BuildType csolution;
  BuildType cproject;
  BuildType target;
  BuildType build;
  std::vector<BuildType> setups;
  std::map<std::string, BuildType> clayers;
};

/**
 * @brief linker context item containing
 *        list of linker scripts for processing,
 *        list of linker regions files for processing,
 *        list of defines
 *        processed regions file,
 *        processed script file
*/
struct LinkerContextItem {
  StrVec scriptList;
  StrVec regionsList;
  StrVec defines;
  std::string regions;
  std::string script;
};

/**
 * @brief context type item containing
 *        list of all build types including mapped ones
 *        list of all target types including mapped ones
 *        map of missing build types
 *        map of missing target types
*/
struct ContextTypesItem {
  StrVec allBuildTypes;
  StrVec allTargetTypes;
  BoolMap missingBuildTypes;
  BoolMap missingTargetTypes;
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
 *        list of output files,
 *        output type,
 *        device selection,
 *        board selection,
 *        device item struct,
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
 *        map of compatible layers,
 *        valid connections,
 *        linker options,
 *        map of variables,
 *        external generator directory,
 *        device pack,
 *        board pack,
 *        boolean processed precedences
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
  OutputTypes outputTypes;
  std::string device;
  std::string board;
  DeviceItem deviceItem;
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
  std::map<std::string, std::vector<ComponentFileItem>> generatorInputFiles;
  std::vector<GroupNode> groups;
  std::map<std::string, std::string> filePaths;
  std::map<std::string, RteGenerator*> generators;
  std::map<std::string, GpdscItem> gpdscs;
  StrVecMap compatibleLayers;
  std::vector<ConnectionsCollectionVec> validConnections;
  LinkerContextItem linker;
  std::map<std::string, std::string> variables;
  StrMap extGenDir;
  RtePackage* devicePack;
  RtePackage* boardPack;
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
  ProjMgrWorker(ProjMgrParser* parser, ProjMgrExtGenerator* extGenerator);

  /**
   * @brief class destructor
  */
  ~ProjMgrWorker(void);

  /**
   * @brief process context
   * @param reference to context
   * @param loadGenFiles boolean automatically load generated files, default true
   * @param resolveDependencies boolean automatically resolve dependencies, default true
   * @param updateRteFiles boolean update RTE files, default true
   * @return true if executed successfully
  */
  bool ProcessContext(ContextItem& context, bool loadGenFiles = true, bool resolveDependencies = true, bool updateRteFiles = true);

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
   * @brief list configuration files
   * @param reference to list of file description strings
   * @param filter words to filter results
   * @return true if executed successfully
  */
  bool ListConfigs(std::vector<std::string>& dependencies, const std::string& filter = RteUtils::EMPTY_STRING);

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
  bool ListContexts(std::vector<std::string>& contexts, const std::string& filter = RteUtils::EMPTY_STRING, const bool ymlOrder = false);

  /**
   * @brief list generators of a given context
   * @param reference to list of generators
   * @return true if executed successfully
  */
  bool ListGenerators(std::vector<std::string>& generators);

  /**
  * @brief list available, referenced or compatible layers
  * @param reference to list of layers
  * @param reference clayer search path
  * @return true if executed successfully
  */
  bool ListLayers(std::vector<std::string>& layers, const std::string& clayerSearchPath);

  /**
  * @brief list installed toolchains
  * @param reference to vector of toolchain items
  * @return true if executed successfully
  */
  bool ListToolchains(std::vector<ToolchainItem>& toolchains);

  /**
  * @brief list environment configurations
  * @param reference to EnvironmentList struct
  * @return true if executed successfully
  */
  bool ListEnvironment(EnvironmentList& env);

  /**
  * @brief list config files
  * @param reference to vector of config files info
  * @return true if executed successfully
  */
  bool ListConfigFiles(std::vector<std::string>& configFiles);

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
   * @brief get yml ordered contexts
   * @param reference to contexts
  */
  void GetYmlOrderedContexts(std::vector<std::string> &contexts);

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
   * @brief set verbose mode
   * @param boolean verbose
  */
  void SetVerbose(bool verbose);

  /**
   * @brief set debug verbosity
   * @param boolean debug
  */
  void SetDebug(bool debug);

  /**
   * @brief set dry-run mode
   * @param boolean dryRun
  */
  void SetDryRun(bool dryRun);

  /**
   * @brief set load packs policy
   * @param reference to load packs policy
  */
  void SetLoadPacksPolicy(const LoadPacksPolicy& policy);

  /**
   * @brief set vector of environment variables
   * @param reference to vector of environment variables
  */
  void SetEnvironmentVariables(const StrVec& envVars);

  /**
   * @brief set selected toolchain
   * @param reference to selected toolchain
  */
  void SetSelectedToolchain(const std::string& selectedToolchain);

  /**
   * @brief execute generator of a given context
   * @param generator identifier
   * @return true if executed successfully
  */
  bool ExecuteGenerator(std::string& generatorId);

  /**
   * @brief execute external generator of a given context
   * @param generator identifier
   * @return true if executed successfully
  */
  bool ExecuteExtGenerator(std::string& generatorId);

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
   * @param context replacement pattern (wildcards are allowed)
   * @return true if executed successfully
  */
  bool ParseContextSelection(
    const std::vector<std::string>& contextSelection,
    const std::string& contextReplace = RteUtils::EMPTY_STRING,
    const bool checkCbuildSet = false);

  /**
   * @brief check if context is selected
   * @param context name
   * @return true if it is selected
  */
  bool IsContextSelected(const std::string& context);

  /**
   * @brief get compiler root directory
   * @return string compiler root directory
  */
  std::string GetCompilerRoot(void);

  /**
   * @brief get pack root directory
   * @return string pack root directory
  */
  std::string GetPackRoot(void);

  /**
   * @brief retrieve all context types, including mapped ones
  */
  void RetrieveAllContextTypes(void);

  /**
   * @brief print missing filters
  */
  void PrintMissingFilters(void);

  /**
   * @brief get selected toolchain
   * @return string selected toolchain
  */
  std::string GetSelectedToochain(void);

  /**
   * @brief process global generators for a given context
   * @param context selected context item
   * @param generator identifier
   * @param reference to project type
   * @param reference to sibling contexts
   * @return true if executed successfully
  */
  bool ProcessGlobalGenerators(ContextItem* context, const std::string& generatorId,
    std::string& projectType, StrVec& siblings);

protected:
  ProjMgrParser* m_parser = nullptr;
  ProjMgrKernel* m_kernel = nullptr;
  RteGlobalModel* m_model = nullptr;
  ProjMgrExtGenerator* m_extGenerator = nullptr;
  std::list<RtePackage*> m_loadedPacks;
  std::vector<ToolchainItem> m_toolchains;
  StrVec m_toolchainConfigFiles;
  StrVec m_missingToolchains;
  StrVec m_envVars;
  std::vector<std::string> m_ymlOrderedContexts;
  std::map<std::string, ContextItem> m_contexts;
  std::map<std::string, ContextItem>* m_contextsPtr;
  std::vector<std::string> m_selectedContexts;
  std::string m_outputDir;
  std::string m_packRoot;
  std::string m_compilerRoot;
  std::string m_selectedToolchain;
  LoadPacksPolicy m_loadPacksPolicy;
  ContextTypesItem m_types;
  bool m_checkSchema;
  bool m_verbose;
  bool m_debug;
  bool m_dryRun;

  bool LoadPacks(ContextItem& context);
  bool GetRequiredPdscFiles(ContextItem& context, const std::string& packRoot, std::set<std::string>& errMsgs);
  bool CheckRteErrors(void);
  bool CheckBoardDeviceInLayer(const ContextItem& context, const ClayerItem& clayer);
  bool CheckCompiler(const std::vector<std::string>& forCompiler, const std::string& selectedCompiler);
  bool CheckType(const TypeFilter& typeFilter, const std::vector<TypePair>& typeVec);
  bool CheckContextFilters(const TypeFilter& typeFilter, const ContextItem& context);
  bool GetTypeContent(ContextItem& context);
  bool GetProjectSetup(ContextItem& context);
  bool InitializeTarget(ContextItem& context);
  bool SetTargetAttributes(ContextItem& context, std::map<std::string, std::string>& attributes);
  bool ProcessPrecedences(ContextItem& context, bool rerun = false);
  bool ProcessPrecedence(StringCollection& item);
  bool ProcessCompilerPrecedence(StringCollection& item, bool acceptRedefinition = false);
  bool ProcessDevice(ContextItem& context);
  bool ProcessDevicePrecedence(StringCollection& item);
  bool ProcessBoardPrecedence(StringCollection& item);
  bool ProcessToolchain(ContextItem& context);
  bool ProcessPackages(ContextItem& context);
  bool ProcessComponents(ContextItem& context);
  RteComponent* ProcessComponent(ContextItem& context, ComponentItem& item, RteComponentMap& componentMap);
  bool ProcessGpdsc(ContextItem& context);
  bool ProcessConfigFiles(ContextItem& context);
  bool ProcessComponentFiles(ContextItem& context);
  bool ProcessGroups(ContextItem& context);
  bool ProcessSequencesRelatives(ContextItem& context, bool rerun);
  bool ProcessSequencesRelatives(ContextItem& context, std::vector<std::string>& src, const std::string& ref = std::string(), bool withHeadingDot = false);
  bool ProcessSequencesRelatives(ContextItem& context, BuildType& build, const std::string& ref = std::string());
  bool ProcessSequenceRelative(ContextItem& context, std::string& item, const std::string& ref = std::string(), bool withHeadingDot = false);
  bool ProcessOutputFilenames(ContextItem& context);
  bool ProcessLinkerOptions(ContextItem& context);
  bool ProcessLinkerOptions(ContextItem& context, const LinkerItem& linker, const std::string& ref);
  bool ProcessProcessorOptions(ContextItem& context);
  void AddContext(ContextDesc& descriptor, const TypePair& type, ContextItem& parentContext);
  bool ValidateContext(ContextItem& context);
  bool FormatValidationResults(std::set<std::string>& results, const ContextItem& context);
  void UpdateMisc(std::vector<MiscItem>& vec, const std::string& compiler);
  void AddMiscUniquely(MiscItem& dst, std::vector<std::vector<MiscItem>*>& srcVec);
  void AddMiscUniquely(MiscItem& dst, std::vector<MiscItem>& srcVec);
  void AddStringItemsUniquely(std::vector<std::string>& dst, const std::vector<std::string>& src);
  void RemoveDefines(std::vector<std::string>& dst, std::vector<std::string>& src);
  void RemoveStringItems(std::vector<std::string>& dst, std::vector<std::string>& src);
  bool GetAccessSequence(size_t& offset, const std::string& src, std::string& sequence, const char start, const char end);
  void MergeStringVector(StringVectorCollection& item);
  void MergeDefines(StringVectorCollection& item);
  bool AddGroup(const GroupNode& src, std::vector<GroupNode>& dst, ContextItem& context, const std::string root);
  bool AddFile(const FileNode& src, std::vector<FileNode>& dst, ContextItem& context, const std::string root);
  bool AddComponent(const ComponentItem& src, const std::string& layer, std::vector<std::pair<ComponentItem, std::string>>& dst, TypePair type, ContextItem& context);
  static std::set<std::string> SplitArgs(const std::string& args, const std::string& delimiter = " ");
  static void ApplyFilter(const std::vector<std::string>& origin, const std::set<std::string>& filter, std::vector<std::string>& result);
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
  void PrintConnectionsValidation(ConnectionsValidationResult result, std::string& msg);
  bool CollectLayersFromPacks(ContextItem& context, StrVecMap& clayers);
  bool CollectLayersFromSearchPath(const std::string& clayerSearchPath, StrVecMap& clayers);
  void GetRequiredLayerTypes(ContextItem& context, LayersDiscovering& discover);
  bool GetCandidateLayers(LayersDiscovering& discover);
  bool ProcessCandidateLayers(ContextItem& context, LayersDiscovering& discover);
  bool ProcessLayerCombinations(ContextItem& context, LayersDiscovering& discover);
  bool DiscoverMatchingLayers(ContextItem& context, std::string clayerSearchPath);
  void CollectConnections(ContextItem& context, ConnectionsCollectionVec& connections);
  void GetConsumesProvides(const ConnectionsCollectionVec& collection, ConnectionsList& connections);
  ConnectionsCollectionMap ClassifyConnections(const ConnectionsCollectionVec& connections, BoolMap optionalTypeFlags);
  ConnectionsValidationResult ValidateConnections(ConnectionsCollectionVec combination);
  void GetAllCombinations(const ConnectionsCollectionMap& src, const ConnectionsCollectionMap::iterator& it,
    std::vector<ConnectionsCollectionVec>& combinations, const ConnectionsCollectionVec& previous = ConnectionsCollectionVec());
  void GetAllSelectCombinations(const ConnectPtrVec& src, const ConnectPtrVec::iterator& it,
    std::vector<ConnectPtrVec>& combinations);
  void PushBackUniquely(ConnectionsCollectionVec& vec, const ConnectionsCollection& value);
  void PushBackUniquely(std::vector<ToolchainItem>& vec, const ToolchainItem& value);
  std::string ExpandString(const std::string& src, const StrMap& variables);
  void GetRegisteredToolchains(void);
  bool GetLatestToolchain(ToolchainItem& toolchain);
  bool GetToolchainConfig(const std::string& name, const std::string& version, std::string& configPath, std::string& selectedConfigVersion);
  bool IsConnectionSubset(const ConnectionsCollection& connectionSubset, const ConnectionsCollection& connectionSuperset);
  bool IsCollectionSubset(const ConnectionsCollectionVec& collectionSubset, const ConnectionsCollectionVec& collectionSuperset);
  void RemoveRedundantSubsets(std::vector<ConnectionsCollectionVec>& validConnections);
  bool ProvidedConnectionsMatch(ConnectionsCollection collection, ConnectionsList connections);
  StrSet GetValidSets(ContextItem& context, const std::string& clayer);
  void SetDefaultLinkerScript(ContextItem& context);
  void CheckAndGenerateRegionsHeader(ContextItem& context);
  bool GenerateRegionsHeader(ContextItem& context, std::string& generatedRegionsFile);
  void UpdatePartialReferencedContext(ContextItem& context, std::string& contextName);
  void ExpandAccessSequence(const ContextItem& context, const ContextItem& refContext, const std::string& sequence, std::string& item, bool withHeadingDot);
  bool GetGeneratorDir(const RteGenerator* generator, ContextItem& context, const std::string& layer, std::string& genDir);
  bool GetGeneratorDir(const std::string& generatorId, ContextItem& context, const std::string& layer, std::string& genDir);
  bool GetExtGeneratorDir(const std::string& generatorId, ContextItem& context, const std::string& layer, std::string& genDir);
  bool ParseContextLayers(ContextItem& context);
  bool AddPackRequirements(ContextItem& context, const std::vector<PackItem> packRequirements);
  void InsertPackRequirements(const std::vector<PackItem>& src, std::vector<PackItem>& dst, std::string base);
  void CheckTypeFilterSpelling(const TypeFilter& typeFilter);
  void CheckCompilerFilterSpelling(const std::string& compiler);
  bool ProcessGeneratedLayers(ContextItem& context);
  void CheckDeviceAttributes(const std::string& device, const ProcessorItem& userSelection, const StrMap& targetAttributes);
};

#endif  // PROJMGRWORKER_H
