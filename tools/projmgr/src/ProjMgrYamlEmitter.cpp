/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrLogger.h"
#include "ProjMgrYamlEmitter.h"
#include "ProjMgrYamlParser.h"
#include "ProjMgrUtils.h"
#include "RteFsUtils.h"
#include "RteItem.h"

#include <filesystem>
#include <fstream>

using namespace std;

ProjMgrYamlEmitter::ProjMgrYamlEmitter(void) {
  // Reserved
}

ProjMgrYamlEmitter::~ProjMgrYamlEmitter(void) {
  // Reserved
}

optional<string> ProjMgrYamlEmitter::EmitContextInfo(const ContextItem& context, const string& destinationPath) {
  typedef map <string, string> StringMap;
  typedef vector<StringMap> StringMapVector;
  
  StringMap infoMap;
  const StringMap origin = {
    {"solution",    context.csolution->path},
    {"project",     context.cproject->path},
    {"build-type",  context.type.build},
    {"target-type", context.type.target},
    {"board",       context.board},
    {"device",      context.device}
  };
  for (const auto& [key, value] : origin) {
    if (!value.empty()) {
      infoMap.insert({ key, value });
    }
  }

  StringMapVector componentVector;
  const string& packRoot = ProjMgrKernel::Get()->GetCmsisPackRoot();
  for (const auto& [componentId, component] : context.components) {
    componentVector.push_back({
      {"component", componentId},
      {"pack", ProjMgrUtils::GetPackageID(component.first->GetPackage())},
      {"pack-path", fs::absolute(packRoot + "/" + component.first->GetPackagePath()).generic_string()},
    });
  }

  YAML::Node rootNode;
  rootNode["context"] = infoMap;
  rootNode["context"]["components"] = componentVector;
  rootNode["destination"] = destinationPath;
  YAML::Emitter emitter;
  emitter << rootNode;

  // Calculate output generator input file path
  string generatorTmpWorkingDir = context.directories.outdir;  // Use out build folder by default since the generator input file is a temporary file
  if (!generatorTmpWorkingDir.empty()) {
    // Outdir may be relative, if so, add project path to it
    if (fs::path(generatorTmpWorkingDir).is_relative()) {
      string projectPath = context.rteActiveProject->GetProjectPath();
      generatorTmpWorkingDir = context.rteActiveProject->GetProjectPath() + generatorTmpWorkingDir;
    }
  } else {
    generatorTmpWorkingDir = destinationPath;
  }
  if (!generatorTmpWorkingDir.empty() && generatorTmpWorkingDir.back() != '/') {
    generatorTmpWorkingDir += '/';
  }
  const string filePath = generatorTmpWorkingDir + context.name + ".generate.yml";

  // Make sure the folders exist
  try {
    if (!fs::exists(generatorTmpWorkingDir)) {
      fs::create_directories(generatorTmpWorkingDir);
    }
    if (!fs::exists(destinationPath)) {
      fs::create_directories(destinationPath);
    }
  } catch (const fs::filesystem_error& e) {
    ProjMgrLogger::Error("Failed to create folders for the generator input file '" + filePath + "': " + e.what());
    return {};
  }

  ofstream fileStream(filePath);
  if (!fileStream) {
    ProjMgrLogger::Error("Failed to create generator input file '" + filePath + "'");
    return {};
  }

  fileStream << emitter.c_str();
  fileStream << flush;
  if (!fileStream) {
    ProjMgrLogger::Error("Failed to write generator input file '" + filePath + "'");
    return {};
  }
  fileStream.close();
  return filePath;
}

