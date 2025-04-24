/*
 * Copyright (c) 2024-2025 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrRunDebug.h"
#include "ProjMgrWorker.h"
#include "ProjMgrLogger.h"
#include "ProjMgrYamlEmitter.h"
#include "RteFsUtils.h"

#include <regex>

using namespace std;

/**
  * @brief default debugger parameters
 */
static constexpr const char* DEBUGGER_NAME_DEFAULT = "<default>";

ProjMgrRunDebug::ProjMgrRunDebug(void) {
  // Reserved
}

ProjMgrRunDebug::~ProjMgrRunDebug(void) {
  // Reserved
}

bool ProjMgrRunDebug::CollectSettings(const vector<ContextItem*>& contexts) {

  // get target settings
  const auto& context0 = contexts.front();
  m_runDebug.solutionName = context0->csolution->name;
  m_runDebug.solution = context0->csolution->path;
  m_runDebug.targetType = context0->type.target;
  m_runDebug.compiler = context0->compiler;
  if (!context0->device.empty()) {
    m_runDebug.device = context0->deviceItem.vendor + "::" + context0->deviceItem.name;
  }
  if (!context0->board.empty()) {
    m_runDebug.board = context0->boardItem.vendor + "::" + context0->boardItem.name +
      (context0->boardItem.revision.empty() ? "" : ":" + context0->boardItem.revision);
  }

  // programming algorithms
  vector<pair<const RteItem*, vector<string>>> algorithms;
  // programming memories
  vector<pair<const RteItem*, vector<string>>> memories;
  // debug infos
  vector<pair<const RteItem*, vector<string>>> debugs;
  // debug vars
  vector<pair<const RteItem*, vector<string>>> debugvars;
  // debug sequences
  vector<pair<const RteItem*, vector<string>>> debugSequences;

  // all processors
  const auto& pnames = context0->rteDevice->GetProcessors();

  // device collections
  for (const auto& [pname, _] : pnames) {
    if (context0->devicePack) {
      m_runDebug.devicePack = context0->devicePack->GetPackageID(true);
      const auto& deviceAlgorithms = context0->rteDevice->GetEffectiveProperties("algorithm", pname);
      for (const auto& deviceAlgorithm : deviceAlgorithms) {
        PushBackUniquely(algorithms, deviceAlgorithm, pname);
      }
      const auto& deviceMemories = context0->rteDevice->GetEffectiveProperties("memory", pname);
      for (const auto& deviceMemory : deviceMemories) {
        PushBackUniquely(memories, deviceMemory, pname);
      }
      const auto& deviceDebugs = context0->rteDevice->GetEffectiveProperties("debug", pname);
      for (const auto& deviceDebug : deviceDebugs) {
        PushBackUniquely(debugs, deviceDebug, pname);
      }
      const auto& deviceDebugVars = context0->rteDevice->GetEffectiveProperties("debugvars", pname);
      for (const auto& deviceDebugVar : deviceDebugVars) {
        PushBackUniquely(debugvars, deviceDebugVar, pname);
      }
      const auto& deviceDebugSequences = context0->rteDevice->GetEffectiveProperties("sequence", pname);
      for (const auto& deviceDebugSequence : deviceDebugSequences) {
        PushBackUniquely(debugSequences, deviceDebugSequence, pname);
      }
    }
  }

  // default ramstart/size: use the first memory with default=1 and rwx attribute
  // if not found, use ramstart/size from other algorithm in DFP
  RamType defaultRam;
  for (const auto& [memory, _] : memories) {
    if (memory->GetAttributeAsBool("default") && GetAccessAttributes(memory).find("rwx") == 0) {
      defaultRam.start = memory->GetAttributeAsULL("start");
      defaultRam.size = memory->GetAttributeAsULL("size");
      defaultRam.pname = memory->GetProcessorName();
      break;
    }
  }
  if (defaultRam.size == 0) {    
    for (const auto& [algorithm, _] : algorithms) {
      if (algorithm->HasAttribute("RAMsize")) {
        defaultRam.start = algorithm->GetAttributeAsULL("RAMstart");
        defaultRam.size = algorithm->GetAttributeAsULL("RAMsize");
        defaultRam.pname = algorithm->GetProcessorName();
        break;
      }
    }
    if (defaultRam.size == 0) {
      ProjMgrLogger::Get().Error("no default rwx memory nor algorithm with ramstart/size was found");
      return false;
    }
  }

  // board collections
  if (context0->boardPack) {
    m_runDebug.boardPack = context0->boardPack->GetPackageID(true);
    Collection<RteItem*> boardAlgorithms;
    context0->rteBoard->GetChildrenByTag("algorithm", boardAlgorithms);
    for (const auto& boardAlgorithm : boardAlgorithms) {
      PushBackUniquely(algorithms, boardAlgorithm, boardAlgorithm->GetProcessorName());
    }
    Collection<RteItem*> boardMemories;
    context0->rteBoard->GetChildrenByTag("memory", boardMemories);
    for (const auto& boardMemory : boardMemories) {
      PushBackUniquely(memories, boardMemory, boardMemory->GetProcessorName());
    }
  }

  // sort collections starting with specific pnames
  for (auto vec : { &algorithms, &memories, &debugs, &debugSequences }) {
    sort(vec->begin(), vec->end(), [](auto& left, auto& right) {
      return left.second.size() < right.second.size();
    });
  }

  // set device/board programming algorithms
  for (const auto& [algorithm, _] : algorithms) {
    if (!algorithm->GetAttributeAsBool("default")) {
      continue;
    }
    if (algorithm->HasAttribute("style")) {
      const auto& style = algorithm->GetAttribute("style");
      if (style != "Keil" && style != "CMSIS") {
        continue;
      }
    }  
    AlgorithmType item;
    item.algorithm = algorithm->GetOriginalAbsolutePath();
    item.start = algorithm->GetAttributeAsULL("start");
    item.size = algorithm->GetAttributeAsULL("size");
    if (algorithm->HasAttribute("RAMsize")) {
      item.ram.start = algorithm->GetAttributeAsULL("RAMstart");
      item.ram.size = algorithm->GetAttributeAsULL("RAMsize");
      item.ram.pname = algorithm->GetProcessorName();
    } else {
      item.ram.start = defaultRam.start;
      item.ram.size = defaultRam.size;
      item.ram.pname = defaultRam.pname;
    }
    m_runDebug.algorithms.push_back(item);
  }

  // set device/board memories
  for (const auto& [memory, _] : memories) {
    MemoryType item;
    item.name = memory->GetName();
    item.access = GetAccessAttributes(memory);
    item.alias = memory->GetAlias();
    item.start = memory->GetAttributeAsULL("start");
    item.size = memory->GetAttributeAsULL("size");
    item.fromPack = memory->GetPackageID(true);
    item.pname = memory->GetProcessorName();
    m_runDebug.systemResources.memories.push_back(item);
  }

  // additional user memory items (system resources and programming algorithms)
  for (const auto& memory : context0->memory) {
    MemoryType memItem;
    memItem.name = memory.name;
    memItem.access = memory.access;
    memItem.start = RteUtils::StringToULL(memory.start);
    memItem.size = RteUtils::StringToULL(memory.size);
    m_runDebug.systemResources.memories.push_back(memItem);
    if (!memory.algorithm.empty()) {
      AlgorithmType algoItem;
      algoItem.algorithm = memory.algorithm;
      algoItem.start = memItem.start;
      algoItem.size = memItem.size;
      algoItem.ram.start = defaultRam.start;
      algoItem.ram.size = defaultRam.size;
      algoItem.ram.pname = defaultRam.pname;
      m_runDebug.algorithms.push_back(algoItem);
    }
  }

  // system descriptions
  for (const auto& [debug, _] : debugs) {
    const auto& svd = debug->GetAttribute("svd");
    if (!svd.empty()) {
      FilesType item;
      item.file = debug->GetAbsolutePackagePath() + svd;
      item.type = "svd";
      item.pname = debug->GetProcessorName();
      m_runDebug.systemDescriptions.push_back(item);
    }
  }
  StrVec scvdFiles;
  for (const auto& context : contexts) {
    for (const auto& [scvdFile, _] : context->rteActiveTarget->GetScvdFiles()) {
      CollectionUtils::PushBackUniquely(scvdFiles, scvdFile);
    }
  }
  for (const auto& scvdFile : scvdFiles) {
    FilesType item;
    item.file = scvdFile;
    item.type = "scvd";
    m_runDebug.systemDescriptions.push_back(item);
  }

  // outputs
  for (const auto& context : contexts) {
    // populate load entries from context outputs
    const map<const string, const OutputType> types = {
      { RteConstants::OUTPUT_TYPE_ELF, context->outputTypes.elf },
      { RteConstants::OUTPUT_TYPE_HEX, context->outputTypes.hex },
      { RteConstants::OUTPUT_TYPE_BIN, context->outputTypes.bin },
    };
    for (const auto& [type, output] : types) {
      const auto& load = SetLoadFromOutput(context, output, type);
      if (!load.file.empty()) {
        m_runDebug.outputs.push_back(load);
      }
    }
  }
  for (const auto& context : contexts) {
    // merge/insert user defined load nodes
    for (const auto& item : context->loads) {
      bool merged = false;
      for (auto& output : m_runDebug.outputs) {
        if (output.file == item.file) {
          output.info = output.info.empty() ? item.info : output.info;
          output.type = output.type.empty() ? item.type : output.type;
          output.run = output.run.empty() ? item.run : output.run;
          output.debug = output.debug.empty() ? item.debug : output.debug;
          merged = true;
          break;
        }
      }
      if (!merged) {
        m_runDebug.outputs.push_back({ item.file, item.info, item.type, item.run, item.debug });
      }
    }
  }

  // debug vars
  for (const auto& [debugvar, _] : debugvars) {
    DebugVarsType item;
    const string vars = RteUtils::EnsureLf(debugvar->GetText());
    if (!vars.empty()) {
      item.vars = regex_replace(vars, regex("\n +"), "\n");
      m_runDebug.debugVars = item;
      break;
    }
  }

  // debug sequences
  for (const auto& [debugSequence, _] : debugSequences) {
    DebugSequencesType sequence;
    sequence.name = debugSequence->GetName();
    sequence.info = debugSequence->GetAttribute("info");
    for (const auto& debugSequenceBlock : debugSequence->GetChildren()) {
      DebugSequencesBlockType block;
      GetDebugSequenceBlock(debugSequenceBlock, block);
      sequence.blocks.push_back(block);
    }
    sequence.pname = debugSequence->GetProcessorName();
    m_runDebug.debugSequences.push_back(sequence);
  }

  // default debugger parameters from DFP and BSP
  DebuggerType defaultDebugger;
  defaultDebugger.dbgconf = context0->dbgconf.first;
  const auto& debugConfig = context0->devicePack ?
    context0->rteDevice->GetSingleEffectiveProperty("debugconfig", context0->deviceItem.pname) : nullptr;
  const auto& debugProbe = context0->boardPack ?
    context0->rteBoard->GetItemByTag("debugProbe") : nullptr;
  defaultDebugger.name = debugProbe ? debugProbe->GetName() : DEBUGGER_NAME_DEFAULT;
  const auto& boardProtocol = debugProbe ? debugProbe->GetAttribute("debugLink") : "";
  const auto& deviceProtocol = debugConfig ? debugConfig->GetAttribute("default") : "";
  defaultDebugger.protocol = !boardProtocol.empty() ? boardProtocol : deviceProtocol;
  if (debugProbe && debugProbe->HasAttribute("debugClock")) {
    defaultDebugger.clock = debugProbe->GetAttributeAsULL("debugClock");
  } else if (debugConfig && debugConfig->HasAttribute("clock")) {
    defaultDebugger.clock = debugConfig->GetAttributeAsULL("clock");
  }

  // user defined debugger parameters
  if (context0->debuggers.size() > 0) {
    for (const auto& debugger : context0->debuggers) {
      DebuggerType item;
      item.name = debugger.name.empty() ? defaultDebugger.name : debugger.name;
      item.info = debugger.info;
      item.protocol = debugger.protocol.empty() ? defaultDebugger.protocol : debugger.protocol;
      item.clock = debugger.clock.empty() ? defaultDebugger.clock : RteUtils::StringToULL(debugger.clock);
      item.dbgconf = debugger.dbgconf.empty() ? defaultDebugger.dbgconf : debugger.dbgconf;
      m_runDebug.debuggers.push_back(item);
    }
  } else {
    m_runDebug.debuggers.push_back(defaultDebugger);
  }

  // debug topology
  if (debugConfig) {
    if (debugConfig->HasAttribute("dormant")) {
      m_runDebug.debugTopology.dormant = debugConfig->GetAttributeAsBool("dormant", false);
    }
    if (debugConfig->HasAttribute("swj")) {
      m_runDebug.debugTopology.swj = debugConfig->GetAttributeAsBool("swj", true);
    }
    const auto sdf = debugConfig->GetAttribute("sdf");
    if (!sdf.empty()) {
      m_runDebug.debugTopology.sdf = debugConfig->GetAbsolutePackagePath() + sdf;
    }
  }

  // debug and access ports collections
  map<unsigned int, vector<AccessPortType>> accessPortsMap;
  map<unsigned int, vector<AccessPortType>> accessPortsChildrenMap;
  map<unsigned int, vector<DatapatchType>> datapatchById;
  map<unsigned int, map<unsigned int, vector<DatapatchType>>> datapatchByIndex;
  const auto& accessPortsV1 = context0->rteDevice->GetEffectiveProperties("accessportV1", context0->deviceItem.pname);
  const auto& accessPortsV2 = context0->rteDevice->GetEffectiveProperties("accessportV2", context0->deviceItem.pname);
  const auto& debugPorts = context0->rteDevice->GetEffectiveProperties("debugport", context0->deviceItem.pname);
  const auto& defaultDp = debugPorts.empty() ? 0 : debugPorts.front()->GetAttributeAsInt("__dp");

  // datapatches
  for (const auto& [debug, _] : debugs) {
    Collection<RteItem*> datapatches;
    debug->GetChildrenByTag("datapatch", datapatches);
    for (const auto& datapatch : datapatches) {
      DatapatchType patch;
      patch.address = datapatch->GetAttributeAsULL("address");
      patch.value = datapatch->GetAttributeAsULL("value");
      if (datapatch->HasAttribute("mask")) {
        patch.mask = datapatch->GetAttributeAsULL("mask");
      }
      patch.type = datapatch->GetAttribute("type");
      patch.info = datapatch->GetAttribute("info");
      if (datapatch->HasAttribute("__apid")) {
        datapatchById[datapatch->GetAttributeAsInt("__apid")].push_back(patch);
      } else {
        const auto& dp = datapatch->GetAttributeAsInt("__dp", defaultDp);
        const auto& apIndex = datapatch->GetAttributeAsInt("__ap", debug->GetAttributeAsInt("__ap", 0));
        datapatchByIndex[dp][apIndex].push_back(patch);
      }
    }
  }
  // access ports from 'debug' property (legacy support)
  map<string, unsigned int> processorApMap;
  if (accessPortsV1.empty() && accessPortsV2.empty()) {
    unsigned int uniqueApId = 0;
    for (const auto& [debug, scope] : debugs) {
      if (scope.size() == 1) {
        const auto& pname = scope.front();
        if (pname.empty() && datapatchByIndex.empty() && !debug->HasAttribute("__dp") && !debug->HasAttribute("__ap")) {
          // unnamed core with default attributes, skip further access port discovering
          break;
        }
        // use a sequential unique ap id
        const auto& apid = uniqueApId++;
        // add ap node to access port map
        AccessPortType ap;
        const auto& dp = debug->GetAttributeAsInt("__dp", defaultDp);
        ap.index = debug->GetAttributeAsInt("__ap", 0);
        ap.apid = apid;
        ap.datapatch = datapatchByIndex[dp][ap.index.value()];
        accessPortsMap[dp].push_back(ap);
        // add apid to processor map
        processorApMap[pname] = apid;
      }
    }
  }
  // processors
  for (const auto& [pname, _] : pnames) {
    ProcessorType processor;
    processor.pname = pname;
    for (const auto& [debug, scope] : debugs) {
      if (scope.size() == 1 && scope.front() == pname) {
        if (debug->HasAttribute("__apid")) {
          processor.apid = debug->GetAttributeAsInt("__apid");
        }
        processor.resetSequence = debug->GetAttribute("defaultResetSequence");
      }
    }
    // legacy apid
    if (!processor.apid.has_value() && processorApMap.find(pname) != processorApMap.end()) {
      processor.apid = processorApMap.at(pname);
    }
    // 'punits': placeholder for future expansion
    processor.punits.clear();
    m_runDebug.debugTopology.processors.push_back(processor);
  }
  // APv1
  for (const auto& accessPortV1 : accessPortsV1) {
    AccessPortType ap;
    ap.apid = accessPortV1->GetAttributeAsInt("__apid");
    ap.datapatch = datapatchById[ap.apid];
    if (accessPortV1->HasAttribute("index")) {
      ap.index = accessPortV1->GetAttributeAsInt("index");
    }
    SetProtNodes(accessPortV1, ap);
    auto dp = accessPortV1->GetAttributeAsInt("__dp", defaultDp);
    accessPortsMap[dp].push_back(ap);
  }
  // APv2
  for (const auto& accessPortV2 : accessPortsV2) {
    AccessPortType ap;
    ap.apid = accessPortV2->GetAttributeAsInt("__apid");
    ap.datapatch = datapatchById[ap.apid];
    if (accessPortV2->HasAttribute("address")) {
      ap.address = accessPortV2->GetAttributeAsULL("address");
    }
    SetProtNodes(accessPortV2, ap);
    if (accessPortV2->HasAttribute("parent")) {
      auto parent = accessPortV2->GetAttributeAsInt("parent");
      accessPortsChildrenMap[parent].push_back(ap);
    } else {
      auto dp = accessPortV2->GetAttributeAsInt("__dp", defaultDp);
      accessPortsMap[dp].push_back(ap);
    }
  }
  // construct debug ports tree
  if (debugPorts.empty() && !accessPortsMap.empty()) {
    // default debug port
    m_runDebug.debugTopology.debugPorts.push_back({0});
  }
  for (const auto& debugPort : debugPorts) {
    DebugPortType dp;
    dp.dpid = debugPort->GetAttributeAsInt("__dp");
    auto jtag = debugPort->GetFirstChild("jtag");
    if (jtag && jtag->HasAttribute("tapindex")) {
      dp.jtagTapIndex = jtag->GetAttributeAsInt("tapindex");
    }
    auto swd = debugPort->GetFirstChild("swd");
    if (swd && swd->HasAttribute("targetsel")) {
      dp.swdTargetSel = swd->GetAttributeAsInt("targetsel");
    }
    // add debug port to debug topology
    m_runDebug.debugTopology.debugPorts.push_back(dp);
  }
  for (auto& dp : m_runDebug.debugTopology.debugPorts) {
    // set first level access ports
    if (accessPortsMap.find(dp.dpid) != accessPortsMap.end()) {
      dp.accessPorts = accessPortsMap.at(dp.dpid);
    }
    // add nested children access ports
    SetAccessPorts(dp.accessPorts, accessPortsChildrenMap);
  }

  return true;
}

