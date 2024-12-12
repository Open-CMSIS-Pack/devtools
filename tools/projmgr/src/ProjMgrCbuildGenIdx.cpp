/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProductInfo.h"
#include "ProjMgrCbuildBase.h"
#include "ProjMgrYamlEmitter.h"
#include "ProjMgrYamlParser.h"
#include "RteFsUtils.h"

using namespace std;

// cbuild-gen-idx
class ProjMgrCbuildGenIdx : public ProjMgrCbuildBase {
public:
  ProjMgrCbuildGenIdx(YAML::Node node, const vector<ContextItem*>& siblings, const string& type, const string& output, const string& gendir);
};

ProjMgrCbuildGenIdx::ProjMgrCbuildGenIdx(YAML::Node node, const vector<ContextItem*>& siblings,
  const string& type, const string& output, const string& gendir) : ProjMgrCbuildBase(true)
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

//-- ProjMgrYamlEmitter::GenerateCbuildGenIndex ---------------------------------------------------
bool ProjMgrYamlEmitter::GenerateCbuildGenIndex(const vector<ContextItem*> siblings,
  const string& type, const string& output, const string& gendir) {
  // generate cbuild-gen-idx.yml as input for external generator
  RteFsUtils::CreateDirectories(output);
  const string& filename = output + "/" + m_parser->GetCsolution().name + ".cbuild-gen-idx.yml";

  YAML::Node rootNode;
  ProjMgrCbuildGenIdx cbuild(rootNode[YAML_BUILD_GEN_IDX], siblings, type, output, gendir);
  return WriteFile(rootNode, filename);
}
