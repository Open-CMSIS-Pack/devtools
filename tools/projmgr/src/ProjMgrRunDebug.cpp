/*
 * Copyright (c) 2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrRunDebug.h"
#include "ProjMgrWorker.h"
#include "ProjMgrYamlEmitter.h"
#include "RteFsUtils.h"

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
    m_runDebug.device = context->deviceItem.vendor + "::" + context->deviceItem.name +
      (context->deviceItem.pname.empty() ? "" : ":" + context->deviceItem.pname);
  }
  if (!context->board.empty()) {
    m_runDebug.board = context->boardItem.vendor + "::" + context->boardItem.name +
      (context->boardItem.revision.empty() ? "" : ":" + context->boardItem.revision);
  }

  // programming algorithms
  Collection<RteItem*> algorithms;
  // debug infos
  Collection<RteItem*> debugs;

  // device collections
  if (context->devicePack) {
    m_runDebug.devicePack = context->devicePack->GetPackageID(true);
    const auto& deviceAlgorithms = context->rteDevice->GetEffectiveProperties("algorithm", context->deviceItem.pname);
    for (const auto& deviceAlgorithm : deviceAlgorithms) {
      algorithms.push_back(deviceAlgorithm);
    }
    const auto& deviceDebugs = context->rteDevice->GetEffectiveProperties("debug", context->deviceItem.pname);
    for (const auto& deviceDebug : deviceDebugs) {
      debugs.push_back(deviceDebug);
    }
  }

  // board collections
  if (context->boardPack) {
    m_runDebug.boardPack = context->boardPack->GetPackageID(true);
    context->rteBoard->GetChildrenByTag("algorithm", algorithms);
  }

  // set device/board programming algorithms
  for (const auto& algorithm : algorithms) {
    AlgorithmType item;
    item.algorithm = algorithm->GetOriginalAbsolutePath();
    item.start = algorithm->GetAttributeAsULL("start");
    item.size = algorithm->GetAttributeAsULL("size");
    item.ramStart = algorithm->GetAttributeAsULL("RAMstart");
    item.ramSize = algorithm->GetAttributeAsULL("RAMsize");
    item.bDefault = algorithm->GetAttributeAsBool("default");
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
  for (const auto& debug : debugs) {
    const auto& svd = debug->GetAttribute("svd");
    if (!svd.empty()) {
      FilesType item;
      item.file = debug->GetAbsolutePackagePath() + svd;
      item.type = "svd";
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
  return true;
}
