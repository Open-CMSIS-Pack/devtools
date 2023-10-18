/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrYamlParser.h"
#include "ProjMgrLogger.h"
#include "ProjMgrUtils.h"
#include "ProjMgrYamlSchemaChecker.h"

#include "RteFsUtils.h"
#include <regex>
#include <string>

using namespace std;

ProjMgrYamlParser::ProjMgrYamlParser(void) {
  // Reserved
}

ProjMgrYamlParser::~ProjMgrYamlParser(void) {
  // Reserved
}

bool ProjMgrYamlParser::ParseCdefault(const string& input,
  CdefaultItem& cdefault, bool checkSchema) {
  try {
    // Validate file schema
    if (checkSchema &&
      !ProjMgrYamlSchemaChecker().Validate(
        input, ProjMgrYamlSchemaChecker::FileType::DEFAULT)) {
      return false;
    }

    cdefault.path = RteFsUtils::MakePathCanonical(input);

    const YAML::Node& root = YAML::LoadFile(input);
    if (!ValidateCdefault(input, root)) {
      return false;
    }

    const YAML::Node& defaultNode = root[YAML_DEFAULT];
    ParseString(defaultNode, YAML_COMPILER, cdefault.compiler);
    ParseMisc(defaultNode, cdefault.misc);
  }
  catch (YAML::Exception& e) {
    ProjMgrLogger::Error(input, e.mark.line + 1, e.mark.column + 1, e.msg);
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ParseCsolution(const string& input,
  CsolutionItem& csolution, bool checkSchema) {
  try {
    // Validate file schema
    if (checkSchema &&
      !ProjMgrYamlSchemaChecker().Validate(
        input, ProjMgrYamlSchemaChecker::FileType::SOLUTION)) {
      return false;
    }

    csolution.path = RteFsUtils::MakePathCanonical(input);
    csolution.directory = RteFsUtils::ParentPath(csolution.path);
    csolution.name = fs::path(input).stem().stem().generic_string();

    const YAML::Node& root = YAML::LoadFile(input);
    if (!ValidateCsolution(input, root)) {
      return false;
    }

    const YAML::Node& solutionNode = root[YAML_SOLUTION];
    if (!ParseContexts(solutionNode, csolution)) {
      return false;
    }

    if (csolution.cprojects.size() == 0) {
      ProjMgrLogger::Error(input, "projects not found");
      return false;
    }

    if (!ParseTargetTypes(solutionNode, csolution.path, csolution.targetTypes)) {
      ProjMgrLogger::Error(input, "target-types not found");
      return false;
    }
    if (!ParseBuildTypes(solutionNode, csolution.path, csolution.buildTypes)) {
      return false;
    }
    ParseOutputDirs(solutionNode, csolution.path, csolution.directories);
    if (!ParseTargetType(solutionNode, csolution.path, csolution.target)) {
      return false;
    }
    ParsePacks(solutionNode, csolution.path, csolution.packs);
    csolution.enableCdefault = solutionNode[YAML_CDEFAULT].IsDefined();
    ParseGenerators(solutionNode, csolution.path, csolution.generators);

  } catch (YAML::Exception& e) {
    ProjMgrLogger::Error(input, e.mark.line + 1, e.mark.column + 1, e.msg);
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ParseCproject(const string& input,
  CsolutionItem& csolution, map<string, CprojectItem>& cprojects,
  bool single, bool checkSchema) {
  CprojectItem cproject;
  try {
    // Validate file schema
    if (checkSchema &&
      !ProjMgrYamlSchemaChecker().Validate(
        input, ProjMgrYamlSchemaChecker::FileType::PROJECT)) {
      return false;
    }

    const YAML::Node& root = YAML::LoadFile(input);
    if (!ValidateCproject(input, root)) {
      return false;
    }

    cproject.path = RteFsUtils::MakePathCanonical(input);
    cproject.directory = RteFsUtils::ParentPath(cproject.path);
    cproject.name = fs::path(input).stem().stem().generic_string();

    const YAML::Node& projectNode = root[YAML_PROJECT];

    ParseOutput(projectNode, cproject.path, cproject.output);

    ParseTargetType(projectNode, cproject.path, cproject.target);

    ParsePacks(projectNode, cproject.path, cproject.packs);

    if (!ParseComponents(projectNode, cproject.path, cproject.components)) {
      return false;
    }
    if (!ParseGroups(projectNode, cproject.path, cproject.groups)) {
      return false;
    }
    if (!ParseLayers(projectNode, cproject.path, cproject.clayers)) {
      return false;
    }
    if (!ParseSetups(projectNode, cproject.path, cproject.setups)) {
      return false;
    }

    ParseConnections(projectNode, cproject.connections);

    ParseLinker(projectNode, cproject.path, cproject.linker);

    ParseGenerators(projectNode, cproject.path, cproject.generators);

    ParseRte(projectNode, cproject.rteBaseDir);

  } catch (YAML::Exception& e) {
    ProjMgrLogger::Error(input, e.mark.line + 1, e.mark.column + 1, e.msg);
    return false;
  }

  cprojects[input] = cproject;

  if (single) {
    csolution.directory = cproject.directory;
    csolution.contexts.push_back({ input });
  }
  return true;
}

bool ProjMgrYamlParser::ParseClayer(const string& input,
  map<string, ClayerItem>& clayers, bool checkSchema) {
  if (clayers.find(input) != clayers.end()) {
    // already parsed
    return true;
  }
  ClayerItem clayer;
  try {
    // Validate file schema
    const bool cgen = fs::path(input).stem().extension().generic_string() == ".cgen";
    if (checkSchema) {
      if (!ProjMgrYamlSchemaChecker().Validate(input, cgen  ?
        ProjMgrYamlSchemaChecker::FileType::GENERATOR_IMPORT :
        ProjMgrYamlSchemaChecker::FileType::LAYER)) {
        return false;
      }
    }

    const YAML::Node& root = YAML::LoadFile(input);
    if (!cgen && !ValidateClayer(input, root)) {
      return false;
    }

    clayer.path = RteFsUtils::MakePathCanonical(input);
    clayer.directory = RteFsUtils::ParentPath(clayer.path);
    clayer.name = fs::path(input).stem().stem().generic_string();

    const YAML::Node& layerNode = root[cgen ? YAML_GENERATOR_IMPORT : YAML_LAYER];
    map<const string, string&> projectChildren = {
      {YAML_FORBOARD, clayer.forBoard},
      {YAML_FORDEVICE, clayer.forDevice},
      {YAML_TYPE, clayer.type},
    };
    for (const auto& item : projectChildren) {
      ParseString(layerNode, item.first, item.second);
    }

    ParseTargetType(layerNode, clayer.path, clayer.target);

    ParsePacks(layerNode, clayer.path, clayer.packs);

    if (!ParseComponents(layerNode, clayer.path, clayer.components)) {
      return false;
    }
    if (!ParseGroups(layerNode, clayer.path, clayer.groups)) {
      return false;
    }

    ParseConnections(layerNode, clayer.connections);

    ParseLinker(layerNode, clayer.path, clayer.linker);

    ParseGenerators(layerNode, clayer.path, clayer.generators);
  }
  catch (YAML::Exception& e) {
    ProjMgrLogger::Error(input, e.mark.line + 1, e.mark.column + 1, e.msg);
    return false;
  }
  clayers[input] = clayer;
  return true;
}

bool ProjMgrYamlParser::ParseCbuildSet(const string& input, CbuildSetItem& cbuildSet, bool checkSchema) {
  // Validate file schema
  if (!ProjMgrYamlSchemaChecker().Validate(
    input, ProjMgrYamlSchemaChecker::FileType::BUILDSET)) {
    return false;
  }

  try {
    const YAML::Node& root = YAML::LoadFile(input);
    if (checkSchema && !ValidateCbuildSet(input, root)) {
      return false;
    }

    const YAML::Node& cbuildSetNode = root[YAML_CBUILD_SET];
    ParseString(cbuildSetNode, YAML_GENERATED_BY, cbuildSet.generatedBy);
    ParseVector(cbuildSetNode, YAML_CONTEXTS, cbuildSet.contexts);
    ParseString(cbuildSetNode, YAML_COMPILER, cbuildSet.compiler);
  }
  catch (YAML::Exception& e) {
    ProjMgrLogger::Error(input, e.mark.line + 1, e.mark.column + 1, e.msg);
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ParseGlobalGenerator(const string& input,
  std::map<std::string, GlobalGeneratorItem>& generators, bool checkSchema) {

  try {
    // Validate file schema
    if (checkSchema && !ProjMgrYamlSchemaChecker().Validate(
      input, ProjMgrYamlSchemaChecker::FileType::GENERATOR)) {
      return false;
    }

    const YAML::Node& root = YAML::LoadFile(input);
    const YAML::Node& generatorNode = root[YAML_GENERATOR];
    for (const auto& generatorEntry : generatorNode) {
      GlobalGeneratorItem generator;
      map<const string, string&> generatorChildren = {
        {YAML_ID, generator.id},
        {YAML_DOWNLOAD_URL, generator.downloadUrl},
        {YAML_RUN, generator.run},
      };
      for (const auto& item : generatorChildren) {
        ParseString(generatorEntry, item.first, item.second);
      }
      ParsePortablePath(generatorEntry, input, YAML_PATH, generator.path, false);
      generators[generator.id] = generator;
    }
  }
  catch (YAML::Exception& e) {
    ProjMgrLogger::Error(input, e.mark.line + 1, e.mark.column + 1, e.msg);
    return false;
  }
  return true;
}

// EnsurePortability checks the presence of backslash, case inconsistency and absolute path
// It clears the string 'value' when it is an absolute path
void ProjMgrYamlParser::EnsurePortability(const string& file, const YAML::Mark& mark, const string& key, string& value, bool checkExist) {
  if (value.find('\\') != string::npos) {
    ProjMgrLogger::Warn(file, mark.line + 1, mark.column + 1, "'" + value + "' contains non-portable backslash, use forward slash instead");
  }
  if (!value.empty()) {
    if (fs::path(value).is_absolute()) {
      ProjMgrLogger::Warn(file, mark.line + 1, mark.column + 1, "absolute path '" + value + "' is not portable, use relative path instead");
      value.clear();
    } else {
      const string parentDir = RteFsUtils::ParentPath(file);
      const string original = RteFsUtils::LexicallyNormal(fs::path(parentDir).append(value).generic_string());
      if (RteFsUtils::Exists(original)) {
        error_code ec;
        string canonical = fs::canonical(original, ec).generic_string();
        if (!canonical.empty() && (original != canonical)) {
          ProjMgrLogger::Warn(file, mark.line + 1, mark.column + 1, "'" + value + "' has case inconsistency, use '" + RteFsUtils::RelativePath(canonical, parentDir) + "' instead");
        }
      } else if (checkExist && !regex_match(value, regex(".*\\$.*\\$.*"))) {
        ProjMgrLogger::Warn(file, mark.line + 1, mark.column + 1, "path '" + value + "' was not found");
      }
    }
  }
}

void ProjMgrYamlParser::ParsePortablePath(const YAML::Node& parent, const string& file, const string& key, string& value, bool checkExist) {
  ParseString(parent, key, value);
  YAML::Mark mark = parent[key].IsDefined() ? parent[key].Mark() : YAML::Mark();
  EnsurePortability(file, mark, key, value, checkExist);
}

void ProjMgrYamlParser::ParsePortablePaths(const YAML::Node& parent, const string& file, const string& key, vector<string>& value) {
  ParseVector(parent, key, value);
  auto node = parent[key].begin();
  for (auto& item : value) {
    YAML::Mark mark = (*node++).Mark();
    EnsurePortability(file, mark, key, item, true);
  }
}

void ProjMgrYamlParser::ParseBoolean(const YAML::Node& parent, const string& key, bool& value, bool def) {
  if (parent[key].IsDefined()) {
    value = parent[key].as<bool>();
    if (parent[key].Type() == YAML::NodeType::Null) {
      value = def;
    }
  }
}

void ProjMgrYamlParser::ParseString(const YAML::Node& parent, const string& key, string& value) {
  if (parent[key].IsDefined()) {
    value = parent[key].as<string>();
    if (parent[key].Type() == YAML::NodeType::Null) {
      value = "";
    }
  }
}

void ProjMgrYamlParser::ParseVector(const YAML::Node& parent, const string& key, vector<string>& value) {
  if (parent[key].IsDefined() && parent[key].IsSequence()) {
    value = parent[key].as<vector<string>>();
  }
}

void ProjMgrYamlParser::ParseDefine(const YAML::Node& parent, vector<string>& define) {
  if (parent[YAML_DEFINE].IsDefined() && parent[YAML_DEFINE].IsSequence()) {
    for (const auto& item : parent[YAML_DEFINE]) {
      // accept map elements <string, string> or a string
      if (item.IsDefined()) {
        if (item.IsMap()) {
          const auto& elements = item.as<map<string, string>>();
          for (auto element : elements) {
            if (YAML::IsNullString(element.second)) {
              element.second = "";
            }
            define.push_back(element.first + "=" + element.second);
          }
        }
        else {
          define.push_back(item.as<string>());
        }
      }
    }
  }
}

void ProjMgrYamlParser::ParseVectorOfStringPairs(const YAML::Node& parent, const string& key, vector<pair<string, string>>& value) {
  if (parent[key].IsDefined() && parent[key].IsSequence()) {
    for (const auto& item : parent[key]) {
      // accept map elements <string, string> or a string
      if (item.IsDefined()) {
        if (item.IsMap()) {
          const auto& elements = item.as<map<string, string>>();
          for (auto element : elements) {
            if (YAML::IsNullString(element.second)) {
              element.second = "";
            }
            value.push_back(element);
          }
        } else {
          value.push_back(make_pair(item.as<string>(), ""));
        }
      }
    }
  }
}

void ProjMgrYamlParser::ParseVectorOrString(const YAML::Node& parent, const string& key, vector<string>& value) {
  ParseVector(parent, key, value);
  if (value.empty()) {
    string strValue;
    ParseString(parent, key, strValue);
    if (!strValue.empty()) {
      value.push_back({ strValue });
    }
  }
}

void ProjMgrYamlParser::ParseProcessor(const YAML::Node& parent, ProcessorItem& processor) {
  if (parent[YAML_PROCESSOR].IsDefined()) {
    const YAML::Node& processorNode = parent[YAML_PROCESSOR];
    map<const string, string&> processorChildren = {
      {YAML_TRUSTZONE, processor.trustzone},
      {YAML_FPU, processor.fpu},
      {YAML_ENDIAN, processor.endian},
    };
    for (const auto& item : processorChildren) {
      ParseString(processorNode, item.first, item.second);
    }
  }
}

bool ProjMgrYamlParser::ParseComponents(const YAML::Node& parent, const string& file, vector<ComponentItem>& components) {
  if (parent[YAML_COMPONENTS].IsDefined()) {
    const YAML::Node& componentsNode = parent[YAML_COMPONENTS];
    for (const auto& componentEntry : componentsNode) {
      ComponentItem componentItem;
      if (!ParseTypeFilter(componentEntry, componentItem.type)) {
        return false;
      }
      ParseString(componentEntry, YAML_COMPONENT, componentItem.component);
      ParseString(componentEntry, YAML_CONDITION, componentItem.condition);
      ParseString(componentEntry, YAML_FROM_PACK, componentItem.fromPack);
      if (!ParseBuildType(componentEntry, file, componentItem.build)) {
        return false;
      }
      components.push_back(componentItem);
    }
  }
  return true;
}

void ProjMgrYamlParser::ParseOutput(const YAML::Node& parent, const string& file, OutputItem& output) {
  if (parent[YAML_OUTPUT].IsDefined()) {
    const YAML::Node& outputNode = parent[YAML_OUTPUT];
    ParsePortablePath(outputNode, file, YAML_BASE_NAME, output.baseName, false);
    ParseVectorOrString(outputNode, YAML_TYPE, output.type);
  }
}

void ProjMgrYamlParser::ParseGenerators(const YAML::Node& parent, const string& file, GeneratorsItem& generators) {
  if (parent[YAML_GENERATORS].IsDefined()) {
    const YAML::Node& generatorsNode = parent[YAML_GENERATORS];
    ParsePortablePath(generatorsNode, file, YAML_BASE_DIR, generators.baseDir);
    if (generatorsNode[YAML_OPTIONS].IsDefined()) {
      const YAML::Node& optionsNode = generatorsNode[YAML_OPTIONS];
      for (const auto& optionsEntry : optionsNode) {
        string generator, path;
        ParseString(optionsEntry, YAML_GENERATOR, generator);
        ParsePortablePath(optionsEntry, file, YAML_PATH, path);
        generators.options[generator] = path;
      }
    }
  }
}

void ProjMgrYamlParser::ParseRte(const YAML::Node& parent, string& rteBaseDir) {
  if (parent[YAML_RTE].IsDefined()) {
    const YAML::Node& rteNode = parent[YAML_RTE];
    ParseString(rteNode, YAML_BASE_DIR, rteBaseDir);
  }
}

bool ProjMgrYamlParser::ParseTypeFilter(const YAML::Node& parent, TypeFilter& type) {
  vector<string> include, exclude;
  ParseVectorOrString(parent, YAML_FORCONTEXT, include);
  ParseVectorOrString(parent, YAML_NOTFORCONTEXT, exclude);
  if (!ParseTypePair(include, type.include) ||
      !ParseTypePair(exclude, type.exclude)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ParseTypePair(vector<string>& vec, vector<TypePair>& typeVec) {
  bool valid = true;
  for (const auto& item : vec) {
    TypePair typePair;
    if (!GetTypes(item, typePair.build, typePair.target)) {
      valid = false;
    }
    typeVec.push_back(typePair);
  }
  return valid;
}

bool ProjMgrYamlParser::GetTypes(const string& type, string& buildType, string& targetType) {
  size_t buildDelimiter = type.find(".");
  size_t targetDelimiter = type.find("+");
  if (((buildDelimiter > 0) && (targetDelimiter > 0)) || (buildDelimiter == targetDelimiter)) {
    ProjMgrLogger::Error("type '" + type + "': delimiters '.' or '+' not set)");
    return false;
  }
  if (targetDelimiter > buildDelimiter) {
    if (targetDelimiter < string::npos) {
      targetType = type.substr(targetDelimiter + 1, string::npos);
      targetDelimiter--;
    }
    buildType = type.substr(buildDelimiter + 1, targetDelimiter);
  }
  else if (buildDelimiter > targetDelimiter) {
    if (buildDelimiter < string::npos) {
      buildType = type.substr(buildDelimiter + 1, string::npos);
      buildDelimiter--;
    }
    targetType = type.substr(targetDelimiter + 1, buildDelimiter);
  }
  return true;
}

void ProjMgrYamlParser::ParseMisc(const YAML::Node& parent, vector<MiscItem>& misc) {
  if (parent[YAML_MISC].IsDefined()) {
    const YAML::Node& miscNode = parent[YAML_MISC];
    for (const auto& miscEntry : miscNode) {
      MiscItem miscItem;
      ParseString(miscEntry, YAML_FORCOMPILER, miscItem.forCompiler);
      ParseVector(miscEntry, YAML_MISC_C, miscItem.c);
      ParseVector(miscEntry, YAML_MISC_CPP, miscItem.cpp);
      ParseVector(miscEntry, YAML_MISC_C_CPP, miscItem.c_cpp);
      ParseVector(miscEntry, YAML_MISC_ASM, miscItem.as);
      ParseVector(miscEntry, YAML_MISC_LINK, miscItem.link);
      ParseVector(miscEntry, YAML_MISC_LINK_C, miscItem.link_c);
      ParseVector(miscEntry, YAML_MISC_LINK_CPP, miscItem.link_cpp);
      ParseVector(miscEntry, YAML_MISC_LIB, miscItem.lib);
      ParseVector(miscEntry, YAML_MISC_LIBRARY, miscItem.library);
      misc.push_back(miscItem);
    }
  }
}

void ProjMgrYamlParser::ParsePacks(const YAML::Node& parent, const string& file, vector<PackItem>& packs) {
  if (parent[YAML_PACKS].IsDefined()) {
    const YAML::Node& packNode = parent[YAML_PACKS];
    for (const auto& packEntry : packNode) {
      PackItem packItem;
      ParseString(packEntry, YAML_PACK, packItem.pack);
      ParsePortablePath(packEntry, file, YAML_PATH, packItem.path);
      ParseTypeFilter(packEntry, packItem.type);
      packs.push_back(packItem);
    }
  }
}

bool ProjMgrYamlParser::ParseFiles(const YAML::Node& parent, const string& file, vector<FileNode>& files) {
  if (parent[YAML_FILES].IsDefined()) {
    const YAML::Node& filesNode = parent[YAML_FILES];
    for (const auto& fileEntry : filesNode) {
      FileNode fileItem;
      if (!ParseTypeFilter(fileEntry, fileItem.type)) {
        return false;
      }
      ParsePortablePath(fileEntry, file, YAML_FILE, fileItem.file);
      ParseVectorOrString(fileEntry, YAML_FORCOMPILER, fileItem.forCompiler);
      ParseString(fileEntry, YAML_CATEGORY, fileItem.category);
      if (!ParseBuildType(fileEntry, file, fileItem.build)) {
        return false;
      }
      files.push_back(fileItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseGroups(const YAML::Node& parent, const string& file, vector<GroupNode>& groups) {
  if (parent[YAML_GROUPS].IsDefined()) {
    const YAML::Node& groupsNode = parent[YAML_GROUPS];
    for (const auto& groupEntry : groupsNode) {
      GroupNode groupItem;
      if (!ParseTypeFilter(groupEntry, groupItem.type)) {
        return false;
      }
      if (!ParseFiles(groupEntry, file, groupItem.files)) {
        return false;
      }
      ParseString(groupEntry, YAML_GROUP, groupItem.group);
      ParseVectorOrString(groupEntry, YAML_FORCOMPILER, groupItem.forCompiler);
      if (!ParseBuildType(groupEntry, file, groupItem.build)) {
        return false;
      }
      ParseGroups(groupEntry, file, groupItem.groups);
      groups.push_back(groupItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseLayers(const YAML::Node& parent, const string& file, vector<LayerItem>& layers) {
  if (parent[YAML_LAYERS].IsDefined()) {
    const YAML::Node& layersNode = parent[YAML_LAYERS];
    for (const auto& layerEntry : layersNode) {
      LayerItem layerItem;
      if (!ParseTypeFilter(layerEntry, layerItem.typeFilter)) {
        return false;
      }
      ParsePortablePath(layerEntry, file, YAML_LAYER, layerItem.layer);
      ParseString(layerEntry, YAML_TYPE, layerItem.type);
      ParseBoolean(layerEntry, YAML_OPTIONAL, layerItem.optional, true);
      layers.push_back(layerItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseSetups(const YAML::Node& parent, const string& file, vector<SetupItem>& setups) {
  if (parent[YAML_SETUPS].IsDefined()) {
    const YAML::Node& setupsNode = parent[YAML_SETUPS];
    for (const auto& setupEntry : setupsNode) {
      SetupItem setupItem;
      if (!ParseTypeFilter(setupEntry, setupItem.type)) {
        return false;
      }
      ParseString(setupEntry, YAML_SETUP, setupItem.description);
      ParseVectorOrString(setupEntry, YAML_FORCOMPILER, setupItem.forCompiler);
      if (!ParseBuildType(setupEntry, file, setupItem.build)) {
        return false;
      }
      ParseOutput(setupEntry, file, setupItem.output);
      ParseLinker(setupEntry, file, setupItem.linker);
      ParseProcessor(setupEntry, setupItem.build.processor);
      setups.push_back(setupItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseContexts(const YAML::Node& parent, CsolutionItem& csolution) {
  if (parent[YAML_PROJECTS].IsDefined()) {
    const YAML::Node& projectsNode = parent[YAML_PROJECTS];
    for (const auto& projectsEntry : projectsNode) {
      ContextDesc descriptor;
      if (!ParseTypeFilter(projectsEntry, descriptor.type)) {
        return false;
      }
      ParsePortablePath(projectsEntry, csolution.path, YAML_PROJECT, descriptor.cproject);
      error_code ec;
      descriptor.cproject = RteFsUtils::RelativePath(fs::canonical(fs::path(csolution.directory).append(descriptor.cproject), ec).generic_string(), csolution.directory);
      if (!descriptor.cproject.empty()) {
        csolution.contexts.push_back(descriptor);
        ProjMgrUtils::PushBackUniquely(csolution.cprojects, descriptor.cproject);
      }
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseBuildTypes(const YAML::Node& parent, const string& file, map<string, BuildType>& buildTypes) {
  std::vector<std::string> invalidBuildTypes;
  if (parent[YAML_BUILDTYPES].IsDefined()) {
    const YAML::Node& buildTypesNode = parent[YAML_BUILDTYPES];
    for (const auto& typeEntry : buildTypesNode) {
      string typeItem;
      BuildType build;
      ParseString(typeEntry, YAML_TYPE, typeItem);
      if (RteUtils::CountDelimiters(typeItem, ".") > 0 || RteUtils::CountDelimiters(typeItem, "+") > 0) {
        invalidBuildTypes.push_back(typeItem);
        continue;
      }
      if (!ParseBuildType(typeEntry, file, build)) {
        return false;
      }
      buildTypes[typeItem] = build;
    }
  }
  if (invalidBuildTypes.size() > 0) {
    string errMsg = "invalid build type(s):\n";
    for (const auto& buildType : invalidBuildTypes) {
      errMsg += "  " + buildType + "\n";
    }
    ProjMgrLogger::Error(errMsg);
    return false;
  }
  return true;
}

void ProjMgrYamlParser::ParseOutputDirs(const YAML::Node& parent, const std::string& file, struct DirectoriesItem& directories) {
  if (parent[YAML_OUTPUTDIRS].IsDefined()) {
    const YAML::Node& outputDirsNode = parent[YAML_OUTPUTDIRS];
    map<const string, string&> outputDirsChildren = {
      {YAML_OUTPUT_CPRJDIR, directories.cprj},
      {YAML_OUTPUT_INTDIR, directories.intdir},
      {YAML_OUTPUT_OUTDIR, directories.outdir},
    };
    for (const auto& item : outputDirsChildren) {
      ParsePortablePath(outputDirsNode, file, item.first, item.second, false);
    }
  }
}

void ProjMgrYamlParser::ParseConnections(const YAML::Node& parent, vector<ConnectItem>& connects) {
  if (parent[YAML_CONNECTIONS].IsDefined()) {
    const YAML::Node& connectsNode = parent[YAML_CONNECTIONS];
    for (const auto& connectEntry : connectsNode) {
      ConnectItem connectItem;
      ParseString(connectEntry, YAML_CONNECT, connectItem.connect);
      ParseString(connectEntry, YAML_SET, connectItem.set);
      ParseString(connectEntry, YAML_INFO, connectItem.info);
      map<const string, vector<pair<string, string>>&> connectConsumesProvides = {
        {YAML_PROVIDES, connectItem.provides},
        {YAML_CONSUMES, connectItem.consumes},
      };
      for (const auto& item : connectConsumesProvides) {
        ParseVectorOfStringPairs(connectEntry, item.first, item.second);
      }
      connects.push_back(connectItem);
    }
  }
}

bool ProjMgrYamlParser::ParseLinker(const YAML::Node& parent, const string& file, vector<LinkerItem>& linker) {
  if (parent[YAML_LINKER].IsDefined()) {
    const YAML::Node& linkerNode = parent[YAML_LINKER];
    for (const auto& linkerEntry : linkerNode) {
      LinkerItem linkerItem;
      if (!ParseTypeFilter(linkerEntry, linkerItem.typeFilter)) {
        return false;
      }
      ParseDefine(linkerEntry, linkerItem.defines);
      ParseVectorOrString(linkerEntry, YAML_FORCOMPILER, linkerItem.forCompiler);
      ParsePortablePath(linkerEntry, file, YAML_REGIONS, linkerItem.regions);
      ParsePortablePath(linkerEntry, file, YAML_SCRIPT, linkerItem.script);
      linker.push_back(linkerItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseTargetTypes(const YAML::Node& parent, const string& file, map<string, TargetType>& targetTypes) {
  std::vector<std::string> invalidTargetTypes;
  const YAML::Node& targetTypesNode = parent[YAML_TARGETTYPES];
  for (const auto& typeEntry : targetTypesNode) {
    string typeItem;
    TargetType target;
    ParseString(typeEntry, YAML_TYPE, typeItem);
    if (RteUtils::CountDelimiters(typeItem, ".") > 0 || RteUtils::CountDelimiters(typeItem, "+") > 0) {
      invalidTargetTypes.push_back(typeItem);
      continue;
    }
    ParseTargetType(typeEntry, file, target);
    targetTypes[typeItem] = target;
  }
  if (invalidTargetTypes.size() > 0) {
    string errMsg = "invalid target type(s):\n";
    for (const auto& targetType : invalidTargetTypes) {
      errMsg += "  " + targetType + "\n";
    }
    ProjMgrLogger::Error(errMsg);
    return false;
  }
  return (targetTypes.size() == 0 ? false : true);
}

bool ProjMgrYamlParser::ParseBuildType(const YAML::Node& parent, const string& file, BuildType& buildType) {
  map<const string, string&> buildChildren = {
    {YAML_COMPILER, buildType.compiler},
    {YAML_OPTIMIZE, buildType.optimize},
    {YAML_DEBUG, buildType.debug},
    {YAML_WARNINGS, buildType.warnings},
    {YAML_LANGUAGE_C, buildType.languageC},
    {YAML_LANGUAGE_CPP, buildType.languageCpp},
  };
  for (const auto& item : buildChildren) {
    ParseString(parent, item.first, item.second);
  }
  ParseProcessor(parent, buildType.processor);
  ParseMisc(parent, buildType.misc);
  ParseDefine(parent, buildType.defines);
  ParseVector(parent, YAML_UNDEFINE, buildType.undefines);
  ParsePortablePaths(parent, file, YAML_ADDPATH, buildType.addpaths);
  ParseVector(parent, YAML_DELPATH, buildType.delpaths);
  ParseVectorOfStringPairs(parent, YAML_VARIABLES, buildType.variables);

  std::vector<std::string> contextMap, invalidContexts;
  ParseVectorOrString(parent, YAML_CONTEXT_MAP, contextMap);
  for (const auto& contextEntry : contextMap) {
    ContextName context;
    if (!ProjMgrUtils::ParseContextEntry(contextEntry, context)) {
      invalidContexts.push_back(contextEntry);
    }
    buildType.contextMap.push_back(context);
  }

  if (invalidContexts.size() > 0) {
    string errMsg = "context-map specifies invalid context(s):\n";
    for (const auto& context : invalidContexts) {
      errMsg += "  " + context + "\n";
    }
    ProjMgrLogger::Error(errMsg);
    return false;
  }

  return true;
}

bool ProjMgrYamlParser::ParseTargetType(const YAML::Node& parent, const string& file, TargetType& targetType) {
  map<const string, string&> targetChildren = {
    {YAML_BOARD, targetType.board},
    {YAML_DEVICE, targetType.device},
  };
  for (const auto& item : targetChildren) {
    ParseString(parent, item.first, item.second);
  }
  return ParseBuildType(parent, file, targetType.build);
}

// Validation Maps
const set<string> defaultKeys = {
  YAML_COMPILER,
  YAML_MISC,
};

const set<string> solutionKeys = {
  YAML_DESCRIPTION,
  YAML_PROJECTS,
  YAML_TARGETTYPES,
  YAML_BUILDTYPES,
  YAML_OUTPUTDIRS,
  YAML_PACKS,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_DEFINE,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_DELPATH,
  YAML_MISC,
  YAML_VARIABLES,
  YAML_CREATED_BY,
  YAML_CREATED_FOR,
  YAML_CDEFAULT,
  YAML_GENERATORS,
};

const set<string> projectsKeys = {
  YAML_PROJECT,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
};

const set<string> projectKeys = {
  YAML_DESCRIPTION,
  YAML_OUTPUT,
  YAML_PACKS,
  YAML_DEVICE,
  YAML_BOARD,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_DEFINE,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_DELPATH,
  YAML_MISC,
  YAML_COMPONENTS,
  YAML_GROUPS,
  YAML_LAYERS,
  YAML_SETUPS,
  YAML_CONNECTIONS,
  YAML_LINKER,
  YAML_GENERATORS,
  YAML_RTE,
};

const set<string> setupKeys = {
  YAML_SETUP,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
  YAML_FORCOMPILER,
  YAML_OUTPUT,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_DEFINE,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_DELPATH,
  YAML_MISC,
  YAML_LINKER,
  YAML_PROCESSOR,
};

const set<string> layerKeys = {
  YAML_DESCRIPTION,
  YAML_FORBOARD,
  YAML_FORDEVICE,
  YAML_TYPE,
  YAML_PACKS,
  YAML_DEVICE,
  YAML_BOARD,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_DEFINE,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_DELPATH,
  YAML_MISC,
  YAML_COMPONENTS,
  YAML_GROUPS,
  YAML_CONNECTIONS,
  YAML_LINKER,
  YAML_GENERATORS,
};

const set<string> cbuildSetKeys = {
  YAML_GENERATED_BY,
  YAML_CONTEXTS,
  YAML_COMPILER,
};

const set<string> targetTypeKeys = {
  YAML_TYPE,
  YAML_DEVICE,
  YAML_BOARD,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_DEFINE,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_DELPATH,
  YAML_MISC,
  YAML_VARIABLES,
  YAML_CONTEXT_MAP,
};

const set<string> buildTypeKeys = {
  YAML_TYPE,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_DEFINE,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_DELPATH,
  YAML_MISC,
  YAML_VARIABLES,
  YAML_CONTEXT_MAP,
};

const set<string> outputDirsKeys = {
  YAML_OUTPUT_CPRJDIR,
  YAML_OUTPUT_INTDIR,
  YAML_OUTPUT_OUTDIR,
};

const set<string> outputKeys = {
  YAML_BASE_NAME,
  YAML_TYPE,
};

const set<string> generatorsKeys = {
  YAML_BASE_DIR,
  YAML_OPTIONS,
};

const set<string> rteKeys = {
  YAML_BASE_DIR,
};

const set<string> processorKeys = {
  YAML_TRUSTZONE,
  YAML_FPU,
  YAML_ENDIAN,
};

const set<string> miscKeys = {
  YAML_FORCOMPILER,
  YAML_MISC_C,
  YAML_MISC_CPP,
  YAML_MISC_C_CPP,
  YAML_MISC_ASM,
  YAML_MISC_LINK,
  YAML_MISC_LINK_C,
  YAML_MISC_LINK_CPP,
  YAML_MISC_LIB,
  YAML_MISC_LIBRARY,
};

const set<string> packsKeys = {
  YAML_PACK,
  YAML_PATH,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
};

const set<string> componentsKeys = {
  YAML_COMPONENT,
  YAML_CONDITION,
  YAML_FROM_PACK,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_DEFINE,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_DELPATH,
  YAML_MISC,
};

const set<string> connectionsKeys = {
  YAML_CONNECT,
  YAML_SET,
  YAML_INFO,
  YAML_PROVIDES,
  YAML_CONSUMES,
};

const set<string> linkerKeys = {
  YAML_REGIONS,
  YAML_SCRIPT,
  YAML_DEFINE,
  YAML_FORCOMPILER,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
};

const set<string> layersKeys = {
  YAML_LAYER,
  YAML_TYPE,
  YAML_OPTIONAL,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
};

const set<string> groupsKeys = {
  YAML_GROUP,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
  YAML_FORCOMPILER,
  YAML_GROUPS,
  YAML_FILES,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_DEFINE,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_DELPATH,
  YAML_MISC,
};

const set<string> filesKeys = {
  YAML_FILE,
  YAML_FORCONTEXT,
  YAML_NOTFORCONTEXT,
  YAML_FORCOMPILER,
  YAML_CATEGORY,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_LANGUAGE_C,
  YAML_LANGUAGE_CPP,
  YAML_DEFINE,
  YAML_UNDEFINE,
  YAML_ADDPATH,
  YAML_DELPATH,
  YAML_MISC,
};

const map<string, set<string>> sequences = {
  {YAML_PROJECTS, projectsKeys},
  {YAML_TARGETTYPES, targetTypeKeys},
  {YAML_BUILDTYPES, buildTypeKeys},
  {YAML_MISC, miscKeys},
  {YAML_PACKS, packsKeys},
  {YAML_COMPONENTS, componentsKeys},
  {YAML_CONNECTIONS, connectionsKeys},
  {YAML_LAYERS, layersKeys},
  {YAML_GROUPS, groupsKeys},
  {YAML_FILES, filesKeys},
  {YAML_SETUPS, setupKeys},
  {YAML_LINKER, linkerKeys},
};

const map<string, set<string>> mappings = {
  {YAML_PROCESSOR, processorKeys},
  {YAML_OUTPUTDIRS, outputDirsKeys},
  {YAML_OUTPUT, outputKeys},
  {YAML_GENERATORS, generatorsKeys},
  {YAML_RTE, rteKeys},
};

bool ProjMgrYamlParser::ValidateCdefault(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_DEFAULT,
  };
  if (!ValidateKeys(input, root, rootKeys)) {
    return false;
  }
  const YAML::Node& defaultNode = root[YAML_DEFAULT];
  if (!ValidateKeys(input, defaultNode, defaultKeys)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ValidateCsolution(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_SOLUTION,
  };
  if (!ValidateKeys(input, root, rootKeys)) {
    return false;
  }
  const YAML::Node& solutionNode = root[YAML_SOLUTION];
  if (!ValidateKeys(input, solutionNode, solutionKeys)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ValidateCproject(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_PROJECT,
  };
  if (!ValidateKeys(input, root, rootKeys)) {
    return false;
  }
  const YAML::Node& projectNode = root[YAML_PROJECT];
  if (!ValidateKeys(input, projectNode, projectKeys)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ValidateClayer(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_LAYER,
  };
  if (!ValidateKeys(input, root, rootKeys)) {
    return false;
  }
  const YAML::Node& layerNode = root[YAML_LAYER];
  if (!ValidateKeys(input, layerNode, layerKeys)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ValidateCbuildSet(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_CBUILD_SET,
  };
  if (!ValidateKeys(input, root, rootKeys)) {
    return false;
  }
  const YAML::Node& contextSetNode = root[YAML_CBUILD_SET];
  if (!ValidateKeys(input, contextSetNode, cbuildSetKeys)) {
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ValidateKeys(const string& input, const YAML::Node& parent, const set<string>& keys) {
  bool valid = true;
  for (const auto& item : parent) {
    const string& key = item.first.as<string>();
    if (keys.find(item.first.as<string>()) == keys.end()) {
      ProjMgrLogger::Warn(input, item.first.Mark().line + 1, item.first.Mark().column + 1, "key '" + item.first.as<string>() + "' was not recognized");
    }
    if (item.second.IsSequence()) {
      if (!ValidateSequence(input, item.second, key)) {
        valid = false;
      }
    } else if (sequences.find(key) != sequences.end()) {
      ProjMgrLogger::Error(input, item.first.Mark().line + 1, item.first.Mark().column + 1, "node '" + item.first.as<string>() + "' shall contain sequence elements");
      valid = false;
    }
    if (item.second.IsMap()) {
      if (!ValidateMapping(input, item.second, key)) {
        valid = false;
      }
    } else if (mappings.find(key) != mappings.end()) {
      ProjMgrLogger::Error(input, item.first.Mark().line + 1, item.first.Mark().column + 1, "node '" + item.first.as<string>() + "' shall contain mapping elements");
      valid = false;
    }
  }
  return valid;
}

bool ProjMgrYamlParser::ValidateSequence(const string& input, const YAML::Node& parent, const string& key) {
  bool valid = true;
  if (sequences.find(key) != sequences.end()) {
    set<string> keys = sequences.at(key);
    for (const YAML::Node& entry : parent) {
      if (!ValidateKeys(input, entry, keys)) {
        valid = false;
      }
    }
  }
  return valid;
}

bool ProjMgrYamlParser::ValidateMapping(const string& input, const YAML::Node& parent, const string& key) {
  bool valid = true;
  if (mappings.find(key) != mappings.end()) {
    set<string> keys = mappings.at(key);
    if (!ValidateKeys(input, parent, keys)) {
      valid = false;
    }
  }
  return valid;
}
