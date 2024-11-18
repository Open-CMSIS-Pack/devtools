/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRYAMLPARSER_H
#define PROJMGRYAMLPARSER_H

#include "ProjMgrParser.h"
#include "yaml-cpp/yaml.h"

/**
  * @brief YAML key definitions
*/
static constexpr const char* YAML_APIS = "apis";
static constexpr const char* YAML_API = "api";
static constexpr const char* YAML_ADDPATH = "add-path";
static constexpr const char* YAML_ADDPATH_ASM = "add-path-asm";
static constexpr const char* YAML_ALWAYS = "always";
static constexpr const char* YAML_ARGUMENT = "argument";
static constexpr const char* YAML_ARGUMENTS = "arguments";
static constexpr const char* YAML_ATTR = "attr";
static constexpr const char* YAML_AUTO = "auto";
static constexpr const char* YAML_BASE = "base";
static constexpr const char* YAML_BASE_DIR = "base-dir";
static constexpr const char* YAML_BASE_NAME = "base-name";
static constexpr const char* YAML_BOARD = "board";
static constexpr const char* YAML_BOARD_PACK = "board-pack";
static constexpr const char* YAML_BRANCH_PROTECTION = "branch-protection";
static constexpr const char* YAML_BUILD = "build";
static constexpr const char* YAML_BUILD_GEN = "build-gen";
static constexpr const char* YAML_BUILD_IDX = "build-idx";
static constexpr const char* YAML_BUILD_GEN_IDX = "build-gen-idx";
static constexpr const char* YAML_BUILDTYPES = "build-types";
static constexpr const char* YAML_CATEGORY = "category";
static constexpr const char* YAML_CBUILDS = "cbuilds";
static constexpr const char* YAML_CBUILD = "cbuild";
static constexpr const char* YAML_CBUILD_GENS = "cbuild-gens";
static constexpr const char* YAML_CBUILD_GEN = "cbuild-gen";
static constexpr const char* YAML_CBUILD_PACK = "cbuild-pack";
static constexpr const char* YAML_CBUILD_SET = "cbuild-set";
static constexpr const char* YAML_CDEFAULT = "cdefault";
static constexpr const char* YAML_CLAYERS = "clayers";
static constexpr const char* YAML_CLAYER = "clayer";
static constexpr const char* YAML_CPROJECTS = "cprojects";
static constexpr const char* YAML_CPROJECT = "cproject";
static constexpr const char* YAML_CSOLUTION = "csolution";
static constexpr const char* YAML_CURRENT_GENERATOR = "current-generator";
static constexpr const char* YAML_CONSUMES = "consumes";
static constexpr const char* YAML_COMMAND = "command";
static constexpr const char* YAML_COMPILER = "compiler";
static constexpr const char* YAML_COMPONENT = "component";
static constexpr const char* YAML_COMPONENTS = "components";
static constexpr const char* YAML_CONDITION = "condition";
static constexpr const char* YAML_CONFIGURATION = "configuration";
static constexpr const char* YAML_CONFIGURATIONS = "configurations";
static constexpr const char* YAML_CONNECT = "connect";
static constexpr const char* YAML_CONNECTIONS = "connections";
static constexpr const char* YAML_CONSTRUCTEDFILES = "constructed-files";
static constexpr const char* YAML_CONTEXT = "context";
static constexpr const char* YAML_CONTEXTS = "contexts";
static constexpr const char* YAML_CONTEXT_MAP = "context-map";
static constexpr const char* YAML_COPY_TO = "copy-to";
static constexpr const char* YAML_CREATED_BY = "created-by";
static constexpr const char* YAML_CREATED_FOR = "created-for";
static constexpr const char* YAML_DEBUG = "debug";
static constexpr const char* YAML_DEFAULT = "default";
static constexpr const char* YAML_DEFINE = "define";
static constexpr const char* YAML_DEFINE_ASM = "define-asm";
static constexpr const char* YAML_DELPATH = "del-path";
static constexpr const char* YAML_DEPENDS_ON = "depends-on";
static constexpr const char* YAML_DESCRIPTION = "description";
static constexpr const char* YAML_DEVICE = "device";
static constexpr const char* YAML_DEVICE_PACK = "device-pack";
static constexpr const char* YAML_DOWNLOAD_URL = "download-url";
static constexpr const char* YAML_DSP = "dsp";
static constexpr const char* YAML_ENDIAN = "endian";
static constexpr const char* YAML_ERRORS = "errors";
static constexpr const char* YAML_EXECUTE = "execute";
static constexpr const char* YAML_EXECUTES = "executes";
static constexpr const char* YAML_FILE = "file";
static constexpr const char* YAML_FILES = "files";
static constexpr const char* YAML_FROM_PACK = "from-pack";
static constexpr const char* YAML_FORBOARD = "for-board";
static constexpr const char* YAML_FORCOMPILER = "for-compiler";
static constexpr const char* YAML_FORCONTEXT = "for-context";
static constexpr const char* YAML_FORDEVICE = "for-device";
static constexpr const char* YAML_FORPROJECTPART = "for-project-part";
static constexpr const char* YAML_FPU = "fpu";
static constexpr const char* YAML_GENERATED_BY = "generated-by";
static constexpr const char* YAML_GENERATOR = "generator";
static constexpr const char* YAML_GENERATORS = "generators";
static constexpr const char* YAML_GENERATOR_IMPORT = "generator-import";
static constexpr const char* YAML_GPDSC = "gpdsc";
static constexpr const char* YAML_GROUP = "group";
static constexpr const char* YAML_GROUPS = "groups";
static constexpr const char* YAML_HOST = "host";
static constexpr const char* YAML_ID = "id";
static constexpr const char* YAML_IMPLEMENTED_BY = "implemented-by";
static constexpr const char* YAML_IMPLEMENTS = "implements";
static constexpr const char* YAML_INFO = "info";
static constexpr const char* YAML_INPUT = "input";
static constexpr const char* YAML_INSTANCES = "instances";
static constexpr const char* YAML_LANGUAGE = "language";
static constexpr const char* YAML_LANGUAGE_C = "language-C";
static constexpr const char* YAML_LANGUAGE_CPP = "language-CPP";
static constexpr const char* YAML_LAYER = "layer";
static constexpr const char* YAML_LAYERS = "layers";
static constexpr const char* YAML_LICENSE = "license";
static constexpr const char* YAML_LICENSES = "licenses";
static constexpr const char* YAML_LICENSE_AGREEMENT = "license-agreement";
static constexpr const char* YAML_LINKER = "linker";
static constexpr const char* YAML_MAP = "map";
static constexpr const char* YAML_MESSAGES = "messages";
static constexpr const char* YAML_MISC = "misc";
static constexpr const char* YAML_MISC_ASM = "ASM";
static constexpr const char* YAML_MISC_C = "C";
static constexpr const char* YAML_MISC_CPP = "CPP";
static constexpr const char* YAML_MISC_C_CPP = "C-CPP";
static constexpr const char* YAML_MISC_LIB = "Lib";
static constexpr const char* YAML_MISC_LIBRARY = "Library";
static constexpr const char* YAML_MISC_LINK = "Link";
static constexpr const char* YAML_MISC_LINK_C = "Link-C";
static constexpr const char* YAML_MISC_LINK_CPP = "Link-CPP";
static constexpr const char* YAML_MVE = "mve";
static constexpr const char* YAML_NAME = "name";
static constexpr const char* YAML_NOTFORCONTEXT = "not-for-context";
static constexpr const char* YAML_OPTIMIZE = "optimize";
static constexpr const char* YAML_OPTIONAL = "optional";
static constexpr const char* YAML_OPTIONS = "options";
static constexpr const char* YAML_OUTPUT = "output";
static constexpr const char* YAML_OUTPUTDIRS = "output-dirs";
static constexpr const char* YAML_OUTPUT_CPRJDIR = "cprjdir";
static constexpr const char* YAML_OUTPUT_INTDIR = "intdir";
static constexpr const char* YAML_OUTPUT_OUTDIR = "outdir";
static constexpr const char* YAML_OUTPUT_RTEDIR = "rtedir";
static constexpr const char* YAML_OUTPUT_TMPDIR = "tmpdir";
static constexpr const char* YAML_PACK = "pack";
static constexpr const char* YAML_PACKS = "packs";
static constexpr const char* YAML_PACKS_MISSING = "packs-missing";
static constexpr const char* YAML_PACKS_UNUSED = "packs-unused";
static constexpr const char* YAML_PATH = "path";
static constexpr const char* YAML_PROCESSOR = "processor";
static constexpr const char* YAML_PROJECT = "project";
static constexpr const char* YAML_PROJECTS = "projects";
static constexpr const char* YAML_PROJECT_TYPE = "project-type";
static constexpr const char* YAML_PROVIDES = "provides";
static constexpr const char* YAML_REBUILD = "rebuild";
static constexpr const char* YAML_REGIONS = "regions";
static constexpr const char* YAML_RESOLVED_PACK = "resolved-pack";
static constexpr const char* YAML_RESOLVED_PACKS = "resolved-packs";
static constexpr const char* YAML_RTE = "rte";
static constexpr const char* YAML_RUN = "run";
static constexpr const char* YAML_SCOPE = "scope";
static constexpr const char* YAML_SCRIPT = "script";
static constexpr const char* YAML_SOLUTION = "solution";
static constexpr const char* YAML_SELECT = "select";
static constexpr const char* YAML_SELECTED_BY = "selected-by";
static constexpr const char* YAML_SELECTED_BY_PACK = "selected-by-pack";
static constexpr const char* YAML_SETUPS = "setups";
static constexpr const char* YAML_SETUP = "setup";
static constexpr const char* YAML_SET = "set";
static constexpr const char* YAML_SETTINGS = "settings";
static constexpr const char* YAML_SELECT_COMPILER = "select-compiler";
static constexpr const char* YAML_STATUS = "status";
static constexpr const char* YAML_SWITCH = "switch";
static constexpr const char* YAML_TARGET_CONFIGURATIONS = "target-configurations";
static constexpr const char* YAML_TARGETTYPE = "target-type";
static constexpr const char* YAML_TARGETTYPES = "target-types";
static constexpr const char* YAML_TRUSTZONE = "trustzone";
static constexpr const char* YAML_CORE = "core";
static constexpr const char* YAML_TYPE = "type";
static constexpr const char* YAML_UNDEFINE = "undefine";
static constexpr const char* YAML_UPDATE = "update";
static constexpr const char* YAML_VARIABLES = "variables";
static constexpr const char* YAML_VERSION = "version";
static constexpr const char* YAML_WARNINGS = "warnings";
static constexpr const char* YAML_WORKING_DIR = "working-dir";

