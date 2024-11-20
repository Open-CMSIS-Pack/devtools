/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProductInfo.h"
#include "ProjMgrLogger.h"
#include "ProjMgrYamlEmitter.h"
#include "ProjMgrYamlParser.h"
#include "ProjMgrYamlSchemaChecker.h"
#include "ProjMgrWorker.h"
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
  ProjMgrYamlBase(bool useAbsolutePaths = false, bool checkSchema = false);
  void SetNodeValue(YAML::Node node, const string& value);
  void SetNodeValue(YAML::Node node, const vector<string>& vec);
  const string FormatPath(const string& original, const string& directory);
  bool CompareFile(const string& filename, const YAML::Node& rootNode);
  bool CompareNodes(const YAML::Node& lhs, const YAML::Node& rhs);
  bool WriteFile(YAML::Node& rootNode, const std::string& filename, const std::string& context = string(), bool allowUpdate = true);
  void SetExecutesNode(YAML::Node node, const map<string, ExecutesItem>& executes, const string& base, const string& ref);
  bool NeedRebuild(const string& filename, const YAML::Node& rootNode);
  const bool m_useAbsolutePaths;
  const bool m_checkSchema;
};

class ProjMgrYamlCbuild : public ProjMgrYamlBase {
private:
  friend class ProjMgrYamlEmitter;
  ProjMgrYamlCbuild(YAML::Node node, const vector<string>& selectedContexts, const string& selectedCompiler, bool checkSchema, bool ignoreRteFileMissing);
  ProjMgrYamlCbuild(YAML::Node node, const ContextItem* context, const string& generatorId, const string& generatorPack, bool checkSchema, bool ignoreRteFileMissing);
  ProjMgrYamlCbuild(YAML::Node node, const vector<ContextItem*>& siblings, const string& type, const string& output, const string& gendir, bool checkSchema);
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
  const bool m_ignoreRteFileMissing;
};

class ProjMgrYamlCbuildIdx : public ProjMgrYamlBase {
private:
  friend class ProjMgrYamlEmitter;
  ProjMgrYamlCbuildIdx(
    YAML::Node node, const vector<ContextItem*>& processedContexts,
    ProjMgrParser& parser, ProjMgrWorker& worker, const string& directory, const set<string>& failedContexts,
    const map<string, ExecutesItem>& executes, bool checkSchema);

  void SetVariablesNode(YAML::Node node, ProjMgrParser& parser, const ContextItem* context, const map<string, map<string, set<const ConnectItem*>>>& layerTypes);
};

class ProjMgrYamlCbuildPack : public ProjMgrYamlBase {
private:
  friend class ProjMgrYamlEmitter;
  ProjMgrYamlCbuildPack(
    YAML::Node node, const vector<ContextItem*>& processedContexts,
    ProjMgrParser& parser, bool keepExistingPackContent, bool checkSchema);
};