// the ProjMgrYamlCbuild class encapsulates the use of the YAML library avoiding propagating its header files
class ProjMgrYamlCbuild {
private:
  friend class ProjMgrYamlEmitter;
  ProjMgrYamlCbuild(YAML::Node node, const vector<ContextItem*> processedContexts, ProjMgrParser& parser, const string& directory);
  ProjMgrYamlCbuild(YAML::Node node, const ContextItem* context);
  void SetContextNode(YAML::Node node, const ContextItem* context);
  void SetComponentsNode(YAML::Node node, const ContextItem* context);
  void SetComponentFilesNode(YAML::Node node, const ContextItem* context, const string& componentId);
  void SetPacksNode(YAML::Node node, const ContextItem* context);
  void SetGroupsNode(YAML::Node node, const vector<GroupNode>& groups);
  void SetFilesNode(YAML::Node node, const vector<FileNode>& files);
  void SetMiscNode(YAML::Node miscNode, const MiscItem& misc);
  void SetMiscNode(YAML::Node miscNode, const vector<MiscItem>& misc);
  void SetNodeValue(YAML::Node node, const string& value);
  void SetNodeValue(YAML::Node node, const vector<string>& vec);
  const string FormatPath(const string& original, const string& directory);
};

ProjMgrYamlCbuild::ProjMgrYamlCbuild(YAML::Node node, const vector<ContextItem*> processedContexts, ProjMgrParser& parser, const string& directory) {
  error_code ec;
  if (&parser.GetCdefault()) {
    const string& cdefaultFilename = fs::relative(parser.GetCdefault().path, directory, ec).generic_string();
    SetNodeValue(node[YAML_CDEFAULT], cdefaultFilename);
  }
  const string& csolutionFilename = fs::relative(parser.GetCsolution().path, directory, ec).generic_string();
  SetNodeValue(node[YAML_CSOLUTION], csolutionFilename);

  for (const auto& [cprojectName, cproject] : parser.GetCprojects()) {
    YAML::Node cprojectNode;
    const string& cprojectFilename = fs::relative(cproject.path, directory, ec).generic_string();
    SetNodeValue(cprojectNode[YAML_CPROJECT], cprojectFilename);
    node[YAML_CPROJECTS].push_back(cprojectNode);
  }

  for (const auto& [clayerName, clayer] : parser.GetClayers()) {
    YAML::Node clayerNode;
    const string& clayerFilename = fs::relative(clayer.path, directory, ec).generic_string();
    SetNodeValue(clayerNode[YAML_CLAYER], clayerFilename);
    node[YAML_CLAYERS].push_back(clayerNode);
  }

  for (const auto& context : processedContexts) {
    if (context) {
      error_code ec;
      YAML::Node cbuildNode;
      const string& filename = context->directories.cprj + "/" + context->name + ".cbuild.yml";
      const string& relativeFilename = fs::relative(filename, directory, ec).generic_string();
      SetNodeValue(cbuildNode[YAML_CBUILD], relativeFilename);
      node[YAML_CBUILDS].push_back(cbuildNode);
    }
  }
}

ProjMgrYamlCbuild::ProjMgrYamlCbuild(YAML::Node node, const ContextItem* context) {
  if (context) {
    SetContextNode(node, context);
  }
}

void ProjMgrYamlCbuild::SetContextNode(YAML::Node contextNode, const ContextItem* context) {
  SetNodeValue(contextNode[YAML_CONTEXT], context->name);
  SetNodeValue(contextNode[YAML_COMPILER], context->compiler);
  SetNodeValue(contextNode[YAML_BOARD], context->board);
  SetNodeValue(contextNode[YAML_DEVICE], context->device);
  SetPacksNode(contextNode[YAML_PACKS], context);
  SetMiscNode(contextNode[YAML_MISC], context->misc);
  SetNodeValue(contextNode[YAML_DEFINE], context->defines);
  vector<string> defines = context->defines;
  for (const auto& define : context->rteActiveTarget->GetDefines()) {
    ProjMgrUtils::PushBackUniquely(defines, define);
  }
  SetNodeValue(contextNode[YAML_DEFINE], defines);
  vector<string> includes = context->includes;
  for (auto include : context->rteActiveTarget->GetIncludePaths()) {
    RteFsUtils::NormalizePath(include, context->cproject->directory);
    ProjMgrUtils::PushBackUniquely(includes, FormatPath(include, context->directories.cprj));
  }
  SetNodeValue(contextNode[YAML_ADDPATH], includes);
  SetComponentsNode(contextNode[YAML_COMPONENTS], context);
  SetGroupsNode(contextNode[YAML_GROUPS], context->groups);
}

