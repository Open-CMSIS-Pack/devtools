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
  void SetDebugTopologyNode(YAML::Node node, const DebugTopologyType& topology);
  void SetProcessorsNode(YAML::Node node, const std::vector<ProcessorType>& processors);
  void SetDebugPortsNode(YAML::Node node, const std::vector<DebugPortType>& debugPorts);
  void SetAccessPortsNode(YAML::Node node, const std::vector<AccessPortType>& accessPorts);
  void SetDatapatchNode(YAML::Node node, const std::vector<DatapatchType>& datapatch);
};

ProjMgrCbuildRun::ProjMgrCbuildRun(YAML::Node node,
  const RunDebugType& debugRun, const std::string& directory) : m_directory(directory) {
  SetNodeValue(node[YAML_GENERATED_BY], ORIGINAL_FILENAME + string(" version ") + VERSION_STRING);
  SetNodeValue(node[YAML_SOLUTION], FormatPath(debugRun.solution, directory));
  SetNodeValue(node[YAML_TARGETTYPE], debugRun.targetType);
  SetNodeValue(node[YAML_COMPILER], debugRun.compiler);
  SetNodeValue(node[YAML_BOARD], debugRun.board);
  SetNodeValue(node[YAML_BOARD_PACK], debugRun.boardPack);
  SetNodeValue(node[YAML_DEVICE], debugRun.device);
  SetNodeValue(node[YAML_DEVICE_PACK], debugRun.devicePack);
  SetFilesNode(node[YAML_OUTPUT], debugRun.outputs);
  SetResourcesNode(node[YAML_SYSTEM_RESOURCES], debugRun.systemResources);
  SetFilesNode(node[YAML_SYSTEM_DESCRIPTIONS], debugRun.systemDescriptions);
  SetDebuggersNode(node[YAML_DEBUGGER], debugRun.debuggers);
  SetDebugVarsNode(node[YAML_DEBUG_VARS], debugRun.debugVars);
  SetDebugSequencesNode(node[YAML_DEBUG_SEQUENCES], debugRun.debugSequences);
  SetProgrammingNode(node[YAML_PROGRAMMING], debugRun.algorithms);
  SetDebugTopologyNode(node[YAML_DEBUG_TOPOLOGY], debugRun.debugTopology);
};

void ProjMgrCbuildRun::SetProgrammingNode(YAML::Node node, const std::vector<AlgorithmType>& algorithms) {
  for (const auto& item : algorithms) {
    YAML::Node algorithmNode;
    SetNodeValue(algorithmNode[YAML_ALGORITHM], FormatPath(item.algorithm, m_directory));
    SetNodeValue(algorithmNode[YAML_START], ProjMgrUtils::ULLToHex(item.start));
    SetNodeValue(algorithmNode[YAML_SIZE], ProjMgrUtils::ULLToHex(item.size));
    SetNodeValue(algorithmNode[YAML_RAM_START], ProjMgrUtils::ULLToHex(item.ram.start));
    SetNodeValue(algorithmNode[YAML_RAM_SIZE], ProjMgrUtils::ULLToHex(item.ram.size));
    SetNodeValue(algorithmNode[YAML_PNAME], item.ram.pname);
    node.push_back(algorithmNode);
  }
}

void ProjMgrCbuildRun::SetFilesNode(YAML::Node node, const std::vector<FilesType>& files) {
  for (const auto& item : files) {
    YAML::Node fileNode;
    SetNodeValue(fileNode[YAML_FILE], FormatPath(item.file, m_directory));
    SetNodeValue(fileNode[YAML_INFO], item.info);
    SetNodeValue(fileNode[YAML_TYPE], item.type);
    SetNodeValue(fileNode[YAML_RUN], item.run);
    SetNodeValue(fileNode[YAML_DEBUG], item.debug);
    SetNodeValue(fileNode[YAML_PNAME], item.pname);
    node.push_back(fileNode);
  }
}

