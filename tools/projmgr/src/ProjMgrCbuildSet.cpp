/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProductInfo.h"
#include "ProjMgrCbuildBase.h"
#include "ProjMgrYamlEmitter.h"
#include "ProjMgrYamlParser.h"

using namespace std;

// cbuild-set
class ProjMgrCbuildSet : public ProjMgrCbuildBase {
public:
  ProjMgrCbuildSet(YAML::Node node, const vector<string>& selectedContexts, const string& selectedCompiler, bool ignoreRteFileMissing);
};

ProjMgrCbuildSet::ProjMgrCbuildSet(YAML::Node node, const vector<string>& selectedContexts,
  const string& selectedCompiler, bool ignoreRteFileMissing) {
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

//-- ProjMgrYamlEmitter::GenerateCbuildSet --------------------------------------------------------
bool ProjMgrYamlEmitter::GenerateCbuildSet(const std::vector<string> selectedContexts,
  const string& selectedCompiler, const string& cbuildSetFile, bool ignoreRteFileMissing) {
  YAML::Node rootNode;
  ProjMgrCbuildSet cbuild(rootNode[YAML_CBUILD_SET], selectedContexts, selectedCompiler, ignoreRteFileMissing);
  return WriteFile(rootNode, cbuildSetFile);
}
