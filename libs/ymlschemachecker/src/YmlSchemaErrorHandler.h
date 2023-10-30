/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef YML_SCHEMA_ERROR_HANLERD_H
#define YML_SCHEMA_ERROR_HANLERD_H

#include "RteError.h"

#include <nlohmann/json-schema.hpp>
#include <string>
#include "yaml-cpp/yaml.h"

using nlohmann::json;

class YmlSchemaErrorHandler : public nlohmann::json_schema::basic_error_handler
{
public:
  YmlSchemaErrorHandler(const std::string& filePath);
  ~YmlSchemaErrorHandler();

  /**
   * @brief callback to process error data
   * @param pointer to json erroneous schema
   * @param erroneous instance
   * @param error message
  */
  void error(const nlohmann::json::json_pointer& ptr,
    const json& instance, const std::string& message) override;

  /**
   * @brief get list of all errors
   * @return list of all errors
  */
  const std::list<RteError>& GetAllErrors() const { return m_errList; };

private:
  std::string  m_yamlFile;
  YAML::Node m_yamlData;
  std::list<RteError> m_errList;

  /**
   * @brief get error location string
   * @param pointer to json erroneous schema
   * @param error message
  */
  std::pair<std::string, bool> GetSearchString(
    const nlohmann::json::json_pointer& ptr,
    const std::string& message) noexcept;
};

#endif // YML_SCHEMA_ERROR_HANLERD_H