ProjMgrYamlCbuildPack::ProjMgrYamlCbuildPack(YAML::Node node, const vector<ContextItem*>& processedContexts, ProjMgrParser& parser, bool keepExistingPackContent, bool checkSchema) :
  ProjMgrYamlBase(false, checkSchema)
{
  const auto& csolution = parser.GetCsolution();

  struct ModelItem {
    PackInfo info;
    ResolvedPackItem resolvedPack;
  };

  map<string, ModelItem> model;

  // Stage 1: Add all known items from the current cbuild pack file, if considering all contexts
  if (keepExistingPackContent) {
    for (const auto& resolvedItem : csolution.cbuildPack.packs) {
      ModelItem modelItem;
      ProjMgrUtils::ConvertToPackInfo(resolvedItem.pack, modelItem.info);
      modelItem.resolvedPack.pack = resolvedItem.pack;
      modelItem.resolvedPack.selectedByPack = resolvedItem.selectedByPack;
      model[resolvedItem.pack] = modelItem;
    }
  }

  // Stage 2: Process packs that are required by used components
  for (const auto& context : processedContexts) {
    for (const auto& [packId, package] : context->packages) {
      // Skip project local packs
      const string& packPath = package->GetRootFilePath(false);
      if (context->localPackPaths.find(packPath) != context->localPackPaths.end()) {
        continue;
      }

      if (model.find(packId) == model.end()) {
        // Add pack
        ModelItem modelItem;
        ProjMgrUtils::ConvertToPackInfo(packId, modelItem.info);
        modelItem.resolvedPack.pack = packId;
        model[packId] = modelItem;
      }
    }
  }

  // Stage 3: Add all user input expression on the matching resolved pack
  for (const auto& context : processedContexts) {
    for (const auto& [userInput, resolvedPacks] : context->userInputToResolvedPackIdMap) {
      for (const auto& resolvedPack : resolvedPacks) {
        if (model.find(resolvedPack) == model.end()) {
          ModelItem modelItem;
          ProjMgrUtils::ConvertToPackInfo(resolvedPack, modelItem.info);
          modelItem.resolvedPack.pack = resolvedPack;
          model[resolvedPack] = modelItem;
        }

        CollectionUtils::PushBackUniquely(model[resolvedPack].resolvedPack.selectedByPack, userInput);
      }
    }
  }

  // Stage 4: Process all wildcard patterns from user and add to selected-by-pack list
  for (const auto& context : processedContexts) {
    for (const auto& packItem : context->packRequirements) {
      // Skip project local packs
      if (!packItem.path.empty()) {
        continue;
      }

      const PackInfo& reqInfo = packItem.pack;
      if (reqInfo.name.empty() || WildCards::IsWildcardPattern(reqInfo.name)) {
        const string packId = RtePackage::ComposePackageID(reqInfo.vendor, reqInfo.name, reqInfo.version);

        for (auto& [_, item] : model) {
          if (ProjMgrUtils::IsMatchingPackInfo(item.info, reqInfo)) {
            CollectionUtils::PushBackUniquely(item.resolvedPack.selectedByPack, packId);
          }
        }
      }
    }
  }

  // Sort model before saving to ensure stable cbuild-pack.yml content
  vector<pair<string, ModelItem>> sortedModel;
  for (const auto& item : model) {
    sortedModel.push_back(item);
  }
  std::sort(sortedModel.begin(), sortedModel.end(), [](const pair<string, ModelItem> &item1, const pair<string, ModelItem> &item2) {
    PackInfo packInfo1, packInfo2;
    ProjMgrUtils::ConvertToPackInfo(item1.first, packInfo1);
    ProjMgrUtils::ConvertToPackInfo(item2.first, packInfo2);

    if (packInfo1.vendor == packInfo2.vendor) {
      if (packInfo1.name == packInfo2.name) {
        // Order by pack version
        return VersionCmp::Compare(packInfo1.version, packInfo2.version) < 0;
      }

      // Order by pack name
      return packInfo1.name < packInfo2.name;
    }

    // Order by vendor name
    return packInfo1.vendor < packInfo2.vendor;
  });

  // Produce the yml output
  for (auto& [packId, modelItem] : sortedModel) {
    YAML::Node resolvedPackNode;
    auto& packItem = modelItem.resolvedPack;

    SetNodeValue(resolvedPackNode[YAML_RESOLVED_PACK], packId);

    sort(packItem.selectedByPack.begin(), packItem.selectedByPack.end());
    SetNodeValue(resolvedPackNode[YAML_SELECTED_BY_PACK], packItem.selectedByPack);

    node[YAML_RESOLVED_PACKS].push_back(resolvedPackNode);
  }
}

ProjMgrYamlBase::ProjMgrYamlBase(bool useAbsolutePaths, bool checkSchema) : m_useAbsolutePaths(useAbsolutePaths), m_checkSchema(checkSchema) {
}