void ProjMgrCbuildRun::SetResourcesNode(YAML::Node node, const SystemResourcesType& systemResources) {
  for (const auto& item : systemResources.memories) {
    YAML::Node memoryNode;
    SetNodeValue(memoryNode[YAML_NAME], item.name);
    SetNodeValue(memoryNode[YAML_ACCESS], item.access);
    SetNodeValue(memoryNode[YAML_START], ProjMgrUtils::ULLToHex(item.start));
    SetNodeValue(memoryNode[YAML_SIZE], ProjMgrUtils::ULLToHex(item.size));
    SetNodeValue(memoryNode[YAML_PNAME], item.pname);
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
    SetNodeValue(debuggerNode[YAML_PROTOCOL], item.protocol);
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
    if (block.execute.empty() && block.blocks.empty()) {
      continue;
    }
    YAML::Node blockNode;
    SetNodeValue(blockNode[YAML_INFO], block.info);
    SetNodeValue(blockNode[YAML_IF], block.control_if);
    SetNodeValue(blockNode[YAML_WHILE], block.control_while);
    if (block.timeout.has_value()) {
      blockNode[YAML_TIMEOUT] = block.timeout.value();
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

void ProjMgrCbuildRun::SetDebugTopologyNode(YAML::Node node, const DebugTopologyType& topology) {
  SetDebugPortsNode(node[YAML_DEBUGPORTS], topology.debugPorts);
  SetProcessorsNode(node[YAML_PROCESSORS], topology.processors);
  if (topology.swj.has_value()) {
    node[YAML_SWJ] = topology.swj.value();
  }
  if (topology.dormant.has_value()) {
    node[YAML_DORMANT] = topology.dormant.value();
  }
  if (!topology.sdf.empty()) {
    SetNodeValue(node[YAML_SDF], FormatPath(topology.sdf, m_directory));
  }
}

void ProjMgrCbuildRun::SetDebugPortsNode(YAML::Node node, const vector<DebugPortType>& debugPorts) {
  for (const auto& dp : debugPorts) {
    YAML::Node dpNode;
    dpNode[YAML_DPID] = dp.dpid;
    if (dp.jtagTapIndex.has_value()) {
      dpNode[YAML_JTAG][YAML_TAPINDEX] = dp.jtagTapIndex.value();
    }
    if (dp.swdTargetSel.has_value()) {
      dpNode[YAML_SWD][YAML_TARGETSEL] = dp.swdTargetSel.value();
    }
    SetAccessPortsNode(dpNode[YAML_ACCESSPORTS], dp.accessPorts);
    node.push_back(dpNode);
  }
}

void ProjMgrCbuildRun::SetAccessPortsNode(YAML::Node node, const vector<AccessPortType>& accessPorts) {
  for (const auto& ap : accessPorts) {
    YAML::Node apNode;
    apNode[YAML_APID] = ap.apid;
    if (ap.index.has_value()) {
      apNode[YAML_INDEX] = ap.index.value();
    }
    if (ap.address.has_value()) {
      SetNodeValue(apNode[YAML_ADDRESS], ProjMgrUtils::ULLToHex(ap.address.value()));
    }
    if (ap.hprot.has_value()) {
      SetNodeValue(apNode[YAML_HPROT], ProjMgrUtils::ULLToHex(ap.hprot.value(), 1));
    }
    if (ap.sprot.has_value()) {
      SetNodeValue(apNode[YAML_SPROT], ProjMgrUtils::ULLToHex(ap.sprot.value(), 1));
    }
    SetDatapatchNode(apNode[YAML_DATAPATCH], ap.datapatch);
    SetAccessPortsNode(apNode[YAML_ACCESSPORTS], ap.accessPorts);
    node.push_back(apNode);
  }
}

void ProjMgrCbuildRun::SetDatapatchNode(YAML::Node node, const vector<DatapatchType>& datapatch) {
  for (const auto& patch : datapatch) {
    YAML::Node patchNode;
    SetNodeValue(patchNode[YAML_ADDRESS], ProjMgrUtils::ULLToHex(patch.address));
    SetNodeValue(patchNode[YAML_VALUE], ProjMgrUtils::ULLToHex(patch.value));
    if (patch.mask.has_value()) {
      SetNodeValue(patchNode[YAML_MASK], ProjMgrUtils::ULLToHex(patch.mask.value()));
    }
    SetNodeValue(patchNode[YAML_TYPE], patch.type);
    SetNodeValue(patchNode[YAML_INFO], patch.info);
    node.push_back(patchNode);
  }
}

void ProjMgrCbuildRun::SetProcessorsNode(YAML::Node node, const vector<ProcessorType>& processors) {
  for (const auto& processor : processors) {
    if (processor.pname.empty() && processor.resetSequence.empty() && !processor.apid.has_value()) {
      continue;
    }
    YAML::Node processorNode;
    SetNodeValue(processorNode[YAML_PNAME], processor.pname);
    if (processor.apid.has_value()) {
      processorNode[YAML_APID] = processor.apid.value();
    }
    SetNodeValue(processorNode[YAML_RESET_SEQUENCE], processor.resetSequence);
    node.push_back(processorNode);
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
