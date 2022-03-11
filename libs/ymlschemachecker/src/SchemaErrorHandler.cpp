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

  auto [schemaNodesStr, bfalseSchema] = GetSearchString(ptr, message);
  schemaNodesStr = schemaNodesStr.substr(1, schemaNodesStr.size());
  RteUtils::SplitString(segments, schemaNodesStr, '/');

  std::vector<YAML::Node> nodes;
  nodes.push_back(m_yamlData);
  for (auto& segment : segments) {
    nodes.push_back(nodes.back()[segment]);
  }
  auto mark = nodes.back().Mark();
  m_errList.push_back(
    SchemaError(m_yamlFile, message,
      mark.line + 1, (bfalseSchema ? 0 : mark.column)));
}

SchemaErrors& CustomErrorHandler::GetAllErrors() {
  return m_errList;
}

std::pair<std::string, bool> CustomErrorHandler::GetSearchString(
  const nlohmann::json::json_pointer& ptr,
  const std::string& message) noexcept
{
  std::string searchStr = ptr.to_string() + "/";
  if (message.find("validation failed for additional property") != std::string::npos) {
    try {
      std::smatch match;
      std::regex re("\\'(.*?)\\'");
      if (std::regex_search(message, match, re)) {
        searchStr += match.str(1);
        return std::pair<std::string, bool>(searchStr, true);
      }
    }
    catch (const std::regex_error& e) {
      std::cerr << "CustomErrorHandler::GetSearchString regex_error caught: " << e.what() << std::endl;
    }
  }
  return std::pair<std::string, bool>(searchStr, false);
}
