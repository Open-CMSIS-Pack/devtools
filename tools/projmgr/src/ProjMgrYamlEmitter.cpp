/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProductInfo.h"
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

// the ProjMgrYamlCbuild class encapsulates the use of the YAML library avoiding propagating its header files
class ProjMgrYamlCbuild {
private:
  friend class ProjMgrYamlEmitter;
  ProjMgrYamlCbuild(YAML::Node node, const vector<ContextItem*> processedContexts, ProjMgrParser& parser, const string& directory);
  ProjMgrYamlCbuild(YAML::Node node, const ContextItem* context);
  void SetContextNode(YAML::Node node, const ContextItem* context);
  void SetComponentsNode(YAML::Node node, const ContextItem* context);
  void SetComponentFilesNode(YAML::Node node, const ContextItem* context, const string& componentId);
  void SetGeneratorsNode(YAML::Node node, const ContextItem* context);
  void SetGeneratorFiles(YAML::Node node, const ContextItem* context, const string& componentId);
  void SetConstructedFilesNode(YAML::Node node, const ContextItem* context);
  void SetOutputDirsNode(YAML::Node node, const ContextItem* context);
  void SetOutputNode(YAML::Node node, const ContextItem* context);
  void SetPacksNode(YAML::Node node, const ContextItem* context);
  void SetGroupsNode(YAML::Node node, const vector<GroupNode>& groups);
  void SetFilesNode(YAML::Node node, const vector<FileNode>& files);
  void SetLinkerNode(YAML::Node node, const ContextItem* context);
  void SetControlsNode(YAML::Node Node, const BuildType& controls);
  void SetProcessorNode(YAML::Node node, const map<string, string>& targetAttributes);
  void SetMiscNode(YAML::Node miscNode, const MiscItem& misc);
  void SetMiscNode(YAML::Node miscNode, const vector<MiscItem>& misc);
  void SetNodeValue(YAML::Node node, const string& value);
  void SetNodeValue(YAML::Node node, const vector<string>& vec);
  const string FormatPath(const string& original, const string& directory);
  bool CompareFile(const string& filename, const YAML::Node& rootNode);
  bool CompareNodes(const YAML::Node& lhs, const YAML::Node& rhs);
};