ProjMgrYamlCbuildIdx::ProjMgrYamlCbuildIdx(YAML::Node node,
  const vector<ContextItem*>& processedContexts, ProjMgrParser& parser, ProjMgrWorker& worker, const string& directory, const set<string>& failedContexts, 
  const map<string, ExecutesItem>& executes, bool checkSchema) : ProjMgrYamlBase(false, checkSchema)
{
  error_code ec;
  SetNodeValue(node[YAML_GENERATED_BY], ORIGINAL_FILENAME + string(" version ") + VERSION_STRING);
  if (processedContexts.size() > 0) {
    SetNodeValue(node[YAML_DESCRIPTION], processedContexts[0]->csolution->description);
  }
  if (!parser.GetCdefault().path.empty()) {
    SetNodeValue(node[YAML_CDEFAULT], FormatPath(parser.GetCdefault().path, directory));
  }
  SetNodeValue(node[YAML_CSOLUTION], FormatPath(parser.GetCsolution().path, directory));
  SetNodeValue(node[YAML_OUTPUT_TMPDIR], FormatPath(parser.GetCsolution().directories.tmpdir, directory));

  // Generate layer info for each target
  vector<string> configTargets;
  for (const auto& context : processedContexts) {
    // Retrieve layer information for a single target exclusively
    if (std::find(configTargets.begin(), configTargets.end(), context->type.target) != configTargets.end()) {
      continue;
    }

    // Collect layer connection info specific to each target
    if (context->validConnections.size() > 0) {
      configTargets.push_back(context->type.target);
      map<int, map<string, map<string, set<const ConnectItem*>>>> configurations;
      int index = 0;
      for (const auto& combination : context->validConnections) {
        index++;
        for (const auto& item : combination) {
          if (item.type.empty())
            continue;
          for (const auto& [type, _] : context->compatibleLayers) {
            // add all required layer types, including discarded optionals
            configurations[index][type].insert({});
          }
          for (const auto& connect : item.connections) {
            configurations[index][item.type][item.filename].insert(connect);
          }
        }
      }

      // Process connection info and generate nodes
      if (configurations.size() > 0) {
        YAML::Node targetTypeNode;
        SetNodeValue(targetTypeNode[YAML_TARGETTYPE], context->type.target);
        for (const auto& [index, types] : configurations) {
          YAML::Node configurationsNode;
          configurationsNode[YAML_CONFIGURATION] = YAML::Null;
          SetVariablesNode(configurationsNode[YAML_VARIABLES], parser, context, types);
          targetTypeNode[YAML_TARGET_CONFIGURATIONS].push_back(configurationsNode);
        }
        node[YAML_CONFIGURATIONS].push_back(targetTypeNode);
      }
    }
  }

  // Generate select-compiler info
  if (worker.GetSelectableCompilers().size() > 0) {
    for (const auto& selectableCompiler : worker.GetSelectableCompilers()) {
      YAML::Node selectCompilerNode;
      SetNodeValue(selectCompilerNode[YAML_COMPILER], selectableCompiler);
      node[YAML_SELECT_COMPILER].push_back(selectCompilerNode);
    }
  }

  const auto& cprojects = parser.GetCprojects();
  const auto& csolution = parser.GetCsolution();

  for (const auto& cprojectFile : csolution.cprojects) {
    auto itr = std::find_if(cprojects.begin(), cprojects.end(),
      [&](const pair<std::string, CprojectItem>& elem) {
        return (fs::path(elem.first).filename().string() == fs::path(cprojectFile).filename().string());
      });
    auto cproject = itr->second;
    YAML::Node cprojectNode;
    const string& cprojectFilename = fs::relative(cproject.path, directory, ec).generic_string();
    SetNodeValue(cprojectNode[YAML_CPROJECT], cprojectFilename);
    vector<string> clayerFilenames;
    for (const auto& item : cproject.clayers) {
      string clayerPath = item.layer;
      RteFsUtils::NormalizePath(clayerPath, cproject.directory);
      const string& clayerFilename = fs::relative(clayerPath, directory, ec).generic_string();
      CollectionUtils::PushBackUniquely(clayerFilenames, clayerFilename);
    }
    for (const auto& clayerFilename : clayerFilenames) {
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
      for (const auto& [clayerFilename, clayerItem] : context->clayers) {
        YAML::Node clayerNode;
        SetNodeValue(clayerNode[YAML_CLAYER], FormatPath(clayerFilename, directory));
        cbuildNode[YAML_CLAYERS].push_back(clayerNode);
      }
      SetNodeValue(cbuildNode[YAML_DEPENDS_ON], context->dependsOn);
      if (std::find(failedContexts.begin(), failedContexts.end(), context->name) != failedContexts.end()) {
        cbuildNode[YAML_ERRORS] = true;
      }

      // errors, warnings and info messages
      const vector<pair<StrVecMap&, const char*>> messages = {
        { ProjMgrLogger::Get().GetErrors(), YAML_ERRORS },
        { ProjMgrLogger::Get().GetWarns(), YAML_WARNINGS },
        { ProjMgrLogger::Get().GetInfos(), YAML_INFO },
      };
      for (const auto& [msgMap, node] : messages) {
        for (const auto& [contextName, msgVec] : msgMap) {
          if (contextName.empty() || contextName == context->name) {
            for (const auto& msg : msgVec) {
              cbuildNode[YAML_MESSAGES][node].push_back(msg);
            }
          }
        }
      }

      vector<string> missingPacks;
      for (const auto& packInfo : context->missingPacks) {
        std::string missingPackName =
          (packInfo.vendor.empty() ? "" : packInfo.vendor + "::") +
          packInfo.name +
          (packInfo.version.empty() ? "" : "@" + packInfo.version);
        CollectionUtils::PushBackUniquely(missingPacks, missingPackName);
      }

      for (const auto& pack : missingPacks) {
        YAML::Node packNode;
        SetNodeValue(packNode[YAML_PACK], pack);
        cbuildNode[YAML_PACKS_MISSING].push_back(packNode);
      }

      for (const auto& pack : context->unusedPacks) {
        YAML::Node packNode;
        SetNodeValue(packNode[YAML_PACK], pack);
        cbuildNode[YAML_PACKS_UNUSED].push_back(packNode);
      }
      node[YAML_CBUILDS].push_back(cbuildNode);
    }
  }

  SetExecutesNode(node[YAML_EXECUTES], executes, directory, directory);
}

