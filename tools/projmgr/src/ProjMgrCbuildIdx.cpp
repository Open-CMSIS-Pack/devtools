/*
 * Copyright (c) 2020-2025 Arm Limited. All rights reserved.
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

// cbuild-idx
class ProjMgrCbuildIdx : public ProjMgrCbuildBase {
public:
  ProjMgrCbuildIdx(
    YAML::Node node, const vector<ContextItem*>& processedContexts,
    ProjMgrParser* parser, ProjMgrWorker* worker, const string& directory, const string& cbuildRun, const set<string>& failedContexts,
    const map<string, ExecutesItem>& executes);
private:
  void SetExecutesNode(YAML::Node node, const map<string, ExecutesItem>& executes, const string& base, const string& ref);
  void SetVariablesNode(YAML::Node node, ProjMgrParser* parser, const ContextItem* context, const VariablesConfiguration& configuration);
};

ProjMgrCbuildIdx::ProjMgrCbuildIdx(YAML::Node node,
  const vector<ContextItem*>& processedContexts, ProjMgrParser* parser, ProjMgrWorker* worker, const string& directory, const string& cbuildRun,
  const set<string>& failedContexts, const map<string, ExecutesItem>& executes) : ProjMgrCbuildBase(false) {
  error_code ec;
  SetNodeValue(node[YAML_GENERATED_BY], ORIGINAL_FILENAME + string(" version ") + VERSION_STRING);
  if (!processedContexts.empty()) {
    const auto& context = processedContexts.front();
    SetNodeValue(node[YAML_DESCRIPTION], context->csolution->description);
    if (!parser->GetCdefault().path.empty() && !context->imageOnly && !context->westOn) {
      SetNodeValue(node[YAML_CDEFAULT], FormatPath(parser->GetCdefault().path, directory));
    }
  }
  SetNodeValue(node[YAML_CSOLUTION], FormatPath(parser->GetCsolution().path, directory));
  if (!cbuildRun.empty()) {
    SetNodeValue(node[YAML_CBUILD_RUN], FormatPath(cbuildRun, directory));
  }
  SetNodeValue(node[YAML_OUTPUT_TMPDIR], FormatPath(parser->GetCsolution().directories.tmpdir, directory));

  // Image Only flag
  bool imageOnlySolution = true;
  for (const auto& processedContext : processedContexts) {
    if (!processedContext->imageOnly) {
      imageOnlySolution = false;
      break;
    }
  }
  if (imageOnlySolution) {
    node[YAML_IMAGE_ONLY] = true;
  }

  // Generate layer info for each target
  vector<string> configTargets;
  for (const auto& context : processedContexts) {
    // Retrieve layer information for a single target exclusively
    if (std::find(configTargets.begin(), configTargets.end(), context->type.target) != configTargets.end()) {
      continue;
    }

    // Collect layer connection info specific to each target
    if (!context->variablesConfigurations.empty()) {
      configTargets.push_back(context->type.target);
      YAML::Node targetTypeNode;
      SetNodeValue(targetTypeNode[YAML_TARGETTYPE], context->type.target);
      for (const auto& configuration : context->variablesConfigurations) {
        YAML::Node configurationsNode;
        configurationsNode[YAML_CONFIGURATION] = YAML::Null;
        SetVariablesNode(configurationsNode[YAML_VARIABLES], parser, context, configuration);
        targetTypeNode[YAML_TARGET_CONFIGURATIONS].push_back(configurationsNode);
      }
      node[YAML_CONFIGURATIONS].push_back(targetTypeNode);
    }
  }

  // Generate select-compiler info
  if (worker->GetSelectableCompilers().size() > 0) {
    for (const auto& selectableCompiler : worker->GetSelectableCompilers()) {
      YAML::Node selectCompilerNode;
      SetNodeValue(selectCompilerNode[YAML_COMPILER], selectableCompiler);
      node[YAML_SELECT_COMPILER].push_back(selectCompilerNode);
    }
  }

  const auto& cprojects = parser->GetCprojects();
  const auto& csolution = parser->GetCsolution();

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
      if (context->westOn) {
        cbuildNode[YAML_WEST] = true;
      }
      if (context->cproject) {
        if (!context->imageOnly) {
          SetNodeValue(cbuildNode[YAML_PROJECT], context->cproject->name);
        }
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
      const vector<pair<const StrVecMap&, const char*>> messages = {
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

void ProjMgrCbuildIdx::SetVariablesNode(YAML::Node node, ProjMgrParser* parser, const ContextItem* context,
  const VariablesConfiguration& configuration) {
  for (const auto& variable : configuration.variables) {
    YAML::Node layerNode;
    if (variable.clayer.empty()) {
      layerNode[variable.name] = "";
    }
    SetNodeValue(layerNode[variable.name], variable.clayer);
    SetNodeValue(layerNode[YAML_DESCRIPTION], variable.description);
    for (const auto& setting : variable.settings) {
      YAML::Node setNode;
      SetNodeValue(setNode[YAML_SET], setting.set);
      layerNode[YAML_SETTINGS].push_back(setNode);
    }
    SetNodeValue(layerNode[YAML_PATH], variable.path);
    SetNodeValue(layerNode[YAML_FILE], variable.file);
    SetNodeValue(layerNode[YAML_COPY_TO], variable.copyTo);
    node.push_back(layerNode);
  }
}

void ProjMgrCbuildIdx::SetExecutesNode(YAML::Node node, const map<string, ExecutesItem>& executes, const string& base, const string& ref) {
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


//-- ProjMgrYamlEmitter::GenerateCbuildIndex ------------------------------------------------------
bool ProjMgrYamlEmitter::GenerateCbuildIndex(const vector<ContextItem*>& contexts,
  const set<string>& failedContexts, const map<string, ExecutesItem>& executes) {

  // generate cbuild-idx.yml
  const string& filename = m_outputDir + "/" + m_parser->GetCsolution().name + ".cbuild-idx.yml";

  YAML::Node rootNode;
  ProjMgrCbuildIdx cbuild(
    rootNode[YAML_BUILD_IDX], contexts, m_parser, m_worker, m_outputDir, m_cbuildRun, failedContexts, executes);

  // set rebuild flags
  if (NeedRebuild(filename, rootNode)) {
    rootNode[YAML_BUILD_IDX][YAML_REBUILD] = true;
  }
  else {
    int index = 0;
    for (const auto& context : contexts) {
      if (context->needRebuild) {
        rootNode[YAML_BUILD_IDX][YAML_CBUILDS][index][YAML_REBUILD] = true;
      }
      index++;
    }
  }
  return WriteFile(rootNode, filename);
}
