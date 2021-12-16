/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrYamlParser.h"
#include "ProjMgrLogger.h"
#include "RteFsUtils.h"
#include <string>

using namespace std;

ProjMgrYamlParser::ProjMgrYamlParser(void) {
  // Reserved
}

ProjMgrYamlParser::~ProjMgrYamlParser(void) {
  // Reserved
}

bool ProjMgrYamlParser::ParseCsolution(const string& input, CsolutionItem& csolution) {
  try {
    const YAML::Node& root = YAML::LoadFile(input);
    if (!ValidateCsolution(input, root)) {
      return false;
    }

    const YAML::Node& solutionNode = root[YAML_SOLUTION];
    if (!ParseContexts(solutionNode, csolution)) {
      return false;
    }

    ParseTargetTypes(root, csolution.targetTypes);
    ParseBuildTypes(root, csolution.buildTypes);

  } catch (YAML::Exception& e) {
    ProjMgrLogger::Error(input, e.mark.line + 1, e.mark.column + 1, e.msg);
    return false;
  }
  return true;
}

bool ProjMgrYamlParser::ParseCproject(const string& input, CsolutionItem& csolution, map<string, CprojectItem>& cprojects, bool single) {
  CprojectItem cproject;
  try {
    const YAML::Node& root = YAML::LoadFile(input);
    if (!ValidateCproject(input, root)) {
      return false;
    }

    const YAML::Node& projectNode = root[YAML_PROJECT];
    map<const string, string&> projectChildren = {
      {YAML_NAME, cproject.name},
      {YAML_DESCRIPTION, cproject.description},
      {YAML_OUTPUTTYPE, cproject.outputType},
    };
    for (const auto& item : projectChildren) {
      ParseString(projectNode, item.first, item.second);
    }
    ParseTargetType(projectNode, cproject.target);
    ParsePackages(projectNode, cproject.packages);

    if (!ParseRteDirs(projectNode, cproject.rteDirs)) {
      return false;
    }
    if (!ParseComponents(projectNode, cproject.components)) {
      return false;
    }
    if (!ParseGroups(projectNode, cproject.groups)) {
      return false;
    }
    if (!ParseLayers(projectNode, cproject.clayers)) {
      return false;
    }

  } catch (YAML::Exception& e) {
    ProjMgrLogger::Error(input, e.mark.line + 1, e.mark.column + 1, e.msg);
    return false;
  }

  cprojects[input] = cproject;

  if (single) {
    csolution.contexts.push_back({ input });
  }
  return true;
}

bool ProjMgrYamlParser::ParseClayer(const string& input, map<string, ClayerItem>& clayers) {
  ClayerItem clayer;
  try {
    const YAML::Node& root = YAML::LoadFile(input);
    if (!ValidateClayer(input, root)) {
      return false;
    }

    const YAML::Node& layerNode = root[YAML_LAYER];
    map<const string, string&> projectChildren = {
      {YAML_NAME, clayer.name},
      {YAML_DESCRIPTION, clayer.description},
      {YAML_OUTPUTTYPE, clayer.outputType},
    };
    for (const auto& item : projectChildren) {
      ParseString(layerNode, item.first, item.second);
    }

    ParseTargetType(layerNode, clayer.target);
    ParsePackages(layerNode, clayer.packages);

    if (!ParseRteDirs(layerNode, clayer.rteDirs)) {
      return false;
    }
    if (!ParseComponents(layerNode, clayer.components)) {
      return false;
    }
    if (!ParseGroups(layerNode, clayer.groups)) {
      return false;
    }
    //TODO: Parse Interfaces

  }
  catch (YAML::Exception& e) {
    ProjMgrLogger::Error(input, e.mark.line + 1, e.mark.column + 1, e.msg);
    return false;
  }
  clayers[input] = clayer;
  return true;
}

