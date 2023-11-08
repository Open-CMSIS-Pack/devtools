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
class ProjMgrYamlBase {
protected:
  friend class ProjMgrYamlEmitter;
  ProjMgrYamlBase(bool useAbsolutePaths = false);
  void SetNodeValue(YAML::Node node, const string& value);
  void SetNodeValue(YAML::Node node, const vector<string>& vec);
  const string FormatPath(const string& original, const string& directory);
  bool CompareFile(const string& filename, const YAML::Node& rootNode);
  bool CompareNodes(const YAML::Node& lhs, const YAML::Node& rhs);
  bool WriteFile(YAML::Node& rootNode, const std::string& filename);
  const bool m_useAbsolutePaths;
};

class ProjMgrYamlCbuild : public ProjMgrYamlBase {
private:
  friend class ProjMgrYamlEmitter;
  ProjMgrYamlCbuild(YAML::Node node, const vector<ContextItem*>& processedContexts, const string& selectedCompiler);
  ProjMgrYamlCbuild(YAML::Node node, const ContextItem* context, const string& generatorId, const string& generatorPack);
  ProjMgrYamlCbuild(YAML::Node node, const vector<ContextItem*>& siblings, const string& type, const string& output, const string& gendir);
  void SetContextNode(YAML::Node node, const ContextItem* context, const string& generatorId, const string& generatorPack);
  void SetComponentsNode(YAML::Node node, const ContextItem* context);
  void SetComponentFilesNode(YAML::Node node, const ContextItem* context, const string& componentId);
  void SetGeneratorsNode(YAML::Node node, const ContextItem* context);
  void SetGeneratorFiles(YAML::Node node, const ContextItem* context, const string& componentId);
  void SetConstructedFilesNode(YAML::Node node, const ContextItem* context);
  void SetOutputDirsNode(YAML::Node node, const ContextItem* context);
  void SetOutputNode(YAML::Node node, const ContextItem* context);
  void SetPacksNode(YAML::Node node, const ContextItem* context);
  void SetGroupsNode(YAML::Node node, const ContextItem* context, const vector<GroupNode>& groups);
  void SetFilesNode(YAML::Node node, const ContextItem* context, const vector<FileNode>& files);
  void SetLinkerNode(YAML::Node node, const ContextItem* context);
  void SetLicenseInfoNode(YAML::Node node, const ContextItem* context);
  void SetControlsNode(YAML::Node Node, const ContextItem* context, const BuildType& controls);
  void SetProcessorNode(YAML::Node node, const map<string, string>& targetAttributes);
  void SetMiscNode(YAML::Node miscNode, const MiscItem& misc);
  void SetMiscNode(YAML::Node miscNode, const vector<MiscItem>& misc);
  void SetDefineNode(YAML::Node node, const vector<string>& vec);
};

class ProjMgrYamlCbuildIdx : public ProjMgrYamlBase {
private:
  friend class ProjMgrYamlEmitter;
  ProjMgrYamlCbuildIdx(YAML::Node node, const vector<ContextItem*>& processedContexts, ProjMgrParser& parser, const string& directory);
};

ProjMgrYamlBase::ProjMgrYamlBase(bool useAbsolutePaths) : m_useAbsolutePaths(useAbsolutePaths) {
}

