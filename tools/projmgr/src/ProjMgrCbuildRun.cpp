/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
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

// cbuild-run
class ProjMgrCbuildRun : public ProjMgrCbuildBase {
public:
  ProjMgrCbuildRun(YAML::Node node, const RunDebugType& debugRun, const std::string& directory);
protected:
  std::string m_directory;
  void SetProgrammingNode(YAML::Node node, const std::vector<AlgorithmType>& algorithms);
  void SetFilesNode(YAML::Node node, const std::vector<FilesType>& outputs);
};

ProjMgrCbuildRun::ProjMgrCbuildRun(YAML::Node node,
  const RunDebugType& debugRun, const std::string& directory) : m_directory(directory) {
  SetNodeValue(node[YAML_GENERATED_BY], ORIGINAL_FILENAME + string(" version ") + VERSION_STRING);
  SetNodeValue(node[YAML_SOLUTION], FormatPath(debugRun.solution, directory));
  SetNodeValue(node[YAML_TARGETTYPE], debugRun.targetType);
  SetNodeValue(node[YAML_COMPILER], debugRun.compiler);
  SetNodeValue(node[YAML_DEVICE], debugRun.device);
  SetNodeValue(node[YAML_DEVICE_PACK], debugRun.devicePack);
  SetNodeValue(node[YAML_BOARD], debugRun.board);
  SetNodeValue(node[YAML_BOARD_PACK], debugRun.boardPack);
  SetProgrammingNode(node[YAML_PROGRAMMING], debugRun.algorithms);
  SetFilesNode(node[YAML_SYSTEM_DESCRIPTIONS], debugRun.systemDescriptions);
  SetFilesNode(node[YAML_OUTPUT], debugRun.outputs);
};

void ProjMgrCbuildRun::SetProgrammingNode(YAML::Node node, const std::vector<AlgorithmType>& algorithms) {
  for (const auto& item : algorithms) {
    YAML::Node algorithmNode;
    SetNodeValue(algorithmNode[YAML_ALGORITHM], FormatPath(item.algorithm, m_directory));
    if (item.size > 0) {
      SetNodeValue(algorithmNode[YAML_START], ProjMgrUtils::ULLToHex(item.start));
      SetNodeValue(algorithmNode[YAML_SIZE], ProjMgrUtils::ULLToHex(item.size));
    }
    if (item.ramSize > 0) {
      SetNodeValue(algorithmNode[YAML_RAM_START], ProjMgrUtils::ULLToHex(item.ramStart));
      SetNodeValue(algorithmNode[YAML_RAM_SIZE], ProjMgrUtils::ULLToHex(item.ramSize));
    }
    if (item.bDefault) {
      algorithmNode[YAML_DEFAULT] = true;
    }
    node.push_back(algorithmNode);
  }
}

void ProjMgrCbuildRun::SetFilesNode(YAML::Node node, const std::vector<FilesType>& files) {
  for (const auto& item : files) {
    YAML::Node fileNode;
    SetNodeValue(fileNode[YAML_FILE], FormatPath(item.file, m_directory));
    SetNodeValue(fileNode[YAML_TYPE], item.type);
    node.push_back(fileNode);
  }
}

//-- ProjMgrYamlEmitter::GenerateCbuildRun --------------------------------------------------------
bool ProjMgrYamlEmitter::GenerateCbuildRun(const RunDebugType& debugRun) {
  // generate cbuild-run.yml
  const string& filename = m_outputDir + "/" + debugRun.targetType + ".cbuild-run.yml";
  YAML::Node rootNode;
  ProjMgrCbuildRun cbuildRun(rootNode[YAML_CBUILD_RUN], debugRun, m_outputDir);
  return WriteFile(rootNode, filename);
}
