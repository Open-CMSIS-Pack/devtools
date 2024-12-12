/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrCbuildBase.h"
#include "ProjMgrYamlEmitter.h"
#include "ProjMgrYamlParser.h"

using namespace std;

// cbuild-pack
class ProjMgrCbuildPack : public ProjMgrCbuildBase {
public:
  ProjMgrCbuildPack(YAML::Node node, const vector<ContextItem*>& processedContexts,
    ProjMgrParser* parser, bool keepExistingPackContent);
};

ProjMgrCbuildPack::ProjMgrCbuildPack(YAML::Node node, const vector<ContextItem*>& processedContexts,
  ProjMgrParser* parser, bool keepExistingPackContent) :
  ProjMgrCbuildBase(false) {

  const auto& csolution = parser->GetCsolution();

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
  std::sort(sortedModel.begin(), sortedModel.end(), [](const pair<string, ModelItem>& item1, const pair<string, ModelItem>& item2) {
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

//-- ProjMgrYamlEmitter::GenerateCbuildPack -------------------------------------------------------
bool ProjMgrYamlEmitter::GenerateCbuildPack(const vector<ContextItem*> contexts,
  bool keepExistingPackContent, bool cbuildPackFrozen) {
  // generate cbuild-pack.yml
  const string& filename = m_parser->GetCsolution().directory + "/" + m_parser->GetCsolution().name + ".cbuild-pack.yml";
  YAML::Node rootNode;
  ProjMgrCbuildPack cbuildPack(rootNode[YAML_CBUILD_PACK], contexts, m_parser, keepExistingPackContent);
  return WriteFile(rootNode, filename, "", !cbuildPackFrozen);
}