ProjMgrYamlCbuild::ProjMgrYamlCbuild(YAML::Node node, const vector<ContextItem*> processedContexts, ProjMgrParser& parser, const string& directory) {
  error_code ec;
  SetNodeValue(node[YAML_GENERATED_BY], ORIGINAL_FILENAME + string(" version ") + VERSION_STRING);

  if (!parser.GetCdefault().path.empty()) {
    SetNodeValue(node[YAML_CDEFAULT], FormatPath(parser.GetCdefault().path, directory));
  }
  SetNodeValue(node[YAML_CSOLUTION], FormatPath(parser.GetCsolution().path, directory));

  const auto& cprojects = parser.GetCprojects();
  const auto& csolution = parser.GetCsolution();

  for (const auto& cprojectFile : csolution.cprojects) {
    auto itr = std::find_if(cprojects.begin(), cprojects.end(),
      [&](const pair<std::string, CprojectItem>& elem) {
        return (elem.first.find(fs::path(cprojectFile).filename().string()) != std::string::npos);
      });
    auto cproject = itr->second;
    YAML::Node cprojectNode;
    const string& cprojectFilename = fs::relative(cproject.path, directory, ec).generic_string();
    SetNodeValue(cprojectNode[YAML_CPROJECT], cprojectFilename);
    for (const auto& item : cproject.clayers) {
      string clayerPath = item.layer;
      RteFsUtils::NormalizePath(clayerPath, cproject.directory);
      const string& clayerFilename = fs::relative(clayerPath, directory, ec).generic_string();
      YAML::Node clayerNode;
      SetNodeValue(clayerNode[YAML_CLAYER], clayerFilename);
      cprojectNode[YAML_CLAYERS].push_back(clayerNode);
    }
    node[YAML_CPROJECTS].push_back(cprojectNode);
  }

  for (const auto& context : processedContexts) {
    if (context) {
      error_code ec;
      YAML::Node cbuildNode;
      const string& filename = context->directories.cprj + "/" + context->name + ".cbuild.yml";
      const string& relativeFilename = fs::relative(filename, directory, ec).generic_string();
      SetNodeValue(cbuildNode[YAML_CBUILD], relativeFilename);
      if (context->cproject) {
        SetNodeValue(cbuildNode[YAML_PROJECT], context->cproject->name);
        SetNodeValue(cbuildNode[YAML_CONFIGURATION],
          (context->type.build.empty() ? "" : ("." + context->type.build)) +
          "+" + context->type.target);
      }
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
  SetNodeValue(contextNode[YAML_GENERATED_BY], ORIGINAL_FILENAME + string(" version ") + VERSION_STRING);
  SetNodeValue(contextNode[YAML_SOLUTION], FormatPath(context->csolution->path, context->directories.cprj));
  SetNodeValue(contextNode[YAML_PROJECT], FormatPath(context->cproject->path, context->directories.cprj));
  SetNodeValue(contextNode[YAML_CONTEXT], context->name);
  SetNodeValue(contextNode[YAML_COMPILER], context->compiler);
  SetNodeValue(contextNode[YAML_BOARD], context->board);
  SetNodeValue(contextNode[YAML_DEVICE], context->device);
  SetProcessorNode(contextNode[YAML_PROCESSOR], context->targetAttributes);
  SetPacksNode(contextNode[YAML_PACKS], context);
  SetControlsNode(contextNode, context->controls.processed);
  SetOutputDirsNode(contextNode[YAML_OUTPUTDIRS], context);
  SetOutputNode(contextNode[YAML_OUTPUT], context);
  vector<string> defines;
  for (const auto& define : context->rteActiveTarget->GetDefines()) {
    ProjMgrUtils::PushBackUniquely(defines, define);
  }
  SetNodeValue(contextNode[YAML_DEFINE], defines);
  vector<string> includes;
  for (auto include : context->rteActiveTarget->GetIncludePaths()) {
    RteFsUtils::NormalizePath(include, context->cproject->directory);
    ProjMgrUtils::PushBackUniquely(includes, FormatPath(include, context->directories.cprj));
  }
  SetNodeValue(contextNode[YAML_ADDPATH], includes);
  SetComponentsNode(contextNode[YAML_COMPONENTS], context);
  SetGeneratorsNode(contextNode[YAML_GENERATORS], context);
  SetLinkerNode(contextNode[YAML_LINKER], context);
  SetGroupsNode(contextNode[YAML_GROUPS], context->groups);
  SetConstructedFilesNode(contextNode[YAML_CONSTRUCTEDFILES], context);
}

void ProjMgrYamlCbuild::SetComponentsNode(YAML::Node node, const ContextItem* context) {
  for (const auto& [componentId, component] : context->components) {
    const RteComponent* rteComponent = component.instance->GetParent()->GetComponent();
    const ComponentItem* componentItem = component.item;
    YAML::Node componentNode;
    SetNodeValue(componentNode[YAML_COMPONENT], componentId);
    SetNodeValue(componentNode[YAML_CONDITION], rteComponent->GetConditionID());
    SetNodeValue(componentNode[YAML_FROM_PACK], ProjMgrUtils::GetPackageID(rteComponent->GetPackage()));
    SetNodeValue(componentNode[YAML_SELECTED_BY], componentItem->component);
    const string& rteDir = rteComponent->GetAttribute("rtedir");
    if (!rteDir.empty()) {
      SetNodeValue(componentNode[YAML_OUTPUT_RTEDIR], FormatPath(context->cproject->directory + "/" + rteDir, context->directories.cprj));
    }
    SetControlsNode(componentNode, componentItem->build);
    SetComponentFilesNode(componentNode[YAML_FILES], context, componentId);
    SetNodeValue(componentNode[YAML_GENERATOR][YAML_ID], component.generator);
    SetGeneratorFiles(componentNode[YAML_GENERATOR], context, componentId);
    node.push_back(componentNode);
  }
}

void ProjMgrYamlCbuild::SetComponentFilesNode(YAML::Node node, const ContextItem* context, const string& componentId) {
  if (context->componentFiles.find(componentId) != context->componentFiles.end()) {
    for (const auto& [file, attr, category, version] : context->componentFiles.at(componentId)) {
      YAML::Node fileNode;
      SetNodeValue(fileNode[YAML_FILE], FormatPath(file, context->directories.cprj));
      SetNodeValue(fileNode[YAML_CATEGORY], category);
      SetNodeValue(fileNode[YAML_ATTR], attr);
      SetNodeValue(fileNode[YAML_VERSION], version);
      node.push_back(fileNode);
    }
  }
}

void ProjMgrYamlCbuild::SetGeneratorFiles(YAML::Node node, const ContextItem* context, const string& componentId) {
  if (context->generatorInputFiles.find(componentId) != context->generatorInputFiles.end()) {
    YAML::Node filesNode;
    for (const auto& [file, attr, category, version] : context->generatorInputFiles.at(componentId)) {
      YAML::Node fileNode;
      SetNodeValue(fileNode[YAML_FILE], FormatPath(file, context->directories.cprj));
      SetNodeValue(fileNode[YAML_CATEGORY], category);
      SetNodeValue(fileNode[YAML_ATTR], attr);
      SetNodeValue(fileNode[YAML_VERSION], version);
      filesNode.push_back(fileNode);
    }
    node[YAML_FILES] = filesNode;
  }
}

void ProjMgrYamlCbuild::SetGeneratorsNode(YAML::Node node, const ContextItem* context) {
  for (const auto& [generatorId, generator] : context->generators) {
    YAML::Node genNode;
    const string& workingDir = generator->GetExpandedWorkingDir(context->rteActiveTarget);
    SetNodeValue(genNode[YAML_WORKING_DIR], FormatPath(workingDir, context->directories.cprj));
    const string& gpdscFile = generator->GetExpandedGpdsc(context->rteActiveTarget);
    SetNodeValue(genNode[YAML_GPDSC], FormatPath(gpdscFile, context->directories.cprj));

    for (const string host : {"win", "linux", "mac", "other"}) {
      YAML::Node commandNode;

      // Executable file
      const string exe = generator->GetExecutable(context->rteActiveTarget, host);
      if (exe.empty())
        continue;
      commandNode[YAML_FILE] = FormatPath(exe, context->directories.cprj);

      // Arguments
      YAML::Node argumentsNode;
      const vector<pair<string, string> >& args = generator->GetExpandedArguments(context->rteActiveTarget, host);
      for (auto [swtch, value] : args) {
        // If the argument is recognized as an absolute path, make sure to reformat
        // it to use CMSIS_PACK_ROOT or to be relative the working directory
        if (!value.empty() && fs::path(value).is_absolute())
          value = FormatPath(value, workingDir);

        argumentsNode.push_back(swtch + value);
      }
      commandNode[YAML_ARGUMENTS] = argumentsNode;
      genNode[YAML_COMMAND][host] = commandNode;
    }

    node[generatorId] = genNode;
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
    SetControlsNode(groupNode, group.build);
    SetFilesNode(groupNode[YAML_FILES], group.files);
    SetGroupsNode(groupNode[YAML_GROUPS], group.groups);
    node.push_back(groupNode);
  }
}

void ProjMgrYamlCbuild::SetFilesNode(YAML::Node node, const vector<FileNode>& files) {
  for (const auto& file : files) {
    YAML::Node fileNode;
    SetNodeValue(fileNode[YAML_FILE], file.file);
    SetNodeValue(fileNode[YAML_CATEGORY], ProjMgrUtils::GetCategory(file.file));
    SetControlsNode(fileNode, file.build);
    node.push_back(fileNode);
  }
}

void ProjMgrYamlCbuild::SetConstructedFilesNode(YAML::Node node, const ContextItem* context) {
  // constructed preIncludeLocal don't appear here because they come under the component they belong to
  // constructed preIncludeGlobal
  const auto& preIncludeFiles = context->rteActiveTarget->GetPreIncludeFiles();
  for (const auto& [component, fileSet] : preIncludeFiles) {
    if (!component) {
      for (const auto& file : fileSet) {
        if (file == "Pre_Include_Global.h") {
          const string& filename = context->rteActiveProject->GetProjectPath() +
            context->rteActiveProject->GetRteHeader(file, context->rteActiveTarget->GetName(), "");
          YAML::Node fileNode;
          SetNodeValue(fileNode[YAML_FILE], FormatPath(filename, context->directories.cprj));
          SetNodeValue(fileNode[YAML_CATEGORY], "preIncludeGlobal");
          node.push_back(fileNode);
        }
      }
    }
  }
  // constructed RTE_Components.h
  const auto& rteComponents = context->rteActiveProject->GetProjectPath() + 
    context->rteActiveProject->GetRteComponentsH(context->rteActiveTarget->GetName(), "");
  YAML::Node rteComponentsNode;
  SetNodeValue(rteComponentsNode[YAML_FILE], FormatPath(rteComponents, context->directories.cprj));
  SetNodeValue(rteComponentsNode[YAML_CATEGORY], "header");
  node.push_back(rteComponentsNode);
}

void ProjMgrYamlCbuild::SetOutputDirsNode(YAML::Node node, const ContextItem* context) {
  const DirectoriesItem& dirs = context->directories;
  map<const string, string> outputDirs = {
    {YAML_OUTPUT_GENDIR, dirs.gendir},
    {YAML_OUTPUT_INTDIR, dirs.intdir},
    {YAML_OUTPUT_OUTDIR, dirs.outdir},
    {YAML_OUTPUT_RTEDIR, dirs.rte},
  };
  for (const auto& [name, dirPath] : outputDirs) {
    SetNodeValue(node[name], dirPath);
  }
}

void ProjMgrYamlCbuild::SetOutputNode(YAML::Node node, const ContextItem* context) {
  const auto& types = context->outputTypes;
  const vector<tuple<bool, const string, const string>> outputTypes = {
    { types.bin.on, types.bin.filename, ProjMgrUtils::OUTPUT_TYPE_BIN },
    { types.elf.on, types.elf.filename, ProjMgrUtils::OUTPUT_TYPE_ELF },
    { types.hex.on, types.hex.filename, ProjMgrUtils::OUTPUT_TYPE_HEX },
    { types.lib.on, types.lib.filename, ProjMgrUtils::OUTPUT_TYPE_LIB },
    { types.cmse.on, types.cmse.filename, ProjMgrUtils::OUTPUT_TYPE_CMSE },
  };
  for (const auto& [on, file, type] : outputTypes) {
    if (on) {
      YAML::Node fileNode;
      SetNodeValue(fileNode[YAML_TYPE], type);
      SetNodeValue(fileNode[YAML_FILE], file);
      node.push_back(fileNode);
    }
  }
}

void ProjMgrYamlCbuild::SetLinkerNode(YAML::Node node, const ContextItem* context) {
  const string script = context->linker.script.empty() ? "" :
    FormatPath(context->directories.cprj + "/" + context->linker.script, context->directories.cprj);
  const string regions = context->linker.regions.empty() ? "" :
    FormatPath(context->directories.cprj + "/" + context->linker.regions, context->directories.cprj);
  SetNodeValue(node[YAML_SCRIPT], script);
  SetNodeValue(node[YAML_REGIONS], regions);
  SetNodeValue(node[YAML_DEFINE], context->linker.defines);
}

void ProjMgrYamlCbuild::SetControlsNode(YAML::Node node, const BuildType& controls) {
  SetNodeValue(node[YAML_OPTIMIZE], controls.optimize);
  SetNodeValue(node[YAML_DEBUG], controls.debug);
  SetNodeValue(node[YAML_WARNINGS], controls.warnings);
  SetMiscNode(node[YAML_MISC], controls.misc);
  SetNodeValue(node[YAML_DEFINE], controls.defines);
  SetNodeValue(node[YAML_UNDEFINE], controls.undefines);
  SetNodeValue(node[YAML_ADDPATH], controls.addpaths);
  SetNodeValue(node[YAML_DELPATH], controls.delpaths);
}

void ProjMgrYamlCbuild::SetProcessorNode(YAML::Node node, const map<string, string>& targetAttributes) {
  if (targetAttributes.find("Dfpu") != targetAttributes.end()) {
    const string& attribute = targetAttributes.at("Dfpu");
    const string& value = (attribute == "NO_FPU") ? "off" : "on";
    SetNodeValue(node[YAML_FPU], value);
  }
  if (targetAttributes.find("Dendian") != targetAttributes.end()) {
    const string& attribute = targetAttributes.at("Dendian");
    const string& value = (attribute == "Big-endian") ? "big" :
                          (attribute == "Little-endian") ? "little" : "";
    SetNodeValue(node[YAML_ENDIAN], value);
  }
  if (targetAttributes.find("Dsecure") != targetAttributes.end()) {
    const string& attribute = targetAttributes.at("Dsecure");
    const string& value = (attribute == "Secure") ? "secure" :
                          (attribute == "Non-secure") ? "non-secure" :
                          (attribute == "TZ-disabled") ? "off" : "";
    SetNodeValue(node[YAML_TRUSTZONE], value);
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
    {YAML_MISC_LINK_C, misc.link_c},
    {YAML_MISC_LINK_CPP, misc.link_cpp},
    {YAML_MISC_LIB, misc.lib},
    {YAML_MISC_LIBRARY, misc.library}
  };
  for (const auto& [key, value] : FLAGS_MATRIX) {
    if (!value.empty()) {
      SetNodeValue(miscNode[key], value);
    }
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
  string path = original;
  RteFsUtils::NormalizePath(path);
  error_code ec;
  path = fs::weakly_canonical(fs::path(path), ec).generic_string();
  size_t index = path.find(packRoot);
  if (index != string::npos) {
    path.replace(index, packRoot.length(), "${CMSIS_PACK_ROOT}");
  } else {
    string compilerRoot;
    ProjMgrUtils::GetCompilerRoot(compilerRoot);
    index = path.find(compilerRoot);
    if (!compilerRoot.empty() && index != string::npos) {
      path.replace(index, compilerRoot.length(), "${CMSIS_COMPILER_ROOT}");
    } else {
      path = fs::relative(path, directory, ec).generic_string();
    }
  }
  return path;
}

bool ProjMgrYamlCbuild::CompareFile(const string& filename, const YAML::Node& rootNode) {
  if (!RteFsUtils::Exists(filename)) {
    return false;
  }
  const YAML::Node& yamlRoot = YAML::LoadFile(filename);
  return CompareNodes(yamlRoot, rootNode);
}

bool ProjMgrYamlCbuild::CompareNodes(const YAML::Node& lhs, const YAML::Node& rhs) {
  auto eraseGenNode = [](const string& inStr) {
    size_t startIndex, endIndex;
    string outStr = inStr;
    startIndex = outStr.find(YAML_GENERATED_BY, 0);
    endIndex = outStr.find('\n', startIndex);
    if (startIndex != std::string::npos && endIndex != std::string::npos) {
      outStr = outStr.erase(startIndex, endIndex - startIndex);
    }
    return outStr;
  };

  YAML::Emitter lhsEmitter, rhsEmitter;
  string lhsData, rhsData;

  // convert nodes into string
  lhsEmitter << lhs;
  rhsEmitter << rhs;

  // remove generated-by node from the string
  lhsData = eraseGenNode(lhsEmitter.c_str());
  rhsData = eraseGenNode(rhsEmitter.c_str());

  return (lhsData == rhsData) ? true : false;
}

bool ProjMgrYamlEmitter::GenerateCbuildIndex(ProjMgrParser& parser, const vector<ContextItem*> contexts, const string& outputDir) {
  // generate cbuild-idx.yml
  const string& directory = outputDir.empty() ? parser.GetCsolution().directory : RteFsUtils::AbsolutePath(outputDir).generic_string();
  const string& filename = directory + "/" + parser.GetCsolution().name + ".cbuild-idx.yml";

  YAML::Node rootNode;
  ProjMgrYamlCbuild cbuild(rootNode[YAML_BUILD_IDX], contexts, parser, directory);

  if (!cbuild.CompareFile(filename, rootNode)) {
    ofstream fileStream(filename);
    if (!fileStream) {
      ProjMgrLogger::Error(filename, "file cannot be written");
      return false;
    }
    YAML::Emitter emitter;
    emitter << rootNode;
    fileStream << emitter.c_str();
    fileStream << endl;
    fileStream << flush;
    fileStream.close();
    ProjMgrLogger::Info(filename, "file generated successfully");
  }
  else {
    ProjMgrLogger::Info(filename, "file is already up-to-date");
  }
  return true;
}

bool ProjMgrYamlEmitter::GenerateCbuild(ContextItem* context) {
  // generate cbuild.yml for each context
  RteFsUtils::CreateDirectories(context->directories.cprj);
  const string& filename = context->directories.cprj + "/" + context->name + ".cbuild.yml";

  // Make sure $G (generator input file) is up to date
  context->rteActiveTarget->SetGeneratorInputFile(filename);

  YAML::Node rootNode;
  ProjMgrYamlCbuild cbuild(rootNode[YAML_BUILD], context);

  // Compare yaml contents
  if (!cbuild.CompareFile(filename, rootNode)) {
    ofstream fileStream(filename);
    if (!fileStream) {
      ProjMgrLogger::Error(filename, "file cannot be written");
      return false;
    }
    YAML::Emitter emitter;
    emitter << rootNode;
    fileStream << emitter.c_str();
    fileStream << endl;
    fileStream << flush;
    fileStream.close();
    ProjMgrLogger::Info(filename, "file generated successfully");
  }
  else {
    ProjMgrLogger::Info(filename, "file is already up-to-date");
  }
  return true;
}
