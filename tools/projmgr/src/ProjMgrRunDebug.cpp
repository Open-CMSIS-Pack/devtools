/*
 * Copyright (c) 2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrRunDebug.h"
#include "ProjMgrWorker.h"
#include "ProjMgrYamlEmitter.h"
#include "RteFsUtils.h"

#include <regex>

using namespace std;

ProjMgrRunDebug::ProjMgrRunDebug(void) {
  // Reserved
}

ProjMgrRunDebug::~ProjMgrRunDebug(void) {
  // Reserved
}

bool ProjMgrRunDebug::CollectSettings(const vector<ContextItem*>& contexts) {

  // get target settings
  const auto& context = contexts.front();
  m_runDebug.solution = context->csolution->path;
  m_runDebug.targetType = context->type.target;
  m_runDebug.compiler = context->compiler;
  if (!context->device.empty()) {
    m_runDebug.device = context->deviceItem.vendor + "::" + context->deviceItem.name;
  }
  if (!context->board.empty()) {
    m_runDebug.board = context->boardItem.vendor + "::" + context->boardItem.name +
      (context->boardItem.revision.empty() ? "" : ":" + context->boardItem.revision);
  }

  // programming algorithms
  vector<pair<const RteItem*, vector<string>>> algorithms;
  // debug infos
  vector<pair<const RteItem*, vector<string>>> debugs;
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
      const auto& deviceDebugs = context->rteDevice->GetEffectiveProperties("debug", pname);
      for (const auto& deviceDebug : deviceDebugs) {
        PushBackUniquely(debugs, deviceDebug, pname);
      }
      const auto& deviceDebugSequences = context->rteDevice->GetEffectiveProperties("sequence", pname);
      for (const auto& deviceDebugSequence : deviceDebugSequences) {
        PushBackUniquely(debugSequences, deviceDebugSequence, pname);
      }
    }
  }

  // board collections
  if (context->boardPack) {
    m_runDebug.boardPack = context->boardPack->GetPackageID(true);
    Collection<RteItem*> boardAlgorithms;
    context->rteBoard->GetChildrenByTag("algorithm", boardAlgorithms);
    for (const auto& boardAlgorithm : boardAlgorithms) {
      PushBackUniquely(algorithms, boardAlgorithm, boardAlgorithm->GetAttribute("Pname"));
    }
  }

  // sort collections starting with specific pnames
  for (auto vec : { &algorithms, &debugs, &debugSequences }) {
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

  // additional programming algorithms
  for (const auto& memory : context->memory) {
    AlgorithmType item;
    item.algorithm = memory.algorithm;
    item.start = RteUtils::StringToULL(memory.start);
    item.size = RteUtils::StringToULL(memory.size);
    m_runDebug.algorithms.push_back(item);
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
  for (const auto& c : contexts) {
    auto output = c->outputTypes;
    if (output.elf.on) {
      RteFsUtils::NormalizePath(output.elf.filename, c->directories.cprj + '/' + c->directories.outdir);
      m_runDebug.outputs.push_back({ output.elf.filename, RteConstants::OUTPUT_TYPE_ELF });
    }
    if (output.hex.on) {
      RteFsUtils::NormalizePath(output.hex.filename, c->directories.cprj + '/' + c->directories.outdir);
      m_runDebug.outputs.push_back({ output.hex.filename, RteConstants::OUTPUT_TYPE_HEX });
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

  return true;
}

void ProjMgrRunDebug::GetDebugSequenceBlock(const RteItem* item, DebugSequencesBlockType& block) {
  // get 'block' attributes
  if (item->GetTag() == "block") {
    block.info = block.info.empty() ? item->GetAttribute("info") : block.info;
    block.atomic = item->GetAttributeAsBool("atomic");
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