void ProjMgrYamlCbuild::SetComponentsNode(YAML::Node node, const ContextItem* context) {
  for (const auto& [componentId, component] : context->components) {
    const RteComponent* rteComponent = component.first;
    const ComponentItem* componentItem = component.second;
    YAML::Node componentNode;
    SetNodeValue(componentNode[YAML_COMPONENT], componentId);
    SetNodeValue(componentNode[YAML_CONDITION], rteComponent->GetConditionID());
    SetNodeValue(componentNode[YAML_FROM_PACK], ProjMgrUtils::GetPackageID(rteComponent->GetPackage()));
    SetNodeValue(componentNode[YAML_SELECTED_BY], componentItem->component);
    SetMiscNode(componentNode[YAML_MISC], componentItem->build.misc);
    SetNodeValue(componentNode[YAML_DEFINE], componentItem->build.defines);
    SetNodeValue(componentNode[YAML_UNDEFINE], componentItem->build.undefines);
    SetNodeValue(componentNode[YAML_ADDPATH], componentItem->build.addpaths);
    SetNodeValue(componentNode[YAML_DELPATH], componentItem->build.delpaths);
    SetComponentFilesNode(componentNode[YAML_FILES], context, componentId);
    node.push_back(componentNode);
  }
}

void ProjMgrYamlCbuild::SetComponentFilesNode(YAML::Node node, const ContextItem* context, const string& componentId) {
  if (context->componentFiles.find(componentId) != context->componentFiles.end()) {
    context->components.at(componentId).first->GetRteFolder();
    for (const auto& file : context->componentFiles.at(componentId)) {
      YAML::Node fileNode;
      SetNodeValue(fileNode[YAML_FILE], FormatPath(file, context->directories.cprj));
      SetNodeValue(fileNode[YAML_CATEGORY], ProjMgrUtils::GetCategory(file));
      node.push_back(fileNode);
    }
  }
}

void ProjMgrYamlCbuild::SetPacksNode(YAML::Node node, const ContextItem* context) {
  for (const auto& [packId, package] : context->packages) {
    YAML::Node packNode;
    SetNodeValue(packNode[YAML_PACK], packId);
    const string& pdscFilename = FormatPath(package->GetPackageFileName(), context->directories.cprj);
    SetNodeValue(packNode[YAML_PATH], fs::path(pdscFilename).parent_path().generic_string());
    node.push_back(packNode);
  }
}

void ProjMgrYamlCbuild::SetGroupsNode(YAML::Node node, const vector<GroupNode>& groups) {
  for (const auto& group : groups) {
    YAML::Node groupNode;
    SetNodeValue(groupNode[YAML_GROUP], group.group);
    SetMiscNode(groupNode[YAML_MISC], group.build.misc);
    SetNodeValue(groupNode[YAML_DEFINE], group.build.defines);
    SetNodeValue(groupNode[YAML_UNDEFINE], group.build.undefines);
    SetNodeValue(groupNode[YAML_ADDPATH], group.build.addpaths);
    SetNodeValue(groupNode[YAML_DELPATH], group.build.delpaths);
    SetFilesNode(groupNode[YAML_FILES], group.files);
    SetGroupsNode(groupNode[YAML_GROUPS], group.groups);
    node.push_back(groupNode);
  }
}

void ProjMgrYamlCbuild::SetFilesNode(YAML::Node node, const vector<FileNode>& files) {
  for (const auto& file : files) {
    YAML::Node fileNode;
    SetNodeValue(fileNode[YAML_FILE], file.file);
    SetMiscNode(fileNode[YAML_MISC], file.build.misc);
    SetNodeValue(fileNode[YAML_DEFINE], file.build.defines);
    SetNodeValue(fileNode[YAML_UNDEFINE], file.build.undefines);
    SetNodeValue(fileNode[YAML_ADDPATH], file.build.addpaths);
    SetNodeValue(fileNode[YAML_DELPATH], file.build.delpaths);
    node.push_back(fileNode);
  }
}

