/*
 * Copyright (c) 2020-2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrCbuildBase.h"
#include "ProjMgrUtils.h"
#include "ProjMgrYamlParser.h"

using namespace std;

// cbuild base
void ProjMgrCbuildBase::SetNodeValue(YAML::Node node, const string& value) {
  if (!value.empty()) {
    node = value;
  }
}

void ProjMgrCbuildBase::SetNodeValue(YAML::Node node, const vector<string>& vec) {
  for (const string& value : vec) {
    if (!value.empty()) {
      node.push_back(value);
    }
  }
}

void ProjMgrCbuildBase::SetNodeValueUniquely(YAML::Node node, const string& value) {
  if (!value.empty()) {
    for (const auto& item : node) {
      if (value == item.as<string>()) {
        return;
      }
    }
    node.push_back(value);
  }
}

YAML::Node ProjMgrCbuildBase::GetCustomNode(const CustomItem& value) {
  YAML::Node node;
  if (!value.scalar.empty()) {
    node = value.scalar;
  }
  else if (!value.vec.empty()) {
    for (const auto& item : value.vec) {
      node.push_back(GetCustomNode(item));
    }
  }
  else if (!value.map.empty()) {
    for (const auto& [key, item] : value.map) {
      node[key] = GetCustomNode(item);
    }
  }
  return node;
}

void ProjMgrCbuildBase::SetCustomNodes(YAML::Node node, const CustomItem& custom) {
  for (const auto& [key, value] : custom.map) {
    node[key] = GetCustomNode(value);
  }
}

const string ProjMgrCbuildBase::FormatPath(const string& original, const string& directory) {
  return ProjMgrUtils::FormatPath(original, directory, m_useAbsolutePaths);
}
