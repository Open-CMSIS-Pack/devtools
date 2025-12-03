/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProductInfo.h"
#include "ProjMgrLogger.h"
#include "ProjMgrYamlEmitter.h"
#include "ProjMgrYamlParser.h"
#include "ProjMgrYamlSchemaChecker.h"
#include "ProjMgrWorker.h"
#include "RteFsUtils.h"

#include <filesystem>
#include <fstream>

using namespace std;

ProjMgrYamlEmitter::ProjMgrYamlEmitter(ProjMgrParser* parser, ProjMgrWorker* worker) :
  m_parser(parser),
  m_worker(worker),
  m_checkSchema(false) {
  // Reserved
}

ProjMgrYamlEmitter::~ProjMgrYamlEmitter(void) {
  // Reserved
}

void ProjMgrYamlEmitter::SetCheckSchema(bool checkSchema) {
  m_checkSchema = checkSchema;
}

void ProjMgrYamlEmitter::SetOutputDir(const std::string& outputDir) {
  m_outputDir = outputDir;
}

bool ProjMgrYamlEmitter::WriteFile(YAML::Node& rootNode, const std::string& filename, const std::string& context, bool allowUpdate) {
  // Compare yaml contents
  if (RteFsUtils::IsDirectory(filename)) {
    ProjMgrLogger::Get().Error("file cannot be written", context, filename);
    return false;
  }
  else if (rootNode.size() == 0) {
    // Remove file as nothing to write.
    if (RteFsUtils::Exists(filename)) {
      RteFsUtils::RemoveFile(filename);
      ProjMgrLogger::Get().Info("file has been removed", context, filename);
    }
    else {
      ProjMgrLogger::Get().Info("file skipped", context, filename);
    }
  }
  else if (!CompareFile(filename, rootNode)) {
    if (!allowUpdate) {
      ProjMgrLogger::Get().Error("file not allowed to be updated", context, filename);
      return false;
    }
    if (!RteFsUtils::MakeSureFilePath(filename)) {
      ProjMgrLogger::Get().Error("destination directory cannot be created", context, filename);
      return false;
    }
    ofstream fileStream(filename);
    if (!fileStream) {
      ProjMgrLogger::Get().Error("file cannot be written", context, filename);
      return false;
    }
    YAML::Emitter emitter;
    emitter.SetNullFormat(YAML::EmptyNull);
    emitter.SetIntBase(YAML::Hex);
    emitter << rootNode;
    fileStream << emitter.c_str();
    fileStream << endl;
    fileStream << flush;
    fileStream.close();
    ProjMgrLogger::Get().Info("file generated successfully", context, filename);

    // Check generated file schema
    if (m_checkSchema && !ProjMgrYamlSchemaChecker().Validate(filename)) {
      return false;
    }
  }
  else {
    ProjMgrLogger::Get().Info("file is already up-to-date", context, filename);
  }
  return true;
}

bool ProjMgrYamlEmitter::CompareFile(const string& filename, const YAML::Node& rootNode) {
  string inBuffer;
  if (!RteFsUtils::Exists(filename) || !RteFsUtils::ReadFile(filename, inBuffer)) {
    return false;
  }
  YAML::Emitter emitter;
  const auto& outBuffer = string((emitter << rootNode).c_str()) + '\n';
  return ProjMgrUtils::NormalizeLineEndings(inBuffer) ==
    ProjMgrUtils::NormalizeLineEndings(outBuffer);
}

bool ProjMgrYamlEmitter::CompareNodes(const YAML::Node& lhs, const YAML::Node& rhs) {
  YAML::Emitter lhsEmitter, rhsEmitter;
  string lhsData, rhsData;

  // convert nodes into string
  lhsEmitter << lhs;
  rhsEmitter << rhs;

  // remove generated-by node from the string
  lhsData = lhsEmitter.c_str();
  rhsData = rhsEmitter.c_str();

  return (lhsData == rhsData) ? true : false;
}

bool ProjMgrYamlEmitter::NeedRebuild(const string& filename, const YAML::Node& newFile) {
  if (!RteFsUtils::Exists(filename) || !RteFsUtils::IsRegularFile(filename)) {
    return false;
  }
  const YAML::Node& oldFile = YAML::LoadFile(filename);
  if (newFile[YAML_BUILD].IsDefined()) {
    // cbuild.yml
    if (newFile[YAML_BUILD][YAML_COMPILER].IsDefined() && oldFile[YAML_BUILD][YAML_COMPILER].IsDefined()) {
      // compare compiler
      return !CompareNodes(newFile[YAML_BUILD][YAML_COMPILER], oldFile[YAML_BUILD][YAML_COMPILER]);
    }
  }
  else if (newFile[YAML_BUILD_IDX].IsDefined()) {
    // cbuild-idx.yml
    if (newFile[YAML_BUILD_IDX][YAML_CBUILDS].IsDefined() && oldFile[YAML_BUILD_IDX][YAML_CBUILDS].IsDefined()) {
      // compare cbuilds
      size_t size = newFile[YAML_BUILD_IDX][YAML_CBUILDS].size();
      if (size != oldFile[YAML_BUILD_IDX][YAML_CBUILDS].size()) {
        return true;
      }
      for (size_t i = 0; i < size; i++) {
        if (!CompareNodes(newFile[YAML_BUILD_IDX][YAML_CBUILDS][i][YAML_CBUILD],
          oldFile[YAML_BUILD_IDX][YAML_CBUILDS][i][YAML_CBUILD])) {
          return true;
        }
      }
    }
  }
  return false;
}
