/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrYamlEmitter.h"
#include "ProjMgrUtils.h"
#include "RteFsUtils.h"
#include "RteItem.h"

#include "yaml-cpp/yaml.h"

#include <fstream>

using namespace std;

ProjMgrYamlEmitter::ProjMgrYamlEmitter(void) {
  // Reserved
}

ProjMgrYamlEmitter::~ProjMgrYamlEmitter(void) {
  // Reserved
}

bool ProjMgrYamlEmitter::EmitContextInfo(const ContextItem& context, const string& dst) {
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
  rootNode["destination"] = dst;
  YAML::Emitter emitter;
  emitter << rootNode;

  ofstream fileStream(context.name + ".generate.yml");
  fileStream << emitter.c_str();
  fileStream << flush;
  fileStream.close();

  return true;
}