void ProjMgrYamlCbuildIdx::SetVariablesNode(YAML::Node node, ProjMgrParser& parser, const ContextItem* context,
  const map<string, map<string, set<const ConnectItem*>>>& layerTypes) {
  for (const auto& [type, filenames] : layerTypes) {
    if (type.empty()) {
      continue;
    }
    YAML::Node layerNode;
    string layerFile;
    if (filenames.empty()) {
      layerNode[type + "-Layer"] = "";
    }
    for (const auto& [filename, options] : filenames) {
      string packRoot = ProjMgrKernel::Get()->GetCmsisPackRoot();
      layerFile = filename;
      RteFsUtils::NormalizePath(layerFile);
      layerFile = RteFsUtils::MakePathCanonical(layerFile);
      // Replace with ${CMSIS_PACK_ROOT} or $SolutionDir()$ depending on the detected path
      size_t index = layerFile.find(packRoot);
      if (index != string::npos) {
        layerFile.replace(index, packRoot.length(), "${CMSIS_PACK_ROOT}");
      }
      else {
        string relPath = RteFsUtils::RelativePath(filename, context->csolution->directory);
        if (!relPath.empty()) {
          layerFile = "$" + string(RteConstants::AS_SOLUTION_DIR_BR) + "$/" + RteFsUtils::LexicallyNormal(relPath);
        }
      }
      SetNodeValue(layerNode[type + "-Layer"], layerFile);
      if (parser.GetGenericClayers().find(filename) != parser.GetGenericClayers().end()) {
        const auto& clayer = parser.GetGenericClayers().at(filename);
        SetNodeValue(layerNode[YAML_DESCRIPTION], clayer.description);
      }
      for (const auto& connect : options) {
        if (!connect->set.empty()) {
          YAML::Node setNode;
          SetNodeValue(setNode[YAML_SET], connect->set + " (" + connect->connect + (connect->info.empty() ? "" : " - " + connect->info) + ")");
          layerNode[YAML_SETTINGS].push_back(setNode);
        }
      }
      if (context->packLayers.find(filename) != context->packLayers.end()) {
        const auto& clayer = context->packLayers.at(filename);
        SetNodeValue(layerNode[YAML_PATH], clayer->GetOriginalAbsolutePath(clayer->GetPathString()));
        SetNodeValue(layerNode[YAML_FILE], clayer->GetFileString());
        SetNodeValue(layerNode[YAML_COPY_TO], clayer->GetCopyToString());
      }
    }
    node.push_back(layerNode);
  }
}

ProjMgrYamlCbuild::ProjMgrYamlCbuild(YAML::Node node, const ContextItem* context,
  const string& generatorId, const string& generatorPack, bool checkSchema, bool ignoreRteFileMissing) : ProjMgrYamlBase(!generatorId.empty(), checkSchema), m_ignoreRteFileMissing(ignoreRteFileMissing)
{
  if (context) {
    SetContextNode(node, context, generatorId, generatorPack);
  }
}

ProjMgrYamlCbuild::ProjMgrYamlCbuild(YAML::Node node,
  const vector<string>& selectedContexts, const string& selectedCompiler, bool checkSchema, bool ignoreRteFileMissing) : m_ignoreRteFileMissing(ignoreRteFileMissing)
{
  SetNodeValue(node[YAML_GENERATED_BY], ORIGINAL_FILENAME + string(" version ") + VERSION_STRING);
  YAML::Node contextsNode = node[YAML_CONTEXTS];
  for (const auto& context : selectedContexts) {
    YAML::Node contextNode;
    SetNodeValue(contextNode[YAML_CONTEXT], context);
    contextsNode.push_back(contextNode);
  }
  if (!selectedCompiler.empty()) {
    SetNodeValue(node[YAML_COMPILER], selectedCompiler);
  }
}