void ProjMgrYamlCbuild::SetMiscNode(YAML::Node miscNode, const vector<MiscItem>& miscVec) {
  if (!miscVec.empty()) {
    SetMiscNode(miscNode, miscVec.front());
  }
}

void ProjMgrYamlCbuild::SetMiscNode(YAML::Node miscNode, const MiscItem& misc) {
  const map<string, vector<string>>& FLAGS_MATRIX = {
    {YAML_MISC_ASM, misc.as},
    {YAML_MISC_C, misc.c},
    {YAML_MISC_CPP, misc.cpp},
    {YAML_MISC_LINK, misc.link},
    {YAML_MISC_LIB, misc.lib}
  };
  for (const auto& [key, value] : FLAGS_MATRIX) {
    if (!value.empty()) {
      SetNodeValue(miscNode[key], value);
    }
  }
  if (miscNode.IsDefined()) {
    SetNodeValue(miscNode[YAML_FORCOMPILER], misc.compiler);
  }
}

void ProjMgrYamlCbuild::SetNodeValue(YAML::Node node, const string& value) {
  if (!value.empty()) {
    node = value;
  }
}

void ProjMgrYamlCbuild::SetNodeValue(YAML::Node node, const vector<string>& vec) {
  for (const string& value : vec) {
    if (!value.empty()) {
      node.push_back(value);
    }
  }
}

const string ProjMgrYamlCbuild::FormatPath(const string& original, const string& directory) {
  string packRoot = ProjMgrKernel::Get()->GetCmsisPackRoot();
  RteFsUtils::NormalizePath(packRoot);
  string path = original;
  RteFsUtils::NormalizePath(path);
  size_t index = path.find(packRoot);
  if (index != string::npos) {
    path.replace(index, packRoot.length(), "$CMSIS_PACK_ROOT$");
  } else {
    error_code ec;
    path = fs::relative(path, directory, ec).generic_string();
  }
  return path;
}

bool ProjMgrYamlEmitter::GenerateCbuild(ProjMgrParser& parser, const vector<ContextItem*> processedContexts, const string& outputDir) {

  // generate cbuild-idx.yml
  const string& directory = outputDir.empty() ? parser.GetCsolution().directory : RteFsUtils::AbsolutePath(outputDir).generic_string();
  const string& filename = directory + "/" + parser.GetCsolution().name + ".cbuild-idx.yml";
  ofstream fileStream(filename);
  bool error = false;
  if (!fileStream) {
    error = true;
    ProjMgrLogger::Error(filename, "file cannot be written");
  } else {
    YAML::Node rootNode;
    ProjMgrYamlCbuild cbuild(rootNode[YAML_BUILD_IDX], processedContexts, parser, directory);
    YAML::Emitter emitter;
    emitter << rootNode;
    fileStream << emitter.c_str();
    fileStream << endl;
    fileStream << flush;
    fileStream.close();
    ProjMgrLogger::Info(filename, "file generated successfully");
  }

  // generate cbuild.yml for each context
  for (const auto& context : processedContexts) {
    RteFsUtils::CreateDirectories(context->directories.cprj);
    const string& filename = context->directories.cprj + "/" + context->name + ".cbuild.yml";
    ofstream fileStream(filename);
    if (!fileStream) {
      error = true;
      ProjMgrLogger::Error(filename, "file cannot be written");
      continue;
    }
    YAML::Node rootNode;
    ProjMgrYamlCbuild cbuild(rootNode[YAML_BUILD], context);
    YAML::Emitter emitter;
    emitter << rootNode;
    fileStream << emitter.c_str();
    fileStream << endl;
    fileStream << flush;
    fileStream.close();
    ProjMgrLogger::Info(filename, "file generated successfully");
  }
  if (error) {
    return false;
  }
  return true;
}
