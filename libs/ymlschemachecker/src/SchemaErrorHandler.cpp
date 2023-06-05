/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SchemaErrorHandler.h"
#include "RteUtils.h"

#include <regex>
#include <iostream>

using namespace std;

CustomErrorHandler::CustomErrorHandler(const std::string& filePath) {
  m_yamlFile = filePath;
  m_yamlData = YAML::LoadFile(m_yamlFile);
  m_errList.clear();
}

CustomErrorHandler::~CustomErrorHandler() {
  // Reserved
}

void CustomErrorHandler::error(const nlohmann::json::json_pointer& ptr,
  const json& instance, const std::string& message)
{
  std::list<std::string> segments;

  if (!ptr.empty()) {
    auto schemaNodesStr = ptr.to_string();
    schemaNodesStr = schemaNodesStr.substr(1, schemaNodesStr.size());
    RteUtils::SplitString(segments, schemaNodesStr, '/');
  }
  std::vector<YAML::Node> nodes;
  nodes.push_back(m_yamlData);
  for (auto& segment : segments) {
    nodes.push_back(nodes.back()[segment]);
  }
  auto mark = nodes.back().Mark();
  if (nodes.size() > 1) {
    auto parent = *prev(prev(nodes.end()));
    for (const auto& item : parent) {
      if (!item.first.Mark().is_null() && item.first.as<string>() == segments.back()) {
        mark = item.first.Mark();
        break;
      }
    }
  }

  m_errList.push_back(
    SchemaError(m_yamlFile, message,
      mark.line + 1, mark.column + 1));
}

SchemaErrors& CustomErrorHandler::GetAllErrors() {
  return m_errList;
}