void ProjMgrYamlCbuild::SetContextNode(YAML::Node contextNode, const ContextItem* context,
  const string& generatorId, const string& generatorPack)
{
  SetNodeValue(contextNode[YAML_GENERATED_BY], ORIGINAL_FILENAME + string(" version ") + VERSION_STRING);
  if (!generatorId.empty()) {
    YAML::Node generatorNode = contextNode[YAML_CURRENT_GENERATOR];
    SetNodeValue(generatorNode[YAML_ID], generatorId);
    SetNodeValue(generatorNode[YAML_FROM_PACK], generatorPack);
  }
  SetNodeValue(contextNode[YAML_SOLUTION], FormatPath(context->csolution->path, context->directories.cbuild));
  SetNodeValue(contextNode[YAML_PROJECT], FormatPath(context->cproject->path, context->directories.cbuild));
  SetNodeValue(contextNode[YAML_CONTEXT], context->name);
  SetNodeValue(contextNode[YAML_COMPILER], context->toolchain.name +
    (context->toolchain.required.empty() || context->toolchain.required == ">=0.0.0" ? "" : '@' + context->toolchain.required));
  if (!context->board.empty()) {
    SetNodeValue(contextNode[YAML_BOARD], context->board);
    if (context->boardPack != nullptr) {
      SetNodeValue(contextNode[YAML_BOARD_PACK], context->boardPack->GetID());
    }
  }
  SetNodeValue(contextNode[YAML_DEVICE], context->device);
  if (context->devicePack != nullptr) {
    SetNodeValue(contextNode[YAML_DEVICE_PACK], context->devicePack->GetID());
  }
  SetProcessorNode(contextNode[YAML_PROCESSOR], context->targetAttributes);
  SetPacksNode(contextNode[YAML_PACKS], context);
  SetControlsNode(contextNode, context, context->controls.processed);
  vector<string> defines;
  if (context->rteActiveTarget != nullptr) {
    for (const auto& define : context->rteActiveTarget->GetDefines()) {
      CollectionUtils::PushBackUniquely(defines, define);
    }
  }
  SetDefineNode(contextNode[YAML_DEFINE], defines);
  SetDefineNode(contextNode[YAML_DEFINE_ASM], defines);
  vector<string> includes;
  if (context->rteActiveTarget != nullptr) {
    for (auto include : context->rteActiveTarget->GetIncludePaths(RteFile::Language::LANGUAGE_NONE)) {
      RteFsUtils::NormalizePath(include, context->cproject->directory);
      CollectionUtils::PushBackUniquely(includes, FormatPath(include, context->directories.cbuild));
    }
  }
  SetNodeValue(contextNode[YAML_ADDPATH], includes);
  SetNodeValue(contextNode[YAML_ADDPATH_ASM], includes);
  SetOutputDirsNode(contextNode[YAML_OUTPUTDIRS], context);
  SetOutputNode(contextNode[YAML_OUTPUT], context);
  SetComponentsNode(contextNode[YAML_COMPONENTS], context);
  SetApisNode(contextNode[YAML_APIS], context);
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
    if (component.item->instances > 1) {
      SetNodeValue(componentNode[YAML_INSTANCES], to_string(component.item->instances));
    }
    SetNodeValue(componentNode[YAML_CONDITION], rteComponent->GetConditionID());
    SetNodeValue(componentNode[YAML_FROM_PACK], rteComponent->GetPackageID());
    SetNodeValue(componentNode[YAML_SELECTED_BY], componentItem->component);
    const auto& api = rteComponent->GetApi(context->rteActiveTarget, true);
    if (api) {
      SetNodeValue(componentNode[YAML_IMPLEMENTS], api->ConstructComponentID(true));
    }
    const string& rteDir = rteComponent->GetAttribute("rtedir");
    if (!rteDir.empty()) {
      SetNodeValue(componentNode[YAML_OUTPUT_RTEDIR], FormatPath(context->cproject->directory + "/" + rteDir, context->directories.cbuild));
    }
    SetControlsNode(componentNode, context, componentItem->build);
    SetComponentFilesNode(componentNode[YAML_FILES], context, componentId);
    if (!component.generator.empty()) {
      SetNodeValue(componentNode[YAML_GENERATOR][YAML_ID], component.generator);
      const RteGenerator* rteGenerator = get_or_null(context->generators, component.generator);
      if(rteGenerator && !rteGenerator->IsExternal()) {
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

void ProjMgrYamlCbuild::SetComponentFilesNode(YAML::Node node, const ContextItem* context, const string& componentId) {
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
          if (backupFiles.size() > 1){
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
          SetNodeValue(fileNode[YAML_BASE], baseFile);
          SetNodeValue(fileNode[YAML_UPDATE], updateFile);
        }

        // Add PLM Status
        if (context->plmStatus.find(file) != context->plmStatus.end()) {
          SetNodeValue(fileNode[YAML_STATUS], context->plmStatus.at(file));
        }
      }
      node.push_back(fileNode);
    }
  }
}