void ProjMgrRunDebug::SetAccessPorts(vector<AccessPortType>& parent, const map<unsigned int, vector<AccessPortType>>& childrenMap) {
  // set access ports children recursively
  for (auto& ap : parent) {
    if (childrenMap.find(ap.apid) != childrenMap.end()) {
      ap.accessPorts = childrenMap.at(ap.apid);
      SetAccessPorts(ap.accessPorts, childrenMap);
    }
  }
}

void ProjMgrRunDebug::SetProtNodes(const RteDeviceProperty* item, AccessPortType& ap) {
  if (item->HasAttribute("HPROT")) {
    ap.hprot = item->GetAttributeAsInt("HPROT");
  }
  if (item->HasAttribute("SPROT")) {
    ap.sprot = item->GetAttributeAsInt("SPROT");
  }
}

FilesType ProjMgrRunDebug::SetLoadFromOutput(const ContextItem* context, OutputType output, const string type) {
  FilesType load;
  if (output.on) {
    RteFsUtils::NormalizePath(output.filename, context->directories.cprj + '/' + context->directories.outdir);
    load.file = output.filename;
    load.info = "generate by " + context->name;
    load.type = type;
    load.pname = context->deviceItem.pname;
  }
  return load;
}

void ProjMgrRunDebug::GetDebugSequenceBlock(const RteItem* item, DebugSequencesBlockType& block) {
  // get 'block' attributes
  if (item->GetTag() == "block") {
    block.info = block.info.empty() ? item->GetAttribute("info") : block.info;
    block.bAtomic = item->GetAttributeAsBool("atomic");
    const string execute = RteUtils::EnsureLf(item->GetText());
    block.execute = regex_replace(execute, regex("\n +"), "\n");
    // 'block' doesn't have children, stop here
    return;
  }

  // get 'control' attributes
  if (item->GetTag() == "control") {
    block.info = item->GetAttribute("info");
    block.control_if = item->GetAttribute("if");
    block.control_while = item->GetAttribute("while");
    if (item->HasAttribute("timeout")) {
      block.timeout = item->GetAttributeAsInt("timeout");
    }
  }

  const auto& children = item->GetChildren();
  if ((children.size() == 1) && (children.front()->GetTag() == "block")) {
    // last child block
    GetDebugSequenceBlock(children.front(), block);
    return;
  }

  for (const auto& child : children) {
    // get children blocks recursively
    DebugSequencesBlockType childBlock;
    GetDebugSequenceBlock(child, childBlock);
    block.blocks.push_back(childBlock);
  }
}

void ProjMgrRunDebug::PushBackUniquely(vector<pair<const RteItem*, vector<string>>>& vec, const RteItem* item, const string pname) {
  for (auto& [rteItem, pnames] : vec) {
    if (rteItem == item) {
      CollectionUtils::PushBackUniquely(pnames, pname);
      return;
    }
  }
  vec.push_back({ item, { pname } });
}

string ProjMgrRunDebug::GetAccessAttributes(const RteItem* mem)
{
  string access = mem->GetAccess();
  if (access.empty()) {
    RteItem m = *mem;
    access = string(m.IsReadAccess() ? "r" : "") + (m.IsWriteAccess() ? "w" : "") + (m.IsExecuteAccess() ? "x" : "");
  }
  return access;
}
