/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef YML_SCHEMAVALIDATOR_H
#define YML_SCHEMAVALIDATOR_H

#include "YmlSchemaChecker.h"

#include "yaml-cpp/yaml.h"
#include <nlohmann/json-schema.hpp>

#include <string>

using nlohmann::json;
using nlohmann::json_uri;

class YmlSchemaValidator {
public:
  YmlSchemaValidator(const std::string& dataFilePath,
    const std::string& schemaFilePath);
  ~YmlSchemaValidator();

  /**
   * @brief Validate yaml data with json schema provided
   * @param errList list of errors found
   * @return true if the validation pass, otherwise false
  */
  bool Validate(std::list<RteError>& errList);

private:
  json ReadData();
  json ReadSchema();

  void Loader(const json_uri& uri, json& schema);

  nlohmann::json YamlToJson(const YAML::Node& root);
  nlohmann::json ParseScalar(const YAML::Node& node);

  std::string m_dataFile;
  std::string m_schemaFile;
};

#endif // YML_SCHEMAVALIDATOR_H
