/*
 * Copyright (c) 2020-2025 Arm Limited. All rights reserved.
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
  void SetResourcesNode(YAML::Node node, const SystemResourcesType& systemResources);
  void SetDebuggersNode(YAML::Node node, const std::vector<DebuggerType>& debuggers);
  void SetDebugVarsNode(YAML::Node node, const DebugVarsType& debugVars);
  void SetDebugSequencesNode(YAML::Node node, const std::vector<DebugSequencesType>& algorithms);
  void SetDebugSequencesBlockNode(YAML::Node node, const std::vector<DebugSequencesBlockType>& blocks);
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
  SetResourcesNode(node[YAML_SYSTEM_RESOURCES], debugRun.systemResources);
  SetDebuggersNode(node[YAML_DEBUGGER], debugRun.debuggers);
  SetDebugVarsNode(node[YAML_DEBUG_VARS], debugRun.debugVars);
  SetDebugSequencesNode(node[YAML_DEBUG_SEQUENCES], debugRun.debugSequences);
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
    SetNodeValue(algorithmNode[YAML_PNAME], item.pname);
    node.push_back(algorithmNode);
  }
}

void ProjMgrCbuildRun::SetFilesNode(YAML::Node node, const std::vector<FilesType>& files) {
  for (const auto& item : files) {
    YAML::Node fileNode;
    SetNodeValue(fileNode[YAML_FILE], FormatPath(item.file, m_directory));
    SetNodeValue(fileNode[YAML_TYPE], item.type);
    SetNodeValue(fileNode[YAML_PNAME], item.pname);
    node.push_back(fileNode);
  }
}

void ProjMgrCbuildRun::SetResourcesNode(YAML::Node node, const SystemResourcesType& systemResources) {
  for (const auto& item : systemResources.memories) {
    YAML::Node memoryNode;
    SetNodeValue(memoryNode[YAML_NAME], item.name);
    SetNodeValue(memoryNode[YAML_ACCESS], item.access);
    if (item.size > 0) {
      SetNodeValue(memoryNode[YAML_START], ProjMgrUtils::ULLToHex(item.start));
      SetNodeValue(memoryNode[YAML_SIZE], ProjMgrUtils::ULLToHex(item.size));
    }
    if (item.bDefault) {
      memoryNode[YAML_DEFAULT] = true;
    }
    if (item.bStartup) {
      memoryNode[YAML_STARTUP] = true;
    }
    SetNodeValue(memoryNode[YAML_PNAME], item.pname);
    if (item.bUninit) {
      memoryNode[YAML_UNINIT] = true;
    }
    SetNodeValue(memoryNode[YAML_ALIAS], item.alias);
    SetNodeValue(memoryNode[YAML_FROM_PACK], item.fromPack);
    node[YAML_MEMORY].push_back(memoryNode);
  }
}

void ProjMgrCbuildRun::SetDebuggersNode(YAML::Node node, const std::vector<DebuggerType>& debuggers) {
  for (const auto& item : debuggers) {
    YAML::Node debuggerNode;
    SetNodeValue(debuggerNode[YAML_NAME], item.name);
    SetNodeValue(debuggerNode[YAML_INFO], item.info);
    SetNodeValue(debuggerNode[YAML_PORT], item.port);
    debuggerNode[YAML_CLOCK] = item.clock;
    SetNodeValue(debuggerNode[YAML_DBGCONF], FormatPath(item.dbgconf, m_directory));
    node.push_back(debuggerNode);
  }
}

void ProjMgrCbuildRun::SetDebugVarsNode(YAML::Node node, const DebugVarsType& debugVars) {
  if (!debugVars.vars.empty()) {
    SetNodeValue(node[YAML_VARS], "|\n" + debugVars.vars);
  }
}

void ProjMgrCbuildRun::SetDebugSequencesNode(YAML::Node node, const std::vector<DebugSequencesType>& sequences) {
  for (const auto& sequence : sequences) {
    YAML::Node sequenceNode;
    SetNodeValue(sequenceNode[YAML_NAME], sequence.name);
    SetNodeValue(sequenceNode[YAML_INFO], sequence.info);
    SetDebugSequencesBlockNode(sequenceNode[YAML_BLOCKS], sequence.blocks);
    SetNodeValue(sequenceNode[YAML_PNAME], sequence.pname);
    node.push_back(sequenceNode);
  }
}

void ProjMgrCbuildRun::SetDebugSequencesBlockNode(YAML::Node node, const std::vector<DebugSequencesBlockType>& blocks) {
  for (const auto& block : blocks) {
    YAML::Node blockNode;
    SetNodeValue(blockNode[YAML_INFO], block.info);
    SetNodeValue(blockNode[YAML_IF], block.control_if);
    SetNodeValue(blockNode[YAML_WHILE], block.control_while);
    if (!block.timeout.empty()) {
      blockNode[YAML_TIMEOUT] = RteUtils::StringToULL(block.timeout);
    }
    if (block.bAtomic) {
      blockNode[YAML_ATOMIC] = YAML::Null;
    }
    if (!block.execute.empty()) {
      SetNodeValue(blockNode[YAML_EXECUTE], "|\n" + block.execute);
    }
    SetDebugSequencesBlockNode(blockNode[YAML_BLOCKS], block.blocks);
    node.push_back(blockNode);
  }
}

//-- ProjMgrYamlEmitter::GenerateCbuildRun --------------------------------------------------------
bool ProjMgrYamlEmitter::GenerateCbuildRun(const RunDebugType& debugRun) {
  // generate cbuild-run.yml
  const string& filename = m_outputDir + "/" + debugRun.solutionName + "+" +
    debugRun.targetType + ".cbuild-run.yml";
  YAML::Node rootNode;
  ProjMgrCbuildRun cbuildRun(rootNode[YAML_CBUILD_RUN], debugRun, m_outputDir);
  return WriteFile(rootNode, filename);
}