/**
  * @brief projmgr parser yaml implementation class, directly coupled to underlying yaml-cpp external library
*/
class ProjMgrYamlParser {
public:
  /**
   * @brief class constructor
  */
  ProjMgrYamlParser(void);

  /**
   * @brief class destructor
  */
  ~ProjMgrYamlParser(void);

  /**
 * @brief parse cdefault
 * @param input cdefault.yml file
 * @param reference to store parsed cdefault item
 * @param checkSchema false to skip schema validation
*/
  bool ParseCdefault(const std::string& input, CdefaultItem& cdefault,
    bool checkSchema);

  /**
   * @brief parse csolution
   * @param input csolution.yml file
   * @param reference to store parsed csolution item
   * @param checkSchema false to skip schema validation
   * @param frozenPacks false to allow missing cbuild-packs.yml file
  */
  bool ParseCsolution(const std::string& input, CsolutionItem& csolution,
    bool checkSchema, bool frozenPacks);

  /**
   * @brief parse cproject
   * @param input cproject.yml file
   * @param reference to store parsed cproject item
   * @param checkSchema false to skip schema validation
  */
  bool ParseCproject(const std::string& input, CsolutionItem& csolution,
    std::map<std::string, CprojectItem>& cprojects, bool single, bool checkSchema);

