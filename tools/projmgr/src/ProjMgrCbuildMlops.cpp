/*
 * Copyright (c) 2020-2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProductInfo.h"
#include "ProjMgrCbuildBase.h"
#include "ProjMgrUtils.h"
#include "ProjMgrYamlEmitter.h"
#include "ProjMgrYamlParser.h"
#include "RteFsUtils.h"

using namespace std;

// cbuild-mlops
class ProjMgrCbuildMlops : public ProjMgrCbuildBase {
public:
  ProjMgrCbuildMlops(YAML::Node node, const MlopsType& mlops, const std::string& directory);
protected:
  std::string m_directory;

  void SetProcessorNode(YAML::Node node, const MlopsProcessorType& processor);
  void SetNpuNode(YAML::Node node, const MlopsNpuType& npu);
  void SetVelaNode(YAML::Node node, const MlopsVelaType& vela);
  void SetModelNode(YAML::Node node, const MlopsModelType& model);
  void SetRunNode(YAML::Node node, const MlopsRunType& run);
  void SetSimulatorNode(YAML::Node node, const MlopsSimulatorType& simulator);

};

ProjMgrCbuildMlops::ProjMgrCbuildMlops(YAML::Node node,
  const MlopsType& mlops, const std::string& directory) : m_directory(directory) {
  SetNodeValue(node[YAML_DESCRIPTION], mlops.description);
  SetProcessorNode(node[YAML_PROCESSOR], mlops.processor);
  SetNpuNode(node[YAML_NPU], mlops.npu);
  SetVelaNode(node[YAML_VELA], mlops.vela);
  SetModelNode(node[YAML_MODEL], mlops.model);
  SetRunNode(node[YAML_HARDWARE], mlops.hardware);
  SetSimulatorNode(node[YAML_SIMULATOR], mlops.simulator);
};

void ProjMgrCbuildMlops::SetProcessorNode(YAML::Node node, const MlopsProcessorType& processor) {
  SetNodeValue(node[YAML_TYPE], processor.type);
}

void ProjMgrCbuildMlops::SetNpuNode(YAML::Node node, const MlopsNpuType& npu) {
  SetNodeValue(node[YAML_TYPE], npu.type);
  if (!npu.macs.empty()) {
    node[YAML_MACS] = RteUtils::StringToULL(npu.macs);
  }
}

void ProjMgrCbuildMlops::SetVelaNode(YAML::Node node, const MlopsVelaType& vela) {
  if (!vela.ini.empty()) {
    SetNodeValue(node[YAML_INI], FormatPath(vela.ini, m_directory));
  }
  SetNodeValue(node[YAML_OPTIONS], vela.options);
}

void ProjMgrCbuildMlops::SetModelNode(YAML::Node node, const MlopsModelType& model) {
  if (!model.clayer.empty()) {
    SetNodeValue(node[YAML_CLAYER], FormatPath(model.clayer, m_directory));
  }
  SetNodeValue(node[YAML_NAME], model.name);
}

void ProjMgrCbuildMlops::SetRunNode(YAML::Node node, const MlopsRunType& run) {
  SetNodeValue(node[YAML_ACTIVE], run.active);
  if (!run.cbuildRun.empty()) {
    SetNodeValue(node[YAML_CBUILD_RUN], FormatPath(run.cbuildRun, m_directory));
  }
  for (const auto& item : run.output) {
    YAML::Node outputNode;
    if (!item.file.empty()) {
      SetNodeValue(outputNode[YAML_FILE], FormatPath(item.file, m_directory));
    }
    SetNodeValue(outputNode[YAML_TYPE], item.type);
    if (outputNode.size() > 0) {
      node[YAML_OUTPUT].push_back(outputNode);
    }
  }
}

void ProjMgrCbuildMlops::SetSimulatorNode(YAML::Node node, const MlopsSimulatorType& simulator) {
  SetRunNode(node, simulator);
  SetNodeValue(node[YAML_MODEL], simulator.model);
  if (!simulator.configFile.empty()) {
    SetNodeValue(node[YAML_CONFIG_FILE], FormatPath(simulator.configFile, m_directory));
  }
}

//-- ProjMgrYamlEmitter::GenerateMlops --------------------------------------------------------
bool ProjMgrYamlEmitter::GenerateMlops(const MlopsType& mlops) {
  // generate cbuild-mlops.yml
  const string& filename = m_outputDir + "/" + m_parser->GetCsolution().name + ".cbuild-mlops.yml";
  const string& directory = RteFsUtils::ParentPath(filename);

  YAML::Node rootNode;
  ProjMgrCbuildMlops cbuildMlops(rootNode[YAML_CBUILD_MLOPS], mlops, directory);
  return WriteFile(rootNode, filename);
}
