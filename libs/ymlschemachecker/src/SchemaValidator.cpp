/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SchemaValidator.h"
#include "SchemaErrorHandler.h"
#include "RteUtils.h"

#include <fstream>
#include <iostream>

using nlohmann::json_schema::json_validator;

SchemaValidator::SchemaValidator(
  const std::string& dataFilePath,
  const std::string& schemaFilePath)
{
  m_dataFile   = dataFilePath;
  m_schemaFile = schemaFilePath;
}

SchemaValidator::~SchemaValidator()
{}

json SchemaValidator::ReadData() {
  json data;

  std::string extn = RteUtils::ExtractFileExtension(m_dataFile, false);
  if (extn == "json") {
    std::ifstream file(m_dataFile);
    if (!file.good()) {
      std::cerr << "error: Could not open " << m_dataFile << " for reading\n";
    }

    try {
      file >> data;
    }
    catch (const std::exception& e) {
      file.close();
      throw std::runtime_error("Error: " + std::string(e.what()) + " - while reading " + m_dataFile + "\n");
    }
    file.close();
  }
  else if (extn == "yml" || extn == "yaml") {
    YAML::Node yamldata;
    try {
      yamldata = YAML::LoadFile(m_dataFile);
    }
    catch (YAML::Exception& e) {
      throw std::runtime_error("Error: Opening " + m_dataFile + " YAML file - " + std::string(e.what()) + "\n");
    }

    data = YamlToJson(yamldata);
  }

  return data;
}

json SchemaValidator::ReadSchema() {
  json schema;
  std::ifstream file(m_schemaFile);
  if (!file.good()) {
    throw std::runtime_error("Error: Could not open " + m_schemaFile + "\n");
  }

  try {
    file >> schema;
  }
  catch (const std::exception& e) {
    throw std::runtime_error("Error: " + std::string(e.what()) + " - while parsing " + m_schemaFile + "\n");
  }

  file.close();
  return schema;
}

/*
 * Copyright (c) 2021 Mirco
 * Author: Mirco
 * This function is part of https://github.com/mircodezorzi/tojson
 * which is released under MIT License.
 * go to https://github.com/mircodezorzi/tojson/blob/master/LICENSE
 * for full license details.  
*/
nlohmann::json SchemaValidator::YamlToJson(const YAML::Node& root) {
  nlohmann::json jsonObj{};

  switch (root.Type())
  {
  case YAML::NodeType::Null:
    break;

  case YAML::NodeType::Scalar: {
    return ParseScalar(root);
  }

  case YAML::NodeType::Sequence:
  {
    for (auto&& node : root) {
      jsonObj.emplace_back(YamlToJson(node));
    }
    break;
  }

  case YAML::NodeType::Map:
  {
    for (auto&& it : root) {
      jsonObj[it.first.as<std::string>()] = YamlToJson(it.second);
    }
    break;
  }

  default:
    break;
  }
  return jsonObj;
}

/*
 * Copyright (c) 2021 Mirco
 * Author: Mirco
 * This function is part of https://github.com/mircodezorzi/tojson
 * which is released under MIT License.
 * go to https://github.com/mircodezorzi/tojson/blob/master/LICENSE
 * for full license details.  
*/
nlohmann::json SchemaValidator::ParseScalar(const YAML::Node& node) {
  int intVal;
  double doubleVal;
  bool boolVal;
  std::string strVal;

  if (YAML::convert<int>::decode(node, intVal)) {
    return intVal;
  }
  if (YAML::convert<double>::decode(node, doubleVal)) {
    return doubleVal;
  }
  if (YAML::convert<bool>::decode(node, boolVal)) {
    return boolVal;
  }
  if (YAML::convert<std::string>::decode(node, strVal)) {
    return strVal;
  }

  return nullptr;
}

bool SchemaValidator::Validate(SchemaErrors& errList) {
  json data, schema;

  // 1) Read the data and schema for the document you want to validate
  try {
    data = ReadData();
    schema = ReadSchema();
  }
  catch (const std::exception& e) {
    std::cerr << e.what();
    return false;
  }
  // 2) create the validator
  nlohmann::json_schema::schema_loader loader =
    std::bind(&SchemaValidator::Loader, this, std::placeholders::_1, std::placeholders::_2);
  json_validator validator(loader, nlohmann::json_schema::default_string_format_check);

  try {
    // insert this schema as the root to the validator
    // this resolves remote-schemas, sub-schemas and references via the given loader-function
    validator.set_root_schema(schema);
  }
  catch (const std::exception& e) {
    std::cerr << "setting root schema failed\n";
    std::cerr << e.what() << "\n";
    return false;
  }

  // 3) do the actual validation of the data
  CustomErrorHandler handler(m_dataFile);
  validator.validate(data, handler);

  errList = handler.GetAllErrors();
  return (errList.size() == 0) ? true : false;
}

void SchemaValidator::Loader(const json_uri& uri, json& schema)
{
  std::string filename = RteUtils::ExtractFilePath(m_schemaFile, true) + uri.path();
  std::ifstream lf(filename);
  if (!lf.good()) {
    throw std::invalid_argument("could not open " + uri.url() + " tried with " + filename);
  }

  try {
    lf >> schema;
  }
  catch (const std::exception& e) {
    std::cerr << e.what() << " while reading " << filename << "\n";
    throw e;
  }
}
