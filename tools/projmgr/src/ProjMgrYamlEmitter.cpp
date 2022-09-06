/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrLogger.h"
#include "ProjMgrYamlEmitter.h"
#include "ProjMgrUtils.h"
#include "RteFsUtils.h"
#include "RteItem.h"

#include "yaml-cpp/yaml.h"

#include <filesystem>
#include <fstream>

using namespace std;

ProjMgrYamlEmitter::ProjMgrYamlEmitter(void) {
  // Reserved
}

ProjMgrYamlEmitter::~ProjMgrYamlEmitter(void) {
  // Reserved
}

optional<string> ProjMgrYamlEmitter::EmitContextInfo(const ContextItem& context, const string& destinationPath) {
  typedef map <string, string> StringMap;
  typedef vector<StringMap> StringMapVector;
  
  StringMap infoMap;
  const StringMap origin = {
    {"solution",    context.csolution->path},
    {"project",     context.cproject->path},
    {"build-type",  context.type.build},
    {"target-type", context.type.target},
    {"board",       context.board},
    {"device",      context.device}
  };
  for (const auto& [key, value] : origin) {
    if (!value.empty()) {
      infoMap.insert({ key, value });
    }
  }

  StringMapVector componentVector;
  const string& packRoot = ProjMgrKernel::Get()->GetCmsisPackRoot();
  for (const auto& [componentId, component] : context.components) {
    componentVector.push_back({
      {"component", componentId},
      {"pack", ProjMgrUtils::GetPackageID(component.first->GetPackage())},
      {"pack-path", fs::absolute(packRoot + "/" + component.first->GetPackagePath()).generic_string()},
    });
  }

  YAML::Node rootNode;
  rootNode["context"] = infoMap;
  rootNode["context"]["components"] = componentVector;
  rootNode["destination"] = destinationPath;
  YAML::Emitter emitter;
  emitter << rootNode;

  // Calculate output generator input file path
  string generatorTmpWorkingDir = context.directories.outdir;  // Use out build folder by default since the generator input file is a temporary file
  if (!generatorTmpWorkingDir.empty()) {
    // Outdir may be relative, if so, add project path to it
    if (fs::path(generatorTmpWorkingDir).is_relative()) {
      string projectPath = context.rteActiveProject->GetProjectPath();
      generatorTmpWorkingDir = context.rteActiveProject->GetProjectPath() + generatorTmpWorkingDir;
    }
  } else {
    generatorTmpWorkingDir = destinationPath;
  }
  if (!generatorTmpWorkingDir.empty() && generatorTmpWorkingDir.back() != '/') {
    generatorTmpWorkingDir += '/';
  }
  const string filePath = generatorTmpWorkingDir + context.name + ".generate.yml";

  // Make sure the folders exist
  try {
    if (!fs::exists(generatorTmpWorkingDir)) {
      fs::create_directories(generatorTmpWorkingDir);
    }
    if (!fs::exists(destinationPath)) {
      fs::create_directories(destinationPath);
    }
  } catch (const fs::filesystem_error& e) {
    ProjMgrLogger::Error("Failed to create folders for the generator input file '" + filePath + "': " + e.what());
    return {};
  }

  ofstream fileStream(filePath);
  if (!fileStream) {
    ProjMgrLogger::Error("Failed to create generator input file '" + filePath + "'");
    return {};
  }

  fileStream << emitter.c_str();
  fileStream << flush;
  if (!fileStream) {
    ProjMgrLogger::Error("Failed to write generator input file '" + filePath + "'");
    return {};
  }
  fileStream.close();
  return filePath;
}
