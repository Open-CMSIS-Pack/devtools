/*
 * Copyright (c) 2024-2025 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrRunDebug.h"
#include "ProjMgrWorker.h"
#include "ProjMgrYamlEmitter.h"
#include "RteFsUtils.h"

#include <regex>

using namespace std;

/**
  * @brief default debugger parameters
 */
static constexpr const char* DEBUGGER_NAME_DEFAULT = "<default>";
static constexpr const char* DEBUGGER_PORT_DEFAULT = "swd";
static unsigned long long DEBUGGER_CLOCK_DEFAULT = 10000000;

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

  // processor names
  map<string, ContextItem*> pnames;
  for (const auto& context : contexts) {
    pnames.emplace(context->deviceItem.pname, context);
  }

  // device collections
  for (const auto& [pname, context] : pnames) {
    if (context->devicePack) {
      m_runDebug.devicePack = context->devicePack->GetPackageID(true);
      const auto& deviceAlgorithms = context->rteDevice->GetEffectiveProperties("algorithm", pname);
      for (const auto& deviceAlgorithm : deviceAlgorithms) {
        PushBackUniquely(algorithms, deviceAlgorithm, pname);
      }
      const auto& deviceMemories = context->rteDevice->GetEffectiveProperties("memory", pname);
      for (const auto& deviceMemory : deviceMemories) {
        PushBackUniquely(memories, deviceMemory, pname);
      }
      const auto& deviceDebugs = context->rteDevice->GetEffectiveProperties("debug", pname);
      for (const auto& deviceDebug : deviceDebugs) {
        PushBackUniquely(debugs, deviceDebug, pname);
      }
      const auto& deviceDebugVars = context->rteDevice->GetEffectiveProperties("debugvars", pname);
      for (const auto& deviceDebugVar : deviceDebugVars) {
        PushBackUniquely(debugvars, deviceDebugVar, pname);
      }
      const auto& deviceDebugSequences = context->rteDevice->GetEffectiveProperties("sequence", pname);
      for (const auto& deviceDebugSequence : deviceDebugSequences) {
        PushBackUniquely(debugSequences, deviceDebugSequence, pname);
      }
    }
  }

  // board collections
  if (context0->boardPack) {
    m_runDebug.boardPack = context0->boardPack->GetPackageID(true);
    Collection<RteItem*> boardAlgorithms;
    context0->rteBoard->GetChildrenByTag("algorithm", boardAlgorithms);
    for (const auto& boardAlgorithm : boardAlgorithms) {
      PushBackUniquely(algorithms, boardAlgorithm, boardAlgorithm->GetAttribute("Pname"));
    }
    Collection<RteItem*> boardMemories;
    context0->rteBoard->GetChildrenByTag("memory", boardMemories);
    for (const auto& boardMemory : boardMemories) {
      PushBackUniquely(memories, boardMemory, boardMemory->GetAttribute("Pname"));
    }
  }

  // sort collections starting with specific pnames
  for (auto vec : { &algorithms, &memories, &debugs, &debugSequences }) {
    sort(vec->begin(), vec->end(), [](auto& left, auto& right) {
      return left.second.size() < right.second.size();
    });
  }

  // set device/board programming algorithms
  for (const auto& [algorithm, pname] : algorithms) {
    AlgorithmType item;
    item.algorithm = algorithm->GetOriginalAbsolutePath();
    item.start = algorithm->GetAttributeAsULL("start");
    item.size = algorithm->GetAttributeAsULL("size");
    item.ramStart = algorithm->GetAttributeAsULL("RAMstart");
    item.ramSize = algorithm->GetAttributeAsULL("RAMsize");
    item.bDefault = algorithm->GetAttributeAsBool("default");
    item.pname = pname.size() == 1 ? pname.front() : "";
    m_runDebug.algorithms.push_back(item);
  }

  // set device/board memories
  for (const auto& [memory, pname] : memories) {
    MemoryType item;
    item.name = memory->GetName();
    item.access = GetAccessAttributes(memory);
    item.alias = memory->GetAlias();
    item.start = memory->GetAttributeAsULL("start");
    item.size = memory->GetAttributeAsULL("size");
    item.bDefault = memory->GetAttributeAsBool("default");
    item.bStartup = memory->GetAttributeAsBool("startup");
    item.bUninit = memory->GetAttributeAsBool("uninit");
    item.fromPack = memory->GetPackageID(true);
    item.pname = pname.size() == 1 ? pname.front() : "";
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
      m_runDebug.algorithms.push_back(algoItem);
    }
  }

  // system descriptions
  for (const auto& [debug, pname] : debugs) {
    const auto& svd = debug->GetAttribute("svd");
    if (!svd.empty()) {
      FilesType item;
      item.file = debug->GetAbsolutePackagePath() + svd;
      item.type = "svd";
      item.pname = pname.size() == 1 ? pname.front() : "";
      m_runDebug.systemDescriptions.push_back(item);
    }
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
  for (const auto& [debugSequence, pname] : debugSequences) {
    DebugSequencesType sequence;
    sequence.name = debugSequence->GetName();
    sequence.info = debugSequence->GetAttribute("info");
    for (const auto& debugSequenceBlock : debugSequence->GetChildren()) {
      DebugSequencesBlockType block;
      GetDebugSequenceBlock(debugSequenceBlock, block);
      sequence.blocks.push_back(block);
    }
    sequence.pname = pname.size() == 1 ? pname.front() : "";
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
  const auto& boardPort = debugProbe ? debugProbe->GetAttribute("debugLink") : "";
  const auto& devicePort = debugConfig ? debugConfig->GetAttribute("default") : "";
  defaultDebugger.port = !boardPort.empty() ? boardPort : !devicePort.empty() ? devicePort : DEBUGGER_PORT_DEFAULT;
  const auto& boardClock = debugProbe ? debugProbe->GetAttributeAsULL("debugClock") : 0;
  const auto& deviceClock = debugConfig ? debugConfig->GetAttributeAsULL("clock") : 0;
  defaultDebugger.clock = boardClock > 0 ? boardClock : deviceClock > 0 ? deviceClock : DEBUGGER_CLOCK_DEFAULT;

  // user defined debugger parameters
  if (context0->debuggers.size() > 0) {
    for (const auto& debugger : context0->debuggers) {
      DebuggerType item;
      item.name = debugger.name.empty() ? defaultDebugger.name : debugger.name;
      item.info = debugger.info;
      item.port = debugger.port.empty() ? defaultDebugger.port : debugger.port;
      item.clock = debugger.clock.empty() ? defaultDebugger.clock : RteUtils::StringToULL(debugger.clock);
      item.dbgconf = debugger.dbgconf.empty() ? defaultDebugger.dbgconf : debugger.dbgconf;
      m_runDebug.debuggers.push_back(item);
    }
  } else {
    m_runDebug.debuggers.push_back(defaultDebugger);
  }

  return true;
}

FilesType ProjMgrRunDebug::SetLoadFromOutput(const ContextItem* context, OutputType output, const string type) {
  FilesType load;
  if (output.on) {
    RteFsUtils::NormalizePath(output.filename, context->directories.cprj + '/' + context->directories.outdir);
    load.file = output.filename;
    load.info = "generate by " + context->name;
    load.type = type;
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
    block.timeout = item->GetAttribute("timeout");
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
