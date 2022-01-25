/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SCHEMAVALIDATOR_H
#define SCHEMAVALIDATOR_H

#include "SchemaChecker.h"

#include "yaml-cpp/yaml.h"
#include <nlohmann/json-schema.hpp>

#include <string>

using nlohmann::json;
using nlohmann::json_uri;

class SchemaValidator {
public:
  SchemaValidator(const std::string& dataFilePath,
    const std::string& schemaFilePath);
  ~SchemaValidator();

  /**
   * @brief Validate yaml data with json schema provided
   * @param errList list of errors found
   * @return true if the validation pass, otherwise false
  */
  bool Validate(SchemaErrors& errList);

private:
  json ReadData();
  json ReadSchema();

  void Loader(const json_uri& uri, json& schema);

  nlohmann::json YamlToJson(const YAML::Node& root);
  nlohmann::json ParseScalar(const YAML::Node& node);

  std::string m_dataFile;
  std::string m_schemaFile;
};

#endif // SCHEMAVALIDATOR_H
