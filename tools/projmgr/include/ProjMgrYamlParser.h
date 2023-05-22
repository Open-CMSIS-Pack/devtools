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
static constexpr const char* YAML_ADDPATHS = "add-paths";
static constexpr const char* YAML_ADDPATH = "add-path";
static constexpr const char* YAML_ARGUMENT = "argument";
static constexpr const char* YAML_ARGUMENTS = "arguments";
static constexpr const char* YAML_ATTR = "attr";
static constexpr const char* YAML_BASE_NAME = "base-name";
static constexpr const char* YAML_BOARD = "board";
static constexpr const char* YAML_BUILD = "build";
static constexpr const char* YAML_BUILD_IDX = "build-idx";
static constexpr const char* YAML_BUILDTYPES = "build-types";
static constexpr const char* YAML_CATEGORY = "category";
static constexpr const char* YAML_CBUILDS = "cbuilds";
static constexpr const char* YAML_CBUILD = "cbuild";
static constexpr const char* YAML_CDEFAULT = "cdefault";
static constexpr const char* YAML_CLAYERS = "clayers";
static constexpr const char* YAML_CLAYER = "clayer";
static constexpr const char* YAML_CPROJECTS = "cprojects";
static constexpr const char* YAML_CPROJECT = "cproject";
static constexpr const char* YAML_CSOLUTION = "csolution";
static constexpr const char* YAML_CONSUMES = "consumes";
static constexpr const char* YAML_COMMAND = "command";
static constexpr const char* YAML_COMPILER = "compiler";
static constexpr const char* YAML_COMPONENT = "component";
static constexpr const char* YAML_COMPONENTS = "components";
static constexpr const char* YAML_CONDITION = "condition";
static constexpr const char* YAML_CONFIGURATION = "configuration";
static constexpr const char* YAML_CONNECT = "connect";
static constexpr const char* YAML_CONNECTIONS = "connections";
static constexpr const char* YAML_CONSTRUCTEDFILES = "constructed-files";
static constexpr const char* YAML_CONTEXT = "context";
static constexpr const char* YAML_CONTEXTS = "contexts";
static constexpr const char* YAML_CONTEXT_MAP = "context-map";
static constexpr const char* YAML_CREATED_BY = "created-by";
static constexpr const char* YAML_CREATED_FOR = "created-for";
static constexpr const char* YAML_DEBUG = "debug";
static constexpr const char* YAML_DEFAULT = "default";
static constexpr const char* YAML_DEFINES = "defines";
static constexpr const char* YAML_DEFINE = "define";
static constexpr const char* YAML_DELPATHS = "del-paths";
static constexpr const char* YAML_DELPATH = "del-path";
static constexpr const char* YAML_DESCRIPTION = "description";
static constexpr const char* YAML_DEVICE = "device";
static constexpr const char* YAML_ENDIAN = "endian";
static constexpr const char* YAML_FILE = "file";
static constexpr const char* YAML_FILES = "files";
static constexpr const char* YAML_FROM_PACK = "from-pack";
static constexpr const char* YAML_FORBOARD = "for-board";
static constexpr const char* YAML_FORCOMPILER = "for-compiler";
static constexpr const char* YAML_FORCONTEXT = "for-context";
static constexpr const char* YAML_FORDEVICE = "for-device";
static constexpr const char* YAML_FORTYPE = "for-type";
static constexpr const char* YAML_FPU = "fpu";
static constexpr const char* YAML_GENERATED_BY = "generated-by";
static constexpr const char* YAML_GENERATOR = "generator";
static constexpr const char* YAML_GENERATORS = "generators";
static constexpr const char* YAML_GPDSC = "gpdsc";
static constexpr const char* YAML_GROUP = "group";
static constexpr const char* YAML_GROUPS = "groups";
static constexpr const char* YAML_HOST = "host";
static constexpr const char* YAML_ID = "id";
static constexpr const char* YAML_INFO = "info";
static constexpr const char* YAML_LAYER = "layer";
static constexpr const char* YAML_LAYERS = "layers";
static constexpr const char* YAML_LINKER = "linker";
static constexpr const char* YAML_MISC = "misc";
static constexpr const char* YAML_MISC_ASM = "ASM";
static constexpr const char* YAML_MISC_C = "C";
static constexpr const char* YAML_MISC_CPP = "CPP";
static constexpr const char* YAML_MISC_C_CPP = "C-CPP";
static constexpr const char* YAML_MISC_C_STAR = "C*";
static constexpr const char* YAML_MISC_LIB = "Lib";
static constexpr const char* YAML_MISC_LIBRARY = "Library";
static constexpr const char* YAML_MISC_LINK = "Link";
static constexpr const char* YAML_MISC_LINK_C = "Link-C";
static constexpr const char* YAML_MISC_LINK_CPP = "Link-CPP";
static constexpr const char* YAML_NOTFORTYPE = "not-for-type";
static constexpr const char* YAML_NOTFORCONTEXT = "not-for-context";
static constexpr const char* YAML_OPTIMIZE = "optimize";
static constexpr const char* YAML_OUTPUT = "output";
static constexpr const char* YAML_OUTPUTDIRS = "output-dirs";
static constexpr const char* YAML_OUTPUT_CPRJDIR = "cprjdir";
static constexpr const char* YAML_OUTPUT_GENDIR = "gendir";
static constexpr const char* YAML_OUTPUT_INTDIR = "intdir";
static constexpr const char* YAML_OUTPUT_OUTDIR = "outdir";
static constexpr const char* YAML_OUTPUT_RTEDIR = "rtedir";
static constexpr const char* YAML_PACK = "pack";
static constexpr const char* YAML_PACKS = "packs";
static constexpr const char* YAML_PATH = "path";
static constexpr const char* YAML_PROCESSOR = "processor";
static constexpr const char* YAML_PROJECT = "project";
static constexpr const char* YAML_PROJECTS = "projects";
static constexpr const char* YAML_PROVIDES = "provides";
static constexpr const char* YAML_REGIONS = "regions";
static constexpr const char* YAML_SCRIPT = "script";
static constexpr const char* YAML_SOLUTION = "solution";
static constexpr const char* YAML_SELECTED_BY = "selected-by";
static constexpr const char* YAML_SETUPS = "setups";
static constexpr const char* YAML_SETUP = "setup";
static constexpr const char* YAML_SET = "set";
static constexpr const char* YAML_SWITCH = "switch";
static constexpr const char* YAML_TARGETTYPES = "target-types";
static constexpr const char* YAML_TRUSTZONE = "trustzone";
static constexpr const char* YAML_TYPE = "type";
static constexpr const char* YAML_UNDEFINES = "undefines";
static constexpr const char* YAML_UNDEFINE = "undefine";
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
  */
  bool ParseCsolution(const std::string& input, CsolutionItem& csolution,
    bool checkSchema);

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