bool ProjMgrYamlParser::ParseRteDirs(const YAML::Node& parent, std::vector<RteDirItem>& dirs) {
  if (parent[YAML_RTEDIRS].IsDefined()) {
    const YAML::Node& dirsNode = parent[YAML_RTEDIRS];
    for (const auto& dirEntry : dirsNode) {
      RteDirItem dirItem;
      if (!ParseTypeFilter(dirEntry, dirItem.type)) {
        return false;
      }
      ParseString(dirEntry, YAML_RTEDIR, dirItem.dir);
      dirs.push_back(dirItem);
    }
  }
  return true;
}

void ProjMgrYamlParser::ParsePackages(const YAML::Node& parent, vector<string>& packages) {
  if (parent[YAML_PACKAGES].IsDefined()) {
    const YAML::Node& packagesNode = parent[YAML_PACKAGES];
    for (const auto& packagesEntry : packagesNode) {
      string packageItem;
      ParseString(packagesEntry, YAML_PACKAGE, packageItem);
      packages.push_back(packageItem);
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

bool ProjMgrYamlParser::ParseComponents(const YAML::Node& parent, vector<ComponentItem>& components) {
  if (parent[YAML_COMPONENTS].IsDefined()) {
    const YAML::Node& componentsNode = parent[YAML_COMPONENTS];
    for (const auto& componentEntry : componentsNode) {
      ComponentItem componentItem;
      if (!ParseTypeFilter(componentEntry, componentItem.type)) {
        return false;
      }
      ParseString(componentEntry, YAML_COMPONENT, componentItem.component);
      ParseBuildType(componentEntry, componentItem.build);
      components.push_back(componentItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseTypeFilter(const YAML::Node& parent, TypeFilter& type) {
  vector<string> include, exclude;
  ParseVectorOrString(parent, YAML_FORTYPE, include);
  ParseVectorOrString(parent, YAML_NOTFORTYPE, exclude);
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
      ParseString(miscEntry, YAML_MISC_COMPILER, miscItem.compiler);
      ParseVector(miscEntry, YAML_MISC_C, miscItem.c);
      ParseVector(miscEntry, YAML_MISC_CPP, miscItem.cpp);
      ParseVector(miscEntry, YAML_MISC_C_CPP, miscItem.c_cpp);
      ParseVector(miscEntry, YAML_MISC_ASM, miscItem.as);
      ParseVector(miscEntry, YAML_MISC_LINK, miscItem.link);
      ParseVector(miscEntry, YAML_MISC_LIB, miscItem.lib);
      misc.push_back(miscItem);
    }
  }
}

bool ProjMgrYamlParser::ParseFiles(const YAML::Node& parent, vector<FileNode>& files) {
  if (parent[YAML_FILES].IsDefined()) {
    const YAML::Node& filesNode = parent[YAML_FILES];
    for (const auto& fileEntry : filesNode) {
      FileNode fileItem;
      if (!ParseTypeFilter(fileEntry, fileItem.type)) {
        return false;
      }
      ParseString(fileEntry, YAML_FILE, fileItem.file);
      ParseString(fileEntry, YAML_CATEGORY, fileItem.category);
      ParseBuildType(fileEntry, fileItem.build);
      files.push_back(fileItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseGroups(const YAML::Node& parent, vector<GroupNode>& groups) {
  if (parent[YAML_GROUPS].IsDefined()) {
    const YAML::Node& groupsNode = parent[YAML_GROUPS];
    for (const auto& groupEntry : groupsNode) {
      GroupNode groupItem;
      if (!ParseTypeFilter(groupEntry, groupItem.type)) {
        return false;
      }
      if (!ParseFiles(groupEntry, groupItem.files)) {
        return false;
      }
      ParseString(groupEntry, YAML_GROUP, groupItem.group);
      ParseBuildType(groupEntry, groupItem.build);
      ParseGroups(groupEntry, groupItem.groups);
      groups.push_back(groupItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseLayers(const YAML::Node& parent, vector<LayerItem>& layers) {
  if (parent[YAML_LAYERS].IsDefined()) {
    const YAML::Node& layersNode = parent[YAML_LAYERS];
    for (const auto& layerEntry : layersNode) {
      LayerItem layerItem;
      if (!ParseTypeFilter(layerEntry, layerItem.type)) {
        return false;
      }
      ParseString(layerEntry, YAML_LAYER, layerItem.layer);
      ParseBuildType(layerEntry, layerItem.build);
      layers.push_back(layerItem);
    }
  }
  return true;
}

bool ProjMgrYamlParser::ParseContexts(const YAML::Node& parent, CsolutionItem& csolution) {
  for (const auto& projectsEntry : parent) {
    ContextDesc descriptor;
    if (!ParseTypeFilter(projectsEntry, descriptor.type)) {
      return false;
    }
    ParseString(projectsEntry, YAML_PROJECT, descriptor.cproject);
    ParseVectorOrString(projectsEntry, YAML_DEPENDS, descriptor.depends);
    ParseTargetType(projectsEntry, descriptor.target);
    csolution.contexts.push_back(descriptor);
    PushBackUniquely(csolution.cprojects, descriptor.cproject);
  }
  return true;
}

void ProjMgrYamlParser::ParseBuildTypes(const YAML::Node& parent, map<string, BuildType>& buildTypes) {
  if (parent[YAML_BUILDTYPES].IsDefined()) {
    const YAML::Node& buildTypesNode = parent[YAML_BUILDTYPES];
    for (const auto& typeEntry : buildTypesNode) {
      string typeItem;
      BuildType build;
      ParseString(typeEntry, YAML_TYPE, typeItem);
      ParseBuildType(typeEntry, build);
      buildTypes[typeItem] = build;
    }
  }
}

void ProjMgrYamlParser::ParseTargetTypes(const YAML::Node& parent, map<string, TargetType>& targetTypes) {
  if (parent[YAML_TARGETTYPES].IsDefined()) {
    const YAML::Node& targetTypesNode = parent[YAML_TARGETTYPES];
    for (const auto& typeEntry : targetTypesNode) {
      string typeItem;
      TargetType target;
      ParseString(typeEntry, YAML_TYPE, typeItem);
      ParseTargetType(typeEntry, target);
      targetTypes[typeItem] = target;
    }
  }
}

void ProjMgrYamlParser::ParseBuildType(const YAML::Node& parent, BuildType& buildType) {
  map<const string, string&> buildChildren = {
    {YAML_COMPILER, buildType.compiler},
    {YAML_OPTIMIZE, buildType.optimize},
    {YAML_DEBUG, buildType.debug},
    {YAML_WARNINGS, buildType.debug},
  };
  for (const auto& item : buildChildren) {
    ParseString(parent, item.first, item.second);
  }
  ParseProcessor(parent, buildType.processor);
  ParseMisc(parent, buildType.misc);
  ParseVector(parent, YAML_DEFINES, buildType.defines);
  ParseVector(parent, YAML_UNDEFINES, buildType.undefines);
  ParseVector(parent, YAML_ADDPATHS, buildType.addpaths);
  ParseVector(parent, YAML_DELPATHS, buildType.delpaths);
}

void ProjMgrYamlParser::ParseTargetType(const YAML::Node& parent, TargetType& targetType) {
  map<const string, string&> targetChildren = {
    {YAML_BOARD, targetType.board},
    {YAML_DEVICE, targetType.device},
  };
  for (const auto& item : targetChildren) {
    ParseString(parent, item.first, item.second);
  }
  ParseBuildType(parent, targetType.build);
}

void ProjMgrYamlParser::PushBackUniquely(vector<string>& vec, const string& value) {
  if (find(vec.cbegin(), vec.cend(), value) == vec.cend()) {
    vec.push_back(value);
  }
}

// Validation Maps
const set<string> solutionKeys = {
  YAML_PROJECT,
  YAML_FORTYPE,
  YAML_NOTFORTYPE,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_UNDEFINES,
  YAML_ADDPATHS,
  YAML_DELPATHS,
  YAML_MISC,
  YAML_DEPENDS,
};

const set<string> projectKeys = {
  YAML_NAME,
  YAML_DESCRIPTION,
  YAML_OUTPUTTYPE,
  YAML_DEVICE,
  YAML_BOARD,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_RTEDIRS,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_UNDEFINES,
  YAML_ADDPATHS,
  YAML_DELPATHS,
  YAML_MISC,
  YAML_PACKAGES,
  YAML_COMPONENTS,
  YAML_GROUPS,
  YAML_LAYERS,
};

const set<string> layerKeys = {
  YAML_NAME,
  YAML_DESCRIPTION,
  YAML_OUTPUTTYPE,
  YAML_DEVICE,
  YAML_BOARD,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_RTEDIRS,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_UNDEFINES,
  YAML_ADDPATHS,
  YAML_DELPATHS,
  YAML_MISC,
  YAML_PACKAGES,
  YAML_COMPONENTS,
  YAML_GROUPS,
  YAML_INTERFACES,
};

const set<string> targetTypeKeys = {
  YAML_TYPE,
  YAML_DEVICE,
  YAML_BOARD,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_UNDEFINES,
  YAML_ADDPATHS,
  YAML_DELPATHS,
  YAML_MISC,
};

const set<string> buildTypeKeys = {
  YAML_TYPE,
  YAML_PROCESSOR,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_UNDEFINES,
  YAML_ADDPATHS,
  YAML_DELPATHS,
  YAML_MISC,
};

const set<string> processorKeys = {
  YAML_TRUSTZONE,
  YAML_FPU,
  YAML_ENDIAN,
};

const set<string> dirsKeys = {
  YAML_RTEDIR,
  YAML_FORTYPE,
};

const set<string> miscKeys = {
  YAML_MISC_COMPILER,
  YAML_MISC_C,
  YAML_MISC_CPP,
  YAML_MISC_C_CPP,
  YAML_MISC_ASM,
  YAML_MISC_LINK,
  YAML_MISC_LIB,
};

const set<string> packagesKeys = {
  YAML_PACKAGE,
};

const set<string> componentsKeys = {
  YAML_COMPONENT,
  YAML_FORTYPE,
  YAML_NOTFORTYPE,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_UNDEFINES,
  YAML_ADDPATHS,
  YAML_DELPATHS,
  YAML_MISC,
};

const set<string> layersKeys = {
  YAML_LAYER,
  YAML_FORTYPE,
  YAML_NOTFORTYPE,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_UNDEFINES,
  YAML_ADDPATHS,
  YAML_DELPATHS,
  YAML_MISC,
};

const set<string> interfacesKeys = {
  YAML_PROVIDES,
  YAML_CONSUMES
};

const set<string> groupsKeys = {
  YAML_GROUP,
  YAML_FORTYPE,
  YAML_NOTFORTYPE,
  YAML_GROUPS,
  YAML_FILES,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_UNDEFINES,
  YAML_ADDPATHS,
  YAML_DELPATHS,
  YAML_MISC,
};

const set<string> filesKeys = {
  YAML_FILE,
  YAML_FORTYPE,
  YAML_NOTFORTYPE,
  YAML_CATEGORY,
  YAML_COMPILER,
  YAML_OPTIMIZE,
  YAML_DEBUG,
  YAML_WARNINGS,
  YAML_DEFINES,
  YAML_UNDEFINES,
  YAML_ADDPATHS,
  YAML_DELPATHS,
  YAML_MISC,
};

const map<string, set<string>> sequences = {
  {YAML_SOLUTION, solutionKeys},
  {YAML_TARGETTYPES, targetTypeKeys},
  {YAML_BUILDTYPES, buildTypeKeys},
  {YAML_MISC, miscKeys},
  {YAML_PACKAGES, packagesKeys},
  {YAML_COMPONENTS, componentsKeys},
  {YAML_LAYERS, layersKeys},
  {YAML_GROUPS, groupsKeys},
  {YAML_FILES, filesKeys},
  {YAML_INTERFACES, interfacesKeys},
};

const map<string, set<string>> mappings = {
  {YAML_PROCESSOR, processorKeys},
};

bool ProjMgrYamlParser::ValidateCsolution(const string& input, const YAML::Node& root) {
  const set<string> rootKeys = {
    YAML_SOLUTION,
    YAML_TARGETTYPES,
    YAML_BUILDTYPES,
  };
  return ValidateKeys(input, root, rootKeys);
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