void ProjMgrYamlCbuild::SetApisNode(YAML::Node node, const ContextItem* context) {
  for (const auto& [api, componentIds] : context->apis) {
    YAML::Node apiNode;
    const auto& apiId = api->ConstructComponentID(true);
    SetNodeValue(apiNode[YAML_API], apiId);
    SetNodeValue(apiNode[YAML_CONDITION], api->GetConditionID());
    SetNodeValue(apiNode[YAML_FROM_PACK], api->GetPackageID());
    if (componentIds.size() == 1) {
      SetNodeValue(apiNode[YAML_IMPLEMENTED_BY], componentIds.front());
    } else {
      SetNodeValue(apiNode[YAML_IMPLEMENTED_BY], componentIds);
    }
    if (context->apiFiles.find(apiId) != context->apiFiles.end()) {
      SetFiles(apiNode, context->apiFiles.at(apiId), context->directories.cbuild);
    }
    node.push_back(apiNode);
  }
}

void ProjMgrYamlCbuild::SetFiles(YAML::Node node, const std::vector<ComponentFileItem>& files, const std::string& dir) {
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

void ProjMgrYamlCbuild::SetGeneratorsNode(YAML::Node node, const ContextItem* context) {
  for (const auto& [generatorId, generator] : context->generators) {
    if(generator->IsExternal()) {
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

void ProjMgrYamlCbuild::SetFilesNode(YAML::Node node, const ContextItem* context, const vector<FileNode>& files) {
  for (const auto& file : files) {
    YAML::Node fileNode;
    string fileName = file.file;
    RteFsUtils::NormalizePath(fileName, context->directories.cprj);
    SetNodeValue(fileNode[YAML_FILE], FormatPath(fileName, context->directories.cbuild));
    SetNodeValue(fileNode[YAML_CATEGORY], file.category.empty() ? RteFsUtils::FileCategoryFromExtension(file.file) : file.category);
    SetControlsNode(fileNode, context, file.build);
    node.push_back(fileNode);
  }
}

void ProjMgrYamlCbuild::SetConstructedFilesNode(YAML::Node node, const ContextItem* context) {
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
  if (context->rteActiveProject != nullptr) {
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
}

void ProjMgrYamlBase::SetExecutesNode(YAML::Node node, const map<string, ExecutesItem>& executes, const string& base, const string& ref) {
  for (const auto& [_, item] : executes) {
    YAML::Node executeNode;
    SetNodeValue(executeNode[YAML_EXECUTE], item.execute);
    SetNodeValue(executeNode[YAML_RUN], item.run);
    if (item.always) {
      executeNode[YAML_ALWAYS] = YAML::Null;
    }
    vector<string> input;
    for (const auto& file : item.input) {
      input.push_back(FormatPath(base + "/" + file, ref));
    }
    SetNodeValue(executeNode[YAML_INPUT], input);
    vector<string> output;
    for (const auto& file : item.output) {
      output.push_back(FormatPath(base + "/" + file, ref));
    }
    SetNodeValue(executeNode[YAML_OUTPUT], output);
    SetNodeValue(executeNode[YAML_DEPENDS_ON], item.dependsOn);
    node.push_back(executeNode);
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
  SetDefineNode(node[YAML_DEFINE_ASM], controls.definesAsm);
  SetNodeValue(node[YAML_UNDEFINE], controls.undefines);
  for (const auto& addpath : controls.addpaths) {
    node[YAML_ADDPATH].push_back(FormatPath(context->directories.cprj + "/" + addpath, context->directories.cbuild));
  }
  for (const auto& addpath : controls.addpathsAsm) {
    node[YAML_ADDPATH_ASM].push_back(FormatPath(context->directories.cprj + "/" + addpath, context->directories.cbuild));
  }
  for (const auto& addpath : controls.delpaths) {
    node[YAML_DELPATH].push_back(FormatPath(context->directories.cprj + "/" + addpath, context->directories.cbuild));
  }
}

void ProjMgrYamlCbuild::SetProcessorNode(YAML::Node node, const map<string, string>& targetAttributes) {
  for (const auto& [rteKey, yamlKey]: RteConstants::DeviceAttributesKeys) {
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
  return ProjMgrUtils::FormatPath(original, directory, m_useAbsolutePaths);
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

bool ProjMgrYamlBase::NeedRebuild(const string& filename, const YAML::Node& newFile) {
  if (!RteFsUtils::Exists(filename) || !RteFsUtils::IsRegularFile(filename)) {
    return false;
  }
  const YAML::Node& oldFile = YAML::LoadFile(filename);
  if (newFile[YAML_BUILD].IsDefined()) {
    // cbuild.yml
    if (newFile[YAML_BUILD][YAML_COMPILER].IsDefined() && oldFile[YAML_BUILD][YAML_COMPILER].IsDefined()) {
      // compare compiler
      return !CompareNodes(newFile[YAML_BUILD][YAML_COMPILER], oldFile[YAML_BUILD][YAML_COMPILER]);
    }
  } else if (newFile[YAML_BUILD_IDX].IsDefined()) {
    // cbuild-idx.yml
    if (newFile[YAML_BUILD_IDX][YAML_CBUILDS].IsDefined() && oldFile[YAML_BUILD_IDX][YAML_CBUILDS].IsDefined()) {
      // compare cbuilds
      size_t size = newFile[YAML_BUILD_IDX][YAML_CBUILDS].size();
      if (size != oldFile[YAML_BUILD_IDX][YAML_CBUILDS].size()) {
        return true;
      }
      for (size_t i = 0; i < size; i++) {
        if (!CompareNodes(newFile[YAML_BUILD_IDX][YAML_CBUILDS][i][YAML_CBUILD],
          oldFile[YAML_BUILD_IDX][YAML_CBUILDS][i][YAML_CBUILD])) {
          return true;
        }
      }
    }
  }
  return false;
}

bool ProjMgrYamlBase::WriteFile(YAML::Node& rootNode, const std::string& filename, const std::string& context, bool allowUpdate) {
  // Compare yaml contents
  if (RteFsUtils::IsDirectory(filename)) {
    ProjMgrLogger::Get().Error("file cannot be written", context, filename);
    return false;
  }
  else if (rootNode.size() == 0) {
    // Remove file as nothing to write.
    if (RteFsUtils::Exists(filename)) {
      RteFsUtils::RemoveFile(filename);
      ProjMgrLogger::Get().Info("file has been removed", context, filename);
    } else {
      ProjMgrLogger::Get().Info("file skipped", context, filename);
    }
  }
  else if (!CompareFile(filename, rootNode)) {
    if (!allowUpdate) {
      ProjMgrLogger::Get().Error("file not allowed to be updated", context, filename);
      return false;
    }
    if (!RteFsUtils::MakeSureFilePath(filename)) {
      ProjMgrLogger::Get().Error("destination directory cannot be created", context, filename);
      return false;
    }
    ofstream fileStream(filename);
    if (!fileStream) {
      ProjMgrLogger::Get().Error("file cannot be written", context, filename);
      return false;
    }
    YAML::Emitter emitter;
    emitter.SetNullFormat(YAML::EmptyNull);
    emitter << rootNode;
    fileStream << emitter.c_str();
    fileStream << endl;
    fileStream << flush;
    fileStream.close();
    ProjMgrLogger::Get().Info("file generated successfully", context, filename);

    // Check generated file schema
    if (m_checkSchema && !ProjMgrYamlSchemaChecker().Validate(filename)) {
      return false;
    }
  }
  else {
    ProjMgrLogger::Get().Info("file is already up-to-date", context, filename);
  }
  return true;
}

bool ProjMgrYamlEmitter::GenerateCbuildIndex(ProjMgrParser& parser, ProjMgrWorker& worker,
  const vector<ContextItem*>& contexts, const string& outputDir, const set<string>& failedContexts,
  const map<string, ExecutesItem>& executes, bool checkSchema) {

  // generate cbuild-idx.yml
  const string& directory = outputDir.empty() ? parser.GetCsolution().directory : RteFsUtils::AbsolutePath(outputDir).generic_string();
  const string& filename = directory + "/" + parser.GetCsolution().name + ".cbuild-idx.yml";

  YAML::Node rootNode;
  ProjMgrYamlCbuildIdx cbuild(
    rootNode[YAML_BUILD_IDX], contexts, parser, worker, directory, failedContexts, executes, checkSchema);

  // set rebuild flags
  if (cbuild.NeedRebuild(filename, rootNode)) {
    rootNode[YAML_BUILD_IDX][YAML_REBUILD] = true;
  } else {
    int index = 0;
    for (const auto& context : contexts) {
      if (context->needRebuild) {
        rootNode[YAML_BUILD_IDX][YAML_CBUILDS][index][YAML_REBUILD] = true;
      }
      index++;
    }
  }
  return cbuild.WriteFile(rootNode, filename);
}

bool ProjMgrYamlEmitter::GenerateCbuild(ContextItem* context, bool checkSchema,
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
  } else {
    rootKey = YAML_BUILD_GEN;
    filename = cbuildGenFilename;
  }
  YAML::Node rootNode;
  ProjMgrYamlCbuild cbuild(rootNode[rootKey], context, generatorId, generatorPack, checkSchema, ignoreRteFileMissing);
  RteFsUtils::CreateDirectories(RteFsUtils::ParentPath(filename));
  // get context rebuild flag
  context->needRebuild = cbuild.NeedRebuild(filename, rootNode);
  return cbuild.WriteFile(rootNode, filename, context->name);
}

bool ProjMgrYamlEmitter::GenerateCbuildSet(const std::vector<string> selectedContexts,
  const string& selectedCompiler, const string& cbuildSetFile, bool checkSchema, bool ignoreRteFileMissing)
{
  YAML::Node rootNode;
  ProjMgrYamlCbuild cbuild(rootNode[YAML_CBUILD_SET], selectedContexts, selectedCompiler, checkSchema, ignoreRteFileMissing);

  return cbuild.WriteFile(rootNode, cbuildSetFile);
}

ProjMgrYamlCbuild::ProjMgrYamlCbuild(YAML::Node node, const vector<ContextItem*>& siblings,
  const string& type, const string& output, const string& gendir, bool checkSchema) : ProjMgrYamlBase(true, checkSchema), m_ignoreRteFileMissing(false)
{
  // construct cbuild-gen-idx.yml content
  SetNodeValue(node[YAML_GENERATED_BY], ORIGINAL_FILENAME + string(" version ") + VERSION_STRING);
  const auto& context = siblings.front();
  const auto& generator = context->extGen.begin()->second;
  YAML::Node generatorNode;
  SetNodeValue(generatorNode[YAML_ID], generator.id);
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
    const auto& siblingGenerator = sibling->extGen.begin()->second;
    SetNodeValue(cbuildGenNode[YAML_NAME], FormatPath(siblingGenerator.name, output));
    SetNodeValue(cbuildGenNode[YAML_MAP], siblingGenerator.map);
    generatorNode[YAML_CBUILD_GENS].push_back(cbuildGenNode);
  }
  node[YAML_GENERATORS].push_back(generatorNode);
}

bool ProjMgrYamlEmitter::GenerateCbuildGenIndex(ProjMgrParser& parser, const vector<ContextItem*> siblings,
  const string& type, const string& output, const string& gendir, bool checkSchema) {
  // generate cbuild-gen-idx.yml as input for external generator
  RteFsUtils::CreateDirectories(output);
  const string& filename = output + "/" + parser.GetCsolution().name + ".cbuild-gen-idx.yml";

  YAML::Node rootNode;
  ProjMgrYamlCbuild cbuild(rootNode[YAML_BUILD_GEN_IDX], siblings, type, output, gendir, checkSchema);
  return cbuild.WriteFile(rootNode, filename);
}

bool ProjMgrYamlEmitter::GenerateCbuildPack(ProjMgrParser& parser, const vector<ContextItem*> contexts, bool keepExistingPackContent, bool cbuildPackFrozen, bool checkSchema) {
  // generate cbuild-pack.yml
  const string& filename = parser.GetCsolution().directory + "/" + parser.GetCsolution().name + ".cbuild-pack.yml";

  YAML::Node rootNode;
  ProjMgrYamlCbuildPack cbuildPack(rootNode[YAML_CBUILD_PACK], contexts, parser, keepExistingPackContent, checkSchema);

  return cbuildPack.WriteFile(rootNode, filename, "", !cbuildPackFrozen);
}
