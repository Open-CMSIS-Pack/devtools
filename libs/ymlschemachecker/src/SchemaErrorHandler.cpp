/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SchemaErrorHandler.h"
#include "RteUtils.h"
#include "yaml-cpp/yaml.h"

void CustomErrorHandler::error(const nlohmann::json::json_pointer& ptr,
  const json& instance, const std::string& message)
{
  std::list<std::string> segments;
  std::string schemaNodesStr;
  
  schemaNodesStr = ptr.to_string();
  schemaNodesStr = schemaNodesStr.substr(1, schemaNodesStr.size());
  RteUtils::SplitString(segments, schemaNodesStr, '/');

  auto yamldata = YAML::LoadFile(m_yamlFile);
  auto itrNode = yamldata;

  for (auto& segment : segments) {
    itrNode = itrNode[segment];
  }
  auto mark = itrNode.Mark();
  m_errList.push_back(SchemaError(m_yamlFile, message, mark.line + 1, mark.column + 1));
}

SchemaErrors& CustomErrorHandler::GetAllErrors() {
  return m_errList;
}