ProjMgrYamlCbuildIdx::ProjMgrYamlCbuildIdx(YAML::Node node, const vector<ContextItem*>& processedContexts, ProjMgrParser& parser, const string& directory) :
  ProjMgrYamlBase(false)
{
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

ProjMgrYamlCbuild::ProjMgrYamlCbuild(YAML::Node node, const ContextItem* context,
  const string& generatorId, const string& generatorPack) :
  ProjMgrYamlBase(!generatorId.empty()) {
  if (context) {
    SetContextNode(node, context, generatorId, generatorPack);
  }
}

ProjMgrYamlCbuild::ProjMgrYamlCbuild(YAML::Node node,
  const vector<ContextItem*>& processedContexts, const string& selectedCompiler)
{
  vector<string> contexts;
  for (const auto& context : processedContexts) {
    if (context) {
      contexts.push_back(context->name);
    }
  }
  SetNodeValue(node[YAML_GENERATED_BY], ORIGINAL_FILENAME + string(" version ") + VERSION_STRING);
  SetNodeValue(node[YAML_CONTEXTS], contexts);
  if (!selectedCompiler.empty()) {
    SetNodeValue(node[YAML_COMPILER], selectedCompiler);
  }
}

void ProjMgrYamlCbuild::SetContextNode(YAML::Node contextNode, const ContextItem* context, const string& generatorId, const string& generatorPack) {
  SetNodeValue(contextNode[YAML_GENERATED_BY], ORIGINAL_FILENAME + string(" version ") + VERSION_STRING);
  if (!generatorId.empty()) {
    YAML::Node generatorNode = contextNode[YAML_CURRENT_GENERATOR];
    SetNodeValue(generatorNode[YAML_ID], generatorId);
    SetNodeValue(generatorNode[YAML_FROM_PACK], generatorPack);
  }
  SetNodeValue(contextNode[YAML_SOLUTION], FormatPath(context->csolution->path, context->directories.cbuild));
  SetNodeValue(contextNode[YAML_PROJECT], FormatPath(context->cproject->path, context->directories.cbuild));
  SetNodeValue(contextNode[YAML_CONTEXT], context->name);
  SetNodeValue(contextNode[YAML_COMPILER], context->compiler);
  if (!context->board.empty()) {
    SetNodeValue(contextNode[YAML_BOARD], context->board);
    SetNodeValue(contextNode[YAML_BOARD_PACK], context->boardPack->GetID());
  }
  SetNodeValue(contextNode[YAML_DEVICE], context->device);
  SetNodeValue(contextNode[YAML_DEVICE_PACK], context->devicePack->GetID());
  SetProcessorNode(contextNode[YAML_PROCESSOR], context->targetAttributes);
  SetPacksNode(contextNode[YAML_PACKS], context);
  SetControlsNode(contextNode, context, context->controls.processed);
  vector<string> defines;
  for (const auto& define : context->rteActiveTarget->GetDefines()) {
    ProjMgrUtils::PushBackUniquely(defines, define);
  }
  SetDefineNode(contextNode[YAML_DEFINE], defines);
  vector<string> includes;
  for (const auto& targetIncludes : {
    context->rteActiveTarget->GetIncludePaths(RteFile::Language::LANGUAGE_C),
    context->rteActiveTarget->GetIncludePaths(RteFile::Language::LANGUAGE_CPP),
    context->rteActiveTarget->GetIncludePaths(RteFile::Language::LANGUAGE_C_CPP),
    context->rteActiveTarget->GetIncludePaths(RteFile::Language::LANGUAGE_NONE)
    }) {
    for (auto include : targetIncludes) {
      RteFsUtils::NormalizePath(include, context->cproject->directory);
      ProjMgrUtils::PushBackUniquely(includes, FormatPath(include, context->directories.cbuild));
    }
  }
  SetNodeValue(contextNode[YAML_ADDPATH], includes);
  SetOutputDirsNode(contextNode[YAML_OUTPUTDIRS], context);
  SetOutputNode(contextNode[YAML_OUTPUT], context);
  SetComponentsNode(contextNode[YAML_COMPONENTS], context);
  SetGeneratorsNode(contextNode[YAML_GENERATORS], context);
  SetLinkerNode(contextNode[YAML_LINKER], context);
  SetGroupsNode(contextNode[YAML_GROUPS], context, context->groups);
  SetConstructedFilesNode(contextNode[YAML_CONSTRUCTEDFILES], context);
  SetLicenseInfoNode(contextNode[YAML_LICENSES], context);
}

void ProjMgrYamlCbuild::SetComponentsNode(YAML::Node node, const ContextItem* context) {
  for (const auto& [componentId, component] : context->components) {
    const RteComponent* rteComponent = component.instance->GetParent()->GetComponent();
    const ComponentItem* componentItem = component.item;
    YAML::Node componentNode;
    SetNodeValue(componentNode[YAML_COMPONENT], componentId);
    SetNodeValue(componentNode[YAML_CONDITION], rteComponent->GetConditionID());
    SetNodeValue(componentNode[YAML_FROM_PACK], rteComponent->GetPackageID());
    SetNodeValue(componentNode[YAML_SELECTED_BY], componentItem->component);
    const string& rteDir = rteComponent->GetAttribute("rtedir");
    if (!rteDir.empty()) {
      SetNodeValue(componentNode[YAML_OUTPUT_RTEDIR], FormatPath(context->cproject->directory + "/" + rteDir, context->directories.cbuild));
    }
    SetControlsNode(componentNode, context, componentItem->build);
    SetComponentFilesNode(componentNode[YAML_FILES], context, componentId);
    if (!component.generator.empty()) {
      if (context->generators.find(component.generator) != context->generators.end()) {
        const RteGenerator* rteGenerator = context->generators.at(component.generator);
        SetNodeValue(componentNode[YAML_GENERATOR][YAML_ID], component.generator);
        SetNodeValue(componentNode[YAML_GENERATOR][YAML_FROM_PACK], RtePackage::GetPackageIDfromAttributes(*rteGenerator->GetPackage()));
        SetGeneratorFiles(componentNode[YAML_GENERATOR], context, componentId);
      } else {
        ProjMgrLogger::Warn(string("Component ") + componentId + " uses unknown generator " + component.generator);
      }
    }
    node.push_back(componentNode);
  }
}

void ProjMgrYamlCbuild::SetComponentFilesNode(YAML::Node node, const ContextItem* context, const string& componentId) {
  if (context->componentFiles.find(componentId) != context->componentFiles.end()) {
    for (const auto& [file, attr, category, language, scope, version] : context->componentFiles.at(componentId)) {
      YAML::Node fileNode;
      SetNodeValue(fileNode[YAML_FILE], FormatPath(file, context->directories.cbuild));
      SetNodeValue(fileNode[YAML_CATEGORY], category);
      SetNodeValue(fileNode[YAML_ATTR], attr);
      SetNodeValue(fileNode[YAML_LANGUAGE], language);
      SetNodeValue(fileNode[YAML_SCOPE], scope);
      SetNodeValue(fileNode[YAML_VERSION], version);
      node.push_back(fileNode);
    }
  }
}

void ProjMgrYamlCbuild::SetGeneratorFiles(YAML::Node node, const ContextItem* context, const string& componentId) {
  if (context->generatorInputFiles.find(componentId) != context->generatorInputFiles.end()) {
    YAML::Node filesNode;
    for (const auto& [file, attr, category, language, scope, version] : context->generatorInputFiles.at(componentId)) {
      YAML::Node fileNode;
      SetNodeValue(fileNode[YAML_FILE], FormatPath(file, context->directories.cbuild));
      SetNodeValue(fileNode[YAML_CATEGORY], category);
      SetNodeValue(fileNode[YAML_ATTR], attr);
      SetNodeValue(fileNode[YAML_LANGUAGE], language);
      SetNodeValue(fileNode[YAML_SCOPE], scope);
      SetNodeValue(fileNode[YAML_VERSION], version);
      filesNode.push_back(fileNode);
    }
    node[YAML_FILES] = filesNode;
  }
}

void ProjMgrYamlCbuild::SetGeneratorsNode(YAML::Node node, const ContextItem* context) {
  for (const auto& [generatorId, generator] : context->generators) {
    YAML::Node genNode;
    SetNodeValue(genNode[YAML_GENERATOR], generatorId);

    string workingDir, gpdscFile;
    for (const auto& [gpdsc, item] : context->gpdscs) {
      if (item.generator == generatorId) {
        workingDir = fs::path(context->cproject->directory).append(item.workingDir).generic_string();
        gpdscFile = fs::path(context->cproject->directory).append(gpdsc).generic_string();
        break;
      }
    }
    SetNodeValue(genNode[YAML_FROM_PACK], RtePackage::GetPackageIDfromAttributes(*generator->GetPackage()));
    SetNodeValue(genNode[YAML_PATH], FormatPath(workingDir, context->directories.cbuild));
    SetNodeValue(genNode[YAML_GPDSC], FormatPath(gpdscFile, context->directories.cbuild));

    for (const string host : {"win", "linux", "mac", "other"}) {
      YAML::Node commandNode;

      // Executable file
      const string exe = generator->GetExecutable(context->rteActiveTarget, host);
      if (exe.empty())
        continue;
      commandNode[YAML_FILE] = FormatPath(exe, context->directories.cbuild);

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

    node.push_back(genNode);
  }
}

void ProjMgrYamlCbuild::SetPacksNode(YAML::Node node, const ContextItem* context) {
  for (const auto& [packId, package] : context->packages) {
    YAML::Node packNode;
    SetNodeValue(packNode[YAML_PACK], packId);
    const string& pdscFilename = FormatPath(package->GetPackageFileName(), context->directories.cbuild);
    SetNodeValue(packNode[YAML_PATH], fs::path(pdscFilename).parent_path().generic_string());
    node.push_back(packNode);
  }
}

void ProjMgrYamlCbuild::SetGroupsNode(YAML::Node node, const ContextItem* context, const vector<GroupNode>& groups) {
  for (const auto& group : groups) {
    YAML::Node groupNode;
    SetNodeValue(groupNode[YAML_GROUP], group.group);
    SetControlsNode(groupNode, context, group.build);
    SetFilesNode(groupNode[YAML_FILES], context, group.files);
    SetGroupsNode(groupNode[YAML_GROUPS], context, group.groups);
    node.push_back(groupNode);
  }
}

void ProjMgrYamlCbuild::SetFilesNode(YAML::Node node, const ContextItem* context, const vector<FileNode>& files) {
  for (const auto& file : files) {
    YAML::Node fileNode;
    SetNodeValue(fileNode[YAML_FILE], file.file);
    SetNodeValue(fileNode[YAML_CATEGORY], ProjMgrUtils::GetCategory(file.file));
    SetControlsNode(fileNode, context, file.build);
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
          SetNodeValue(fileNode[YAML_FILE], FormatPath(filename, context->directories.cbuild));
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
  SetNodeValue(rteComponentsNode[YAML_FILE], FormatPath(rteComponents, context->directories.cbuild));
  SetNodeValue(rteComponentsNode[YAML_CATEGORY], "header");
  node.push_back(rteComponentsNode);
}

void ProjMgrYamlCbuild::SetOutputDirsNode(YAML::Node node, const ContextItem* context) {
  const DirectoriesItem& dirs = context->directories;
  map<const string, string> outputDirs = {
    {YAML_OUTPUT_INTDIR, dirs.intdir},
    {YAML_OUTPUT_OUTDIR, dirs.outdir},
    {YAML_OUTPUT_RTEDIR, dirs.rte},
  };
  for (auto [name, dirPath] : outputDirs) {
    RteFsUtils::NormalizePath(dirPath, context->directories.cprj);
    SetNodeValue(node[name], FormatPath(dirPath, context->directories.cbuild));
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
    fs::path(context->linker.script).is_absolute() ? FormatPath(context->linker.script, "") :
    FormatPath(context->directories.cprj + "/" + context->linker.script, context->directories.cbuild);
  const string regions = context->linker.regions.empty() ? "" :
    FormatPath(context->directories.cprj + "/" + context->linker.regions, context->directories.cbuild);
  SetNodeValue(node[YAML_SCRIPT], script);
  SetNodeValue(node[YAML_REGIONS], regions);
  SetDefineNode(node[YAML_DEFINE], context->linker.defines);
}

void ProjMgrYamlCbuild::SetLicenseInfoNode(YAML::Node node, const ContextItem* context) {
  // add licensing info for active target
  RteLicenseInfoCollection licenseInfos;
  context->rteActiveProject->CollectLicenseInfosForTarget(licenseInfos, context->rteActiveTarget->GetName());
  for (auto [id, licInfo] : licenseInfos.GetLicensInfos()) {

    YAML::Node licNode;
    SetNodeValue(licNode[YAML_LICENSE], RteLicenseInfo::ConstructLicenseTitle(licInfo));
    const string& license_agreement = licInfo->GetAttribute("agreement");
    if (!license_agreement.empty()) {
      SetNodeValue(licNode[YAML_LICENSE_AGREEMENT], license_agreement);
    }
    YAML::Node packsNode = licNode[YAML_PACKS];
    for (auto pack : licInfo->GetPackIDs()) {
      YAML::Node packNode;
      SetNodeValue(packNode[YAML_PACK], pack);
      packsNode.push_back(packNode);
    }
    YAML::Node componentsNode = licNode[YAML_COMPONENTS];
    for (auto compID : licInfo->GetComponentIDs()) {
      YAML::Node componentNode;
      SetNodeValue(componentNode[YAML_COMPONENT], compID);
      componentsNode.push_back(componentNode);
    }
    node.push_back(licNode);
  }
}

void ProjMgrYamlCbuild::SetControlsNode(YAML::Node node, const ContextItem* context, const BuildType& controls) {
  SetNodeValue(node[YAML_OPTIMIZE], controls.optimize);
  SetNodeValue(node[YAML_DEBUG], controls.debug);
  SetNodeValue(node[YAML_WARNINGS], controls.warnings);
  SetNodeValue(node[YAML_LANGUAGE_C], controls.languageC);
  SetNodeValue(node[YAML_LANGUAGE_CPP], controls.languageCpp);
  SetMiscNode(node[YAML_MISC], controls.misc);
  SetDefineNode(node[YAML_DEFINE], controls.defines);
  SetNodeValue(node[YAML_UNDEFINE], controls.undefines);
  for (const auto& addpath : controls.addpaths) {
    node[YAML_ADDPATH].push_back(FormatPath(context->directories.cprj + "/" + addpath, context->directories.cbuild));
  }
  for (const auto& addpath : controls.delpaths) {
    node[YAML_DELPATH].push_back(FormatPath(context->directories.cprj + "/" + addpath, context->directories.cbuild));
  }
}

void ProjMgrYamlCbuild::SetProcessorNode(YAML::Node node, const map<string, string>& targetAttributes) {
  for (const auto& [rteKey, yamlKey]: ProjMgrUtils::DeviceAttributesKeys) {
    if (targetAttributes.find(rteKey) != targetAttributes.end()) {
      const auto& rteValue = targetAttributes.at(rteKey);
      const auto& yamlValue = ProjMgrUtils::GetDeviceAttribute(rteKey, rteValue);
      if (!yamlValue.empty()) {
        SetNodeValue(node[yamlKey], yamlValue);
      }
    }
  }
  if (targetAttributes.find("Dcore") != targetAttributes.end()) {
    const string& core = targetAttributes.at("Dcore");
    SetNodeValue(node[YAML_CORE], core);
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

void ProjMgrYamlBase::SetNodeValue(YAML::Node node, const string& value) {
  if (!value.empty()) {
    node = value;
  }
}

void ProjMgrYamlBase::SetNodeValue(YAML::Node node, const vector<string>& vec) {
  for (const string& value : vec) {
    if (!value.empty()) {
      node.push_back(value);
    }
  }
}

void ProjMgrYamlCbuild::SetDefineNode(YAML::Node define, const vector<string>& vec) {
  for (const string& defineStr : vec) {
    if (!defineStr.empty()) {
      auto key = RteUtils::GetPrefix(defineStr, '=');
      auto value = RteUtils::GetSuffix(defineStr, '=');
      if (!value.empty()) {
        // map define
        YAML::Node defineNode;
        SetNodeValue(defineNode[key], value);
        define.push_back(defineNode);
      } else {
        // string define
        define.push_back(key);
      }
    }
  }
}

const string ProjMgrYamlBase::FormatPath(const string& original, const string& directory) {
  string packRoot = ProjMgrKernel::Get()->GetCmsisPackRoot();
  string path = original;
  RteFsUtils::NormalizePath(path);
  error_code ec;
  path = fs::weakly_canonical(fs::path(path), ec).generic_string();
  if (!m_useAbsolutePaths) {
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
  }
  return path;
}

bool ProjMgrYamlBase::CompareFile(const string& filename, const YAML::Node& rootNode) {
  if (!RteFsUtils::Exists(filename)) {
    return false;
  }
  const YAML::Node& yamlRoot = YAML::LoadFile(filename);
  return CompareNodes(yamlRoot, rootNode);
}

bool ProjMgrYamlBase::CompareNodes(const YAML::Node& lhs, const YAML::Node& rhs) {
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

bool ProjMgrYamlBase::WriteFile(YAML::Node& rootNode, const std::string& filename) {
  // Compare yaml contents
  if (!CompareFile(filename, rootNode)) {
    if (!RteFsUtils::MakeSureFilePath(filename)) {
      ProjMgrLogger::Error(filename, "destination directory can not be created");
      return false;
    }
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

bool ProjMgrYamlEmitter::GenerateCbuildIndex(ProjMgrParser& parser, const vector<ContextItem*> contexts, const string& outputDir) {
  // generate cbuild-idx.yml
  const string& directory = outputDir.empty() ? parser.GetCsolution().directory : RteFsUtils::AbsolutePath(outputDir).generic_string();
  const string& filename = directory + "/" + parser.GetCsolution().name + ".cbuild-idx.yml";

  YAML::Node rootNode;
  ProjMgrYamlCbuildIdx cbuild(rootNode[YAML_BUILD_IDX], contexts, parser, directory);

  return cbuild.WriteFile(rootNode, filename);
}

bool ProjMgrYamlEmitter::GenerateCbuild(ContextItem* context, const string& generatorId, const string& generatorPack) {
  // generate cbuild.yml or cbuild-gen.yml for each context
  context->directories.cbuild = context->directories.cprj;
  string tmpDir = context->directories.intdir;
  RteFsUtils::NormalizePath(tmpDir, context->directories.cbuild);
  const string cbuildGenFilename = fs::path(tmpDir).append(context->name + ".cbuild-gen.yml").generic_string();

  // Make sure $G (generator input file) is up to date
  context->rteActiveTarget->SetGeneratorInputFile(cbuildGenFilename);

  string filename, rootKey;
  if (generatorId.empty()) {
    rootKey = YAML_BUILD;
    filename = fs::path(context->directories.cbuild).append(context->name + ".cbuild.yml").generic_string();
  } else {
    rootKey = YAML_BUILD_GEN;
    filename = cbuildGenFilename;
  }
  YAML::Node rootNode;
  ProjMgrYamlCbuild cbuild(rootNode[rootKey], context, generatorId, generatorPack);
  RteFsUtils::CreateDirectories(RteFsUtils::ParentPath(filename));
  return cbuild.WriteFile(rootNode, filename);
}

bool ProjMgrYamlEmitter::GenerateCbuildSet(ProjMgrParser& parser,
  const std::vector<ContextItem*> contexts, const string& selectedCompiler)
{
  const string& filename = parser.GetCsolution().directory + "/" +
    parser.GetCsolution().name + ".cbuild-set.yml";

  YAML::Node rootNode;
  ProjMgrYamlCbuild cbuild(rootNode[YAML_CBUILD_SET], contexts, selectedCompiler);

  return cbuild.WriteFile(rootNode, filename);
}

ProjMgrYamlCbuild::ProjMgrYamlCbuild(YAML::Node node, const vector<ContextItem*>& siblings,
  const string& type, const string& output, const string& gendir) : ProjMgrYamlBase(true)
{
  // construct cbuild-gen-idx.yml content
  SetNodeValue(node[YAML_GENERATED_BY], ORIGINAL_FILENAME + string(" version ") + VERSION_STRING);
  const auto& context = siblings.front();
  const auto& generator = context->extGenDir.begin();
  YAML::Node generatorNode;
  SetNodeValue(generatorNode[YAML_ID], generator->first);
  SetNodeValue(generatorNode[YAML_OUTPUT], FormatPath(gendir, output));
  SetNodeValue(generatorNode[YAML_DEVICE], context->deviceItem.name);
  SetNodeValue(generatorNode[YAML_BOARD], context->board);
  SetNodeValue(generatorNode[YAML_PROJECT_TYPE], type);
  for (const auto& sibling : siblings) {
    YAML::Node cbuildGenNode;
    string tmpDir = sibling->directories.intdir;
    RteFsUtils::NormalizePath(tmpDir, context->directories.cprj);
    string cbuildGenFilename = fs::path(tmpDir).append(sibling->name + ".cbuild-gen.yml").generic_string();
    SetNodeValue(cbuildGenNode[YAML_CBUILD_GEN], FormatPath(cbuildGenFilename, output));
    SetNodeValue(cbuildGenNode[YAML_PROJECT], sibling->cproject->name);
    SetNodeValue(cbuildGenNode[YAML_CONFIGURATION], (sibling->type.build.empty() ? "" :
      ("." + sibling->type.build)) + "+" + sibling->type.target);
    SetNodeValue(cbuildGenNode[YAML_FORPROJECTPART],
      (type == TYPE_MULTI_CORE ? sibling->deviceItem.pname :
      (type == TYPE_TRUSTZONE ? sibling->controls.processed.processor.trustzone : "")));
    generatorNode[YAML_CBUILD_GENS].push_back(cbuildGenNode);
  }
  node[YAML_GENERATORS].push_back(generatorNode);
}

bool ProjMgrYamlEmitter::GenerateCbuildGenIndex(ProjMgrParser& parser, const vector<ContextItem*> siblings,
  const string& type, const string& output, const string& gendir) {
  // generate cbuild-gen-idx.yml as input for external generator
  RteFsUtils::CreateDirectories(output);
  const string& filename = output + "/" + parser.GetCsolution().name + ".cbuild-gen-idx.yml";

  YAML::Node rootNode;
  ProjMgrYamlCbuild cbuild(rootNode[YAML_BUILD_GEN_IDX], siblings, type, output, gendir);
  return cbuild.WriteFile(rootNode, filename);
}
