/*
 * Copyright (c) 2020-2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProductInfo.h"
#include "ProjMgrCbuildBase.h"
#include "ProjMgrLogger.h"
#include "ProjMgrYamlEmitter.h"
#include "ProjMgrYamlParser.h"
#include "RteFsUtils.h"

using namespace std;

// cbuild and cbuild-gen
class ProjMgrCbuild : public ProjMgrCbuildBase {
public:
  ProjMgrCbuild(YAML::Node node, const ContextItem* context, const string& generatorId, const string& generatorPack, bool ignoreRteFileMissing);
private:
  void SetContextNode(YAML::Node node, const ContextItem* context, const string& generatorId, const string& generatorPack);
  void SetComponentsNode(YAML::Node node, const ContextItem* context);
  void SetComponentFilesNode(YAML::Node node, const ContextItem* context, const string& componentId);
  void SetApisNode(YAML::Node node, const ContextItem* context);
  void SetGeneratorsNode(YAML::Node node, const ContextItem* context);
  void SetFiles(YAML::Node node, const std::vector<ComponentFileItem>& files, const std::string& dir);
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
  void SetBooksNode(YAML::Node node, const std::vector<BookItem>& books, const std::string& dir);
  void SetDebugConfigNode(YAML::Node node, const ContextItem* context);
  void SetPLMStatus(YAML::Node node, const ContextItem* context, const string& file);
  void SetWestNode(YAML::Node node, const ContextItem* context);
  bool m_ignoreRteFileMissing;
};

ProjMgrCbuild::ProjMgrCbuild(YAML::Node node, const ContextItem* context,
  const string& generatorId, const string& generatorPack, bool ignoreRteFileMissing) :
  ProjMgrCbuildBase(!generatorId.empty()), m_ignoreRteFileMissing(ignoreRteFileMissing) {
  if (context) {
    SetContextNode(node, context, generatorId, generatorPack);
  }
}

void ProjMgrCbuild::SetContextNode(YAML::Node contextNode, const ContextItem* context,
  const string& generatorId, const string& generatorPack) {
  SetNodeValue(contextNode[YAML_GENERATED_BY], ORIGINAL_FILENAME + string(" version ") + VERSION_STRING);
  if (!generatorId.empty()) {
    YAML::Node generatorNode = contextNode[YAML_CURRENT_GENERATOR];
    SetNodeValue(generatorNode[YAML_ID], generatorId);
    SetNodeValue(generatorNode[YAML_FROM_PACK], generatorPack);
  }
  SetNodeValue(contextNode[YAML_SOLUTION], FormatPath(context->csolution->path, context->directories.cbuild));
  if (!context->cproject->path.empty()) {
    SetNodeValue(contextNode[YAML_PROJECT], FormatPath(context->cproject->path, context->directories.cbuild));
  }
  SetNodeValue(contextNode[YAML_CONTEXT], context->name);
  SetNodeValue(contextNode[YAML_COMPILER], context->toolchain.name +
    (context->toolchain.required.empty() || context->toolchain.required == ">=0.0.0" ? "" : '@' + context->toolchain.required));
  if (!context->boardItem.name.empty()) {
    const auto& board = context->boardItem.vendor + "::" + context->boardItem.name +
      (context->boardItem.revision.empty() ? "" : ":" + context->boardItem.revision);
    SetNodeValue(contextNode[YAML_BOARD], board);
    if (context->boardPack != nullptr) {
      SetNodeValue(contextNode[YAML_BOARD_PACK], context->boardPack->GetID());
    }
    SetBooksNode(contextNode[YAML_BOARD_BOOKS], context->boardBooks, context->directories.cbuild);
  }
  if (!context->deviceItem.name.empty()) {
    const auto& device = context->deviceItem.vendor + "::" + context->deviceItem.name +
      (context->deviceItem.pname.empty() ? "" : ":" + context->deviceItem.pname);
    SetNodeValue(contextNode[YAML_DEVICE], device);
  }
  if (context->devicePack != nullptr) {
    SetNodeValue(contextNode[YAML_DEVICE_PACK], context->devicePack->GetID());
  }
  SetBooksNode(contextNode[YAML_DEVICE_BOOKS], context->deviceBooks, context->directories.cbuild);
  SetDebugConfigNode(contextNode[YAML_DBGCONF], context);
  if (!context->imageOnly && !context->westOn) {
    SetProcessorNode(contextNode[YAML_PROCESSOR], context->targetAttributes);
  }
  SetPacksNode(contextNode[YAML_PACKS], context);
  if (!context->imageOnly && !context->westOn) {
    SetControlsNode(contextNode, context, context->controls.processed);
    vector<string> defines;
    if (context->rteActiveTarget != nullptr) {
      for (const auto& define : context->rteActiveTarget->GetDefines()) {
        CollectionUtils::PushBackUniquely(defines, define);
      }
    }
    SetDefineNode(contextNode[YAML_DEFINE], defines);
    SetDefineNode(contextNode[YAML_DEFINE_ASM], defines);
    if (context->rteActiveTarget != nullptr) {
      for (auto include : context->rteActiveTarget->GetIncludePaths(RteFile::Language::LANGUAGE_NONE)) {
        RteFsUtils::NormalizePath(include, context->cproject->directory);
        include = FormatPath(include, context->directories.cbuild);
        SetNodeValueUniquely(contextNode[YAML_ADDPATH], include);
        SetNodeValueUniquely(contextNode[YAML_ADDPATH_ASM], include);
      }
    }
    SetOutputDirsNode(contextNode[YAML_OUTPUTDIRS], context);
  }
  if (context->westOn) {
    string outDir = context->directories.outdir;
    RteFsUtils::NormalizePath(outDir, context->directories.cprj);
    SetNodeValue(contextNode[YAML_OUTPUTDIRS][YAML_OUTPUT_OUTDIR], FormatPath(outDir, context->directories.cbuild));
  }  
  SetOutputNode(contextNode[YAML_OUTPUT], context);
  if (!context->imageOnly && !context->westOn) {
    SetComponentsNode(contextNode[YAML_COMPONENTS], context);
    SetApisNode(contextNode[YAML_APIS], context);
    SetGeneratorsNode(contextNode[YAML_GENERATORS], context);
    SetLinkerNode(contextNode[YAML_LINKER], context);
    SetGroupsNode(contextNode[YAML_GROUPS], context, context->groups);
    SetConstructedFilesNode(contextNode[YAML_CONSTRUCTEDFILES], context);
  }
  SetLicenseInfoNode(contextNode[YAML_LICENSES], context);
  if (context->westOn) {
    SetWestNode(contextNode[YAML_WEST], context);
  }
}

void ProjMgrCbuild::SetComponentsNode(YAML::Node node, const ContextItem* context) {
  for (const auto& [componentId, component] : context->components) {
    const RteComponent* rteComponent = component.instance->GetComponent();
    if(!rteComponent) {
      continue;
    }
    const ComponentItem* componentItem = component.item;
    YAML::Node componentNode;
    SetNodeValue(componentNode[YAML_COMPONENT], componentId);
    if (component.item->instances > 1) {
      SetNodeValue(componentNode[YAML_INSTANCES], to_string(component.item->instances));
    }
    if (rteComponent->HasMaxInstances()) {
      SetNodeValue(componentNode[YAML_MAX_INSTANCES], to_string(rteComponent->GetMaxInstances()));
    }
    SetNodeValue(componentNode[YAML_CONDITION], rteComponent->GetConditionID());
    SetNodeValue(componentNode[YAML_FROM_PACK], rteComponent->GetPackageID());
    SetNodeValue(componentNode[YAML_SELECTED_BY], componentItem->component);
    const auto& api = rteComponent->GetApi(context->rteActiveTarget, true);
    if (api) {
      SetNodeValue(componentNode[YAML_IMPLEMENTS], api->ConstructComponentID(true));
    }
    SetControlsNode(componentNode, context, componentItem->build);
    SetComponentFilesNode(componentNode[YAML_FILES], context, componentId);
    if (!component.generator.empty()) {
      SetNodeValue(componentNode[YAML_GENERATOR][YAML_ID], component.generator);
      const RteGenerator* rteGenerator = get_or_null(context->generators, component.generator);
      if (rteGenerator && !rteGenerator->IsExternal()) {
        SetNodeValue(componentNode[YAML_GENERATOR][YAML_FROM_PACK], rteGenerator->GetPackageID());
        if (context->generatorInputFiles.find(componentId) != context->generatorInputFiles.end()) {
          SetFiles(componentNode[YAML_GENERATOR], context->generatorInputFiles.at(componentId), context->directories.cbuild);
        }
      } else if (contains_key(context->extGen, component.generator)) {
        SetNodeValue(componentNode[YAML_GENERATOR][YAML_PATH],
          FormatPath(fs::path(context->extGen.at(component.generator).name).generic_string(),
            context->directories.cbuild));
      } else {
        ProjMgrLogger::Get().Warn(string("Component ") + componentId + " uses unknown generator " + component.generator, context->name);
      }
    }
    node.push_back(componentNode);
  }
}

void ProjMgrCbuild::SetDebugConfigNode(YAML::Node node, const ContextItem* context) {
  string dbgconf = context->debugger.dbgconf.empty() ? context->dbgconf.first : context->debugger.dbgconf;
  if (!dbgconf.empty()) {
    YAML::Node fileNode;
    SetNodeValue(fileNode[YAML_FILE], FormatPath(dbgconf, context->directories.cbuild));
    if (dbgconf == context->dbgconf.first) {
      SetNodeValue(fileNode[YAML_VERSION], context->dbgconf.second->GetSemVer(true));
      SetPLMStatus(fileNode, context, dbgconf);
    }
    node.push_back(fileNode);
  }
}

void ProjMgrCbuild::SetComponentFilesNode(YAML::Node node, const ContextItem* context, const string& componentId) {
  if (context->componentFiles.find(componentId) != context->componentFiles.end()) {
    for (const auto& [file, attr, category, language, scope, version, select] : context->componentFiles.at(componentId)) {
      YAML::Node fileNode;
      SetNodeValue(fileNode[YAML_FILE], FormatPath(file, context->directories.cbuild));
      SetNodeValue(fileNode[YAML_CATEGORY], category);
      SetNodeValue(fileNode[YAML_ATTR], attr);
      SetNodeValue(fileNode[YAML_LANGUAGE], language);
      SetNodeValue(fileNode[YAML_SCOPE], scope);
      SetNodeValue(fileNode[YAML_VERSION], version);
      SetNodeValue(fileNode[YAML_SELECT], select);
      if (attr == "config") {
        SetPLMStatus(fileNode, context, file);
      }
      node.push_back(fileNode);
    }
  }
}

void ProjMgrCbuild::SetPLMStatus(YAML::Node node, const ContextItem* context, const string& file) {
  string directory = RteUtils::ExtractFilePath(file, false);
  string name = RteUtils::ExtractFileName(file);

  // lambda to get backup file with specified file filter
  auto GetBackupFile = [&](const string& fileFilter) {
    list<string> backupFiles;
    RteFsUtils::GrepFileNames(backupFiles, directory, fileFilter + "@*");

    // Return empty string if no backup files found
    if (backupFiles.size() == 0) {
      return RteUtils::EMPTY_STRING;
    }

    // Warn if multiple backup files are found
    //This is a safeguard. however, this condition should never be triggered
    if (backupFiles.size() > 1) {
      ProjMgrLogger::Get().Warn(
        "'" + directory + "' contains more than one '" + fileFilter + " file, PLM may fail");
    }

    return FormatPath(backupFiles.front().c_str(), context->directories.cbuild);
  };

  // Get base and update backup files
  string baseFile = GetBackupFile(name + '.' + RteUtils::BASE_STRING);
  string updateFile = GetBackupFile(name + '.' + RteUtils::UPDATE_STRING);

  // Add nodes if both base and update files exist
  if (!baseFile.empty() && !updateFile.empty()) {
    SetNodeValue(node[YAML_BASE], baseFile);
    SetNodeValue(node[YAML_UPDATE], updateFile);
  }

  // Add PLM Status
  if (context->plmStatus.find(file) != context->plmStatus.end()) {
    SetNodeValue(node[YAML_STATUS], context->plmStatus.at(file));
  }
}

void ProjMgrCbuild::SetApisNode(YAML::Node node, const ContextItem* context) {
  for (const auto& [apiId, item] : context->apis) {
    YAML::Node apiNode;
    const auto& api = item.first;
    const auto& componentIds = item.second;
    SetNodeValue(apiNode[YAML_API], apiId);
    SetNodeValue(apiNode[YAML_CONDITION], api->GetConditionID());
    SetNodeValue(apiNode[YAML_FROM_PACK], api->GetPackageID());
    if (componentIds.size() == 1) {
      SetNodeValue(apiNode[YAML_IMPLEMENTED_BY], componentIds.front());
    }
    else {
      SetNodeValue(apiNode[YAML_IMPLEMENTED_BY], componentIds);
    }
    if (context->apiFiles.find(apiId) != context->apiFiles.end()) {
      SetFiles(apiNode, context->apiFiles.at(apiId), context->directories.cbuild);
    }
    node.push_back(apiNode);
  }
}

void ProjMgrCbuild::SetFiles(YAML::Node node, const std::vector<ComponentFileItem>& files, const std::string& dir) {
  YAML::Node filesNode;
  for (const auto& [file, attr, category, language, scope, version, select] : files) {
    YAML::Node fileNode;
    SetNodeValue(fileNode[YAML_FILE], FormatPath(file, dir));
    SetNodeValue(fileNode[YAML_CATEGORY], category);
    SetNodeValue(fileNode[YAML_ATTR], attr);
    SetNodeValue(fileNode[YAML_LANGUAGE], language);
    SetNodeValue(fileNode[YAML_SCOPE], scope);
    SetNodeValue(fileNode[YAML_VERSION], version);
    SetNodeValue(fileNode[YAML_SELECT], select);
    filesNode.push_back(fileNode);
  }
  node[YAML_FILES] = filesNode;
}

void ProjMgrCbuild::SetGeneratorsNode(YAML::Node node, const ContextItem* context) {
  for (const auto& [generatorId, generator] : context->generators) {
    if (generator->IsExternal()) {
      continue;
    }
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
    SetNodeValue(genNode[YAML_FROM_PACK], generator->GetPackageID());
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

void ProjMgrCbuild::SetPacksNode(YAML::Node node, const ContextItem* context) {
  for (const auto& [packId, package] : context->packages) {
    YAML::Node packNode;
    SetNodeValue(packNode[YAML_PACK], packId);
    const string& pdscFilename = FormatPath(package->GetPackageFileName(), context->directories.cbuild);
    SetNodeValue(packNode[YAML_PATH], fs::path(pdscFilename).parent_path().generic_string());
    node.push_back(packNode);
  }
}

void ProjMgrCbuild::SetGroupsNode(YAML::Node node, const ContextItem* context, const vector<GroupNode>& groups) {
  for (const auto& group : groups) {
    if (group.files.empty() && group.groups.empty()) {
      continue;
    }
    YAML::Node groupNode;
    SetNodeValue(groupNode[YAML_GROUP], group.group);
    SetControlsNode(groupNode, context, group.build);
    SetFilesNode(groupNode[YAML_FILES], context, group.files);
    SetGroupsNode(groupNode[YAML_GROUPS], context, group.groups);
    node.push_back(groupNode);
  }
}

void ProjMgrCbuild::SetFilesNode(YAML::Node node, const ContextItem* context, const vector<FileNode>& files) {
  for (const auto& file : files) {
    YAML::Node fileNode;
    string fileName = file.file;
    RteFsUtils::NormalizePath(fileName, context->directories.cprj);
    SetNodeValue(fileNode[YAML_FILE], FormatPath(fileName, context->directories.cbuild));
    SetNodeValue(fileNode[YAML_CATEGORY], file.category.empty() ? RteFsUtils::FileCategoryFromExtension(file.file) : file.category);
    SetNodeValue(fileNode[YAML_LINK], file.link);
    SetControlsNode(fileNode, context, file.build);
    node.push_back(fileNode);
  }
}

void ProjMgrCbuild::SetConstructedFilesNode(YAML::Node node, const ContextItem* context) {
  // constructed preIncludeLocal don't appear here because they come under the component they belong to
  // constructed preIncludeGlobal
  if (context->rteActiveTarget != nullptr) {
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
    if (m_ignoreRteFileMissing || RteFsUtils::Exists(rteComponents)) {
      YAML::Node rteComponentsNode;
      const string path = FormatPath(rteComponents, context->directories.cbuild);
      SetNodeValue(rteComponentsNode[YAML_FILE], path);
      SetNodeValue(rteComponentsNode[YAML_CATEGORY], "header");
      node.push_back(rteComponentsNode);
    }
  }
}

void ProjMgrCbuild::SetOutputDirsNode(YAML::Node node, const ContextItem* context) {
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

void ProjMgrCbuild::SetOutputNode(YAML::Node node, const ContextItem* context) {
  const auto& types = context->outputTypes;
  const vector<tuple<bool, const string, const string>> outputTypes = {
    { types.bin.on, types.bin.filename, RteConstants::OUTPUT_TYPE_BIN },
    { types.elf.on, types.elf.filename, RteConstants::OUTPUT_TYPE_ELF },
    { types.hex.on, types.hex.filename, RteConstants::OUTPUT_TYPE_HEX },
    { types.lib.on, types.lib.filename, RteConstants::OUTPUT_TYPE_LIB },
    { types.cmse.on, types.cmse.filename, RteConstants::OUTPUT_TYPE_CMSE },
    { types.map.on, types.map.filename, RteConstants::OUTPUT_TYPE_MAP },
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

void ProjMgrCbuild::SetLinkerNode(YAML::Node node, const ContextItem* context) {
  const string script = context->linker.script.empty() ? "" :
    fs::path(context->linker.script).is_absolute() ? FormatPath(context->linker.script, "") :
    FormatPath(context->directories.cprj + "/" + context->linker.script, context->directories.cbuild);
  const string regions = context->linker.regions.empty() ? "" :
    FormatPath(context->directories.cprj + "/" + context->linker.regions, context->directories.cbuild);
  SetNodeValue(node[YAML_SCRIPT], script);
  SetNodeValue(node[YAML_REGIONS], regions);
  SetDefineNode(node[YAML_DEFINE], context->linker.defines);
}

void ProjMgrCbuild::SetLicenseInfoNode(YAML::Node node, const ContextItem* context) {
  // add licensing info for active target
  if (context->rteActiveProject != nullptr) {
    RteLicenseInfoCollection licenseInfos;
    context->rteActiveProject->CollectLicenseInfosForTarget(licenseInfos, context->rteActiveTarget->GetName());
    for (auto [id, licInfo] : licenseInfos.GetLicensInfos()) {

      YAML::Node licNode;
      SetNodeValue(licNode[YAML_LICENSE], RteLicenseInfo::ConstructLicenseTitle(licInfo));
      const string& license_agreement = licInfo->GetAttribute("agreement");
      if (!license_agreement.empty()) {
        SetNodeValue(licNode[YAML_LICENSE_AGREEMENT], FormatPath(license_agreement, context->directories.cbuild));
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
}

void ProjMgrCbuild::SetControlsNode(YAML::Node node, const ContextItem* context, const BuildType& controls) {
  SetNodeValue(node[YAML_OPTIMIZE], controls.optimize);
  SetNodeValue(node[YAML_DEBUG], controls.debug);
  SetNodeValue(node[YAML_WARNINGS], controls.warnings);
  SetNodeValue(node[YAML_LANGUAGE_C], controls.languageC);
  SetNodeValue(node[YAML_LANGUAGE_CPP], controls.languageCpp);
  if (controls.lto) {
    node[YAML_LINK_TIME_OPTIMIZE] = true;
  }
  SetMiscNode(node[YAML_MISC], controls.misc);
  SetDefineNode(node[YAML_DEFINE], controls.defines);
  SetDefineNode(node[YAML_DEFINE_ASM], controls.definesAsm);
  SetNodeValue(node[YAML_UNDEFINE], controls.undefines);
  for (auto addpath : controls.addpaths) {
    RteFsUtils::NormalizePath(addpath, context->directories.cprj);
    SetNodeValueUniquely(node[YAML_ADDPATH], FormatPath(addpath, context->directories.cbuild));
  }
  for (auto addpath : controls.addpathsAsm) {
    RteFsUtils::NormalizePath(addpath, context->directories.cprj);
    SetNodeValueUniquely(node[YAML_ADDPATH_ASM], FormatPath(addpath, context->directories.cbuild));
  }
  for (auto delpath : controls.delpaths) {
    RteFsUtils::NormalizePath(delpath, context->directories.cprj);
    SetNodeValueUniquely(node[YAML_DELPATH], FormatPath(delpath, context->directories.cbuild));
  }
}

void ProjMgrCbuild::SetBooksNode(YAML::Node node, const std::vector<BookItem>& books, const std::string& dir) {
  for (const auto& book : books) {
    YAML::Node bookNode;
    SetNodeValue(bookNode[YAML_NAME], FormatPath(book.name, dir));
    SetNodeValue(bookNode[YAML_TITLE], book.title);
    SetNodeValue(bookNode[YAML_CATEGORY], book.category);
    node.push_back(bookNode);
  }
}

void ProjMgrCbuild::SetProcessorNode(YAML::Node node, const map<string, string>& targetAttributes) {
  for (const auto& [rteKey, yamlKey] : RteConstants::DeviceAttributesKeys) {
    if (targetAttributes.find(rteKey) != targetAttributes.end()) {
      const auto& rteValue = targetAttributes.at(rteKey);
      const auto& yamlValue = RteConstants::GetDeviceAttribute(rteKey, rteValue);
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

void ProjMgrCbuild::SetMiscNode(YAML::Node miscNode, const vector<MiscItem>& miscVec) {
  if (!miscVec.empty()) {
    SetMiscNode(miscNode, miscVec.front());
  }
}

void ProjMgrCbuild::SetMiscNode(YAML::Node miscNode, const MiscItem& misc) {
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


void ProjMgrCbuild::SetDefineNode(YAML::Node define, const vector<string>& vec) {
  for (const string& defineStr : vec) {
    if (!defineStr.empty()) {
      auto key = RteUtils::GetPrefix(defineStr, '=');
      auto value = RteUtils::GetSuffix(defineStr, '=');
      if (!value.empty()) {
        // map define
        YAML::Node defineNode;
        SetNodeValue(defineNode[key], value);
        define.push_back(defineNode);
      }
      else {
        // string define
        define.push_back(key);
      }
    }
  }
}

void ProjMgrCbuild::SetWestNode(YAML::Node node, const ContextItem* context) {
  SetNodeValue(node[YAML_PROJECT_ID], context->west.projectId);
  SetNodeValue(node[YAML_APP_PATH], FormatPath(context->west.app, context->directories.cbuild));
  SetNodeValue(node[YAML_BOARD], context->west.board);
  SetNodeValue(node[YAML_DEVICE], context->west.device);
  SetDefineNode(node[YAML_WEST_DEFS], context->west.westDefs);
  SetNodeValue(node[YAML_WEST_OPT], context->west.westOpt);
}

//-- ProjMgrYamlEmitter::GenerateCbuild -----------------------------------------------------------
bool ProjMgrYamlEmitter::GenerateCbuild(ContextItem* context,
  const string& generatorId, const string& generatorPack, bool ignoreRteFileMissing)
{
  // generate cbuild.yml or cbuild-gen.yml for each context
  context->directories.cbuild = context->directories.cprj;
  string tmpDir = context->directories.intdir;
  RteFsUtils::NormalizePath(tmpDir, context->directories.cbuild);
  const string cbuildGenFilename = fs::path(tmpDir).append(context->name + ".cbuild-gen.yml").generic_string();

  // Make sure $G (generator input file) is up to date
  if (context->rteActiveTarget != nullptr) {
    context->rteActiveTarget->SetGeneratorInputFile(cbuildGenFilename);
  }

  string filename, rootKey;
  if (generatorId.empty()) {
    rootKey = YAML_BUILD;
    filename = fs::path(context->directories.cbuild).append(context->name + ".cbuild.yml").generic_string();
  }
  else {
    rootKey = YAML_BUILD_GEN;
    filename = cbuildGenFilename;
  }
  YAML::Node rootNode;
  ProjMgrCbuild cbuild(rootNode[rootKey], context, generatorId, generatorPack, ignoreRteFileMissing);
  if (context->westOn) {
    CopyWestGroups(filename, rootNode);
  }
  RteFsUtils::CreateDirectories(RteFsUtils::ParentPath(filename));
  // get context rebuild flag
  context->needRebuild = NeedRebuild(filename, rootNode);
  return WriteFile(rootNode, filename, context->name);
}

void ProjMgrYamlEmitter::CopyWestGroups(const string& filename, YAML::Node rootNode) {
  if (!RteFsUtils::Exists(filename) || !RteFsUtils::IsRegularFile(filename)) {
    return;
  }
  const YAML::Node& cbuildFile = YAML::LoadFile(filename);
  if (cbuildFile[YAML_BUILD][YAML_GROUPS].IsDefined()) {
    rootNode[YAML_BUILD][YAML_GROUPS] = cbuildFile[YAML_BUILD][YAML_GROUPS];
  }
}