protected:
  void ParseMisc(const YAML::Node& parent, std::vector<MiscItem>& misc);
  void ParsePacks(const YAML::Node& parent, std::vector<PackItem>& packs);
  void ParseProcessor(const YAML::Node& parent, ProcessorItem& processor);
  void ParseString(const YAML::Node& parent, const std::string& key, std::string& value);
  void ParseVector(const YAML::Node& parent, const std::string& key, std::vector<std::string>& value);
  void ParseVectorOfStringPairs(const YAML::Node& parent, const std::string& key, std::vector<std::pair<std::string, std::string>>& value);
  void ParseVectorOrString(const YAML::Node& parent, const std::string& key, std::vector<std::string>& value);
  void ParseBuildType(const YAML::Node& parent, BuildType& buildType);
  void ParseOutput(const YAML::Node& parent, OutputItem& output);
  void ParseOutputDirs(const YAML::Node& parent, struct DirectoriesItem& directories);
  void ParseConnections(const YAML::Node& parent, std::vector<ConnectItem>& connects);
  void ParseTargetType(const YAML::Node& parent, TargetType& targetType);
  void ParseBuildTypes(const YAML::Node& parent, std::map<std::string, BuildType>& buildTypes);
  void ParseTargetTypes(const YAML::Node& parent, std::map<std::string, TargetType>& targetTypes);
  bool ParseContexts(const YAML::Node& parent, CsolutionItem& contexts);
  bool ParseComponents(const YAML::Node& parent, std::vector<ComponentItem>& components);
  bool ParseFiles(const YAML::Node& parent, std::vector<FileNode>& files);
  bool ParseGroups(const YAML::Node& parent, std::vector<GroupNode>& groups);
  bool ParseLayers(const YAML::Node& parent, std::vector<LayerItem>& layers);
  bool ParseSetups(const YAML::Node& parent, std::vector<SetupItem>& setups);
  bool ParseTypeFilter(const YAML::Node& parent, TypeFilter& type);
  bool ParseTypePair(std::vector<std::string>& vec, std::vector<TypePair>& typeVec);
  bool ParseLinker(const YAML::Node& parent, std::vector<LinkerItem>& linker);
  bool GetTypes(const std::string& type, std::string& buildType, std::string& targetType);
  bool ValidateCdefault(const std::string& input, const YAML::Node& root);
  bool ValidateCsolution(const std::string& input, const YAML::Node& root);
  bool ValidateCproject(const std::string& input, const YAML::Node& root);
  bool ValidateClayer(const std::string& input, const YAML::Node& root);
  bool ValidateKeys(const std::string& input, const YAML::Node& parent, const std::set<std::string>& keys);
  bool ValidateSequence(const std::string& input, const YAML::Node& parent, const std::string& seqKey);
  bool ValidateMapping(const std::string& input, const YAML::Node& parent, const std::string& seqKey);

};

#endif  // PROJMGRYAMLPARSER_H
