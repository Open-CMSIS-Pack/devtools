/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrExtGenerator.h"
#include "ProjMgrLogger.h"
#include "ProjMgrYamlEmitter.h"
#include "ProjMgrKernel.h"

#include "RteFsUtils.h"

using namespace std;

ProjMgrExtGenerator::ProjMgrExtGenerator(ProjMgrParser* parser) :
  m_parser(parser),
  m_checkSchema(false) {
}

ProjMgrExtGenerator::~ProjMgrExtGenerator() {
  // Reserved
}

void ProjMgrExtGenerator::SetCheckSchema(bool checkSchema) {
  m_checkSchema = checkSchema;
}

bool ProjMgrExtGenerator::IsGlobalGenerator(const string& generatorId) {
  return ProjMgrKernel::Get()->GetExternalGenerator(generatorId) != nullptr;
}

bool ProjMgrExtGenerator::CheckGeneratorId(const string& generatorId, const string& componentId) {
  if (!IsGlobalGenerator(generatorId)) {
    ProjMgrLogger::Error("generator '" + generatorId + "' required by component '" +
      componentId + "' was not found in global register");
    return false;
  }
  return true;
}

const string& ProjMgrExtGenerator::GetGlobalGenDir(const string& generatorId) {
  RteGenerator* g = ProjMgrKernel::Get()->GetExternalGenerator(generatorId);
  return g ? g->GetPathAttribute() : RteUtils::EMPTY_STRING;
}

const string& ProjMgrExtGenerator::GetGlobalGenRunCmd(const string& generatorId) {
  RteGenerator* g = ProjMgrKernel::Get()->GetExternalGenerator(generatorId);
  return g ? g->GetRunAttribute() : RteUtils::EMPTY_STRING;
}

const string& ProjMgrExtGenerator::GetGlobalDescription(const string& generatorId) {
  RteGenerator* g = ProjMgrKernel::Get()->GetExternalGenerator(generatorId);
  return g ? g->GetDescription() : RteUtils::EMPTY_STRING;
}

void ProjMgrExtGenerator::AddUsedGenerator(const string& generatorId, const string& genDir, const string& contextId) {
  m_usedGenerators[generatorId][genDir].push_back(contextId);
}

const GeneratorContextVecMap& ProjMgrExtGenerator::GetUsedGenerators(void) {
  return m_usedGenerators;
}

ClayerItem* ProjMgrExtGenerator::GetGeneratorImport(const string& contextId, bool& success) {
  ContextName context;
  ProjMgrUtils::ParseContextEntry(contextId, context);
  success = true;
  for (const auto& [generator, genDirs] : GetUsedGenerators()) {
    for (const auto& [genDir, contexts] : genDirs) {
      if (find(contexts.begin(), contexts.end(), contextId) != contexts.end()) {
        const string cgenFile = fs::path(genDir).append(context.project + ".cgen.yml").generic_string();
        if (!RteFsUtils::Exists(cgenFile)) {
          ProjMgrLogger::Error(cgenFile, "cgen file was not found, run generator '" + generator + "' for context '" + contextId + "'");
          success = false;
          return nullptr;
        }
        if (!m_parser->ParseClayer(cgenFile, m_checkSchema)) {
          success = false;
          return nullptr;
        }
        return &m_parser->GetClayers().at(cgenFile);
      }
    }
  }
  return nullptr;
}