  /**
   * @brief parse clayer
   * @param input clayer.yml file
   * @param reference to store parsed clayer item
   * @param checkSchema false to skip schema validation
  */
  bool ParseClayer(const std::string& input, std::map<std::string,
    ClayerItem>& clayers, bool checkSchema);

  /**
   * @brief parse cbuild-set
   * @param input path to cbuild-set.yml file
   * @param reference to store parsed cbuildset item
   * @param checkSchema false to skip schema validation
   * @return true if executed successfully
  */
  bool ParseCbuildSet(const std::string& input, CbuildSetItem& cbuildSet, bool checkSchema);


protected:
  bool ParseCbuildPack(const std::string& input, CbuildPackItem& cbuildPack, bool checkSchema);
  void ParseMisc(const YAML::Node& parent, std::vector<MiscItem>& misc);
  void ParseDefine(const YAML::Node& defineNode, std::vector<std::string>& define);
  void ParsePacks(const YAML::Node& parent, const std::string& file, std::vector<PackItem>& packs);
  void ParseResolvedPacks(const YAML::Node& parent, std::vector<ResolvedPackItem>& resolvedPacks);
  void ParseProcessor(const YAML::Node& parent, ProcessorItem& processor);
  void ParseBoolean(const YAML::Node& parent, const std::string& key, bool& value, bool def);
  void ParseString(const YAML::Node& parent, const std::string& key, std::string& value);
  void ParseInt(const YAML::Node& parent, const std::string& key, int& value);
  void ParseVector(const YAML::Node& parent, const std::string& key, std::vector<std::string>& value);
  void ParseVectorOfStringPairs(const YAML::Node& parent, const std::string& key, std::vector<std::pair<std::string, std::string>>& value);
  void ParseVectorOrString(const YAML::Node& parent, const std::string& key, std::vector<std::string>& value);
  bool ParseBuildType(const YAML::Node& parent, const std::string& file, BuildType& buildType);
  void ParseOutput(const YAML::Node& parent, const std::string& file, OutputItem& output);
  void ParseOutputDirs(const YAML::Node& parent, const std::string& file, struct DirectoriesItem& directories);
  void ParseGenerators(const YAML::Node& parent, const std::string& file, GeneratorsItem& generators);
  void ParseExecutes(const YAML::Node& parent, const std::string& file, std::vector<ExecutesItem>& executes);
  void ParseConnections(const YAML::Node& parent, std::vector<ConnectItem>& connects);
  bool ParseTargetType(const YAML::Node& parent, const std::string& file, TargetType& targetType);
  bool ParseBuildTypes(const YAML::Node& parent, const std::string& file, BuildTypes& buildTypes);
  bool ParseTargetTypes(const YAML::Node& parent, const std::string& file, TargetTypes& targetTypes);
  bool ParseContexts(const YAML::Node& parent, CsolutionItem& contexts);
  bool ParseComponents(const YAML::Node& parent, const std::string& file, std::vector<ComponentItem>& components);
  bool ParseSelectableCompilers(const YAML::Node& parent, std::vector<std::string>& selectableCompilers);
  bool ParseFiles(const YAML::Node& parent, const std::string& file, std::vector<FileNode>& files);
  bool ParseGroups(const YAML::Node& parent, const std::string& file, std::vector<GroupNode>& groups);
  bool ParseLayers(const YAML::Node& parent, const std::string& file, std::vector<LayerItem>& layers);
  bool ParseSetups(const YAML::Node& parent, const std::string& file, std::vector<SetupItem>& setups);
  bool ParseTypeFilter(const YAML::Node& parent, TypeFilter& type);
  bool ParseTypePair(std::vector<std::string>& vec, std::vector<TypePair>& typeVec);
  bool ParseLinker(const YAML::Node& parent, const std::string& file, std::vector<LinkerItem>& linker);
  void ParseRte(const YAML::Node& parent, std::string& rteBaseDir);
  bool GetTypes(const std::string& type, std::string& buildType, std::string& targetType, std::string& pattern);
  bool ValidateCdefault(const std::string& input, const YAML::Node& root);
  bool ValidateCsolution(const std::string& input, const YAML::Node& root);
  bool ValidateCproject(const std::string& input, const YAML::Node& root);
  bool ValidateClayer(const std::string& input, const YAML::Node& root);
  bool ValidateCbuildPack(const std::string& input, const YAML::Node& root);
  bool ValidateCbuildSet(const std::string& input, const YAML::Node& root);
  bool ValidateKeys(const std::string& input, const YAML::Node& parent, const std::set<std::string>& keys);
  bool ValidateSequence(const std::string& input, const YAML::Node& parent, const std::string& seqKey);
  bool ValidateMapping(const std::string& input, const YAML::Node& parent, const std::string& seqKey);
  void ParsePortablePath(const YAML::Node& parent, const std::string& file, const std::string& key, std::string& value);
  void ParsePortablePaths(const YAML::Node& parent, const std::string& file, const std::string& key, std::vector<std::string>& value);
  void EnsurePortability(const std::string& file, const YAML::Mark& mark, const std::string& key, std::string& value);

};

#endif  // PROJMGRYAMLPARSER_H
