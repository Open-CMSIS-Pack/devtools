/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrExtGenerator.h"
#include "ProjMgrLogger.h"
#include "ProjMgrYamlEmitter.h"

#include "RteFsUtils.h"

using namespace std;

ProjMgrExtGenerator::ProjMgrExtGenerator(ProjMgrParser* parser) :
  m_parser(parser),
  m_checkSchema(false) {
}

ProjMgrExtGenerator::~ProjMgrExtGenerator(void) {
  // Reserved
}

void ProjMgrExtGenerator::SetCheckSchema(bool checkSchema) {
  m_checkSchema = checkSchema;
}

bool ProjMgrExtGenerator::RetrieveGlobalGenerators(void) {
  if (m_globalGeneratorFiles.empty()) {
    // get global generator files
    ProjMgrUtils::GetCompilerRoot(m_compilerRoot);
    StrVec toolchainConfigFiles;
    error_code ec;
    for (auto const& entry : fs::recursive_directory_iterator(m_compilerRoot, ec)) {
      if (entry.path().filename().string().find(".generator.yml") == string::npos) {
        continue;
      }
      m_globalGeneratorFiles.push_back(entry.path().generic_string());
    }
    // parse global generator files
    for (auto const& generatorFile : m_globalGeneratorFiles) {
      if (!m_parser->ParseGlobalGenerator(generatorFile, m_checkSchema)) {
        return false;
      }
    }
    m_globalGenerators = m_parser->GetGlobalGenerators();
  }
  return true;
}

bool ProjMgrExtGenerator::IsGlobalGenerator(const string& generatorId) {
  if (m_globalGenerators.find(generatorId) == m_globalGenerators.end()) {
    return false;
  }
  return true;
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
  return(m_globalGenerators[generatorId].path);
}

const string& ProjMgrExtGenerator::GetGlobalGenRunCmd(const string& generatorId) {
  return(m_globalGenerators[generatorId].run);
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
