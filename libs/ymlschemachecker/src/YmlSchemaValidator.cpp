/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "YmlSchemaValidator.h"
#include "YmlSchemaErrorHandler.h"
#include "RteUtils.h"

#include <fstream>
#include <iostream>

using nlohmann::json_schema::json_validator;

YmlSchemaValidator::YmlSchemaValidator(
  const std::string& dataFilePath,
  const std::string& schemaFilePath):
  m_dataFile(dataFilePath),
  m_schemaFile(schemaFilePath)
{
}

YmlSchemaValidator::~YmlSchemaValidator()
{}

json YmlSchemaValidator::ReadData() {
  json data;

  std::string extn = RteUtils::ExtractFileExtension(m_dataFile, false);
  if (extn == "json") {
    std::ifstream file(m_dataFile);
    if (!file.good()) {
      throw RteError(m_dataFile, "could not open file", 0, 0 );
    }

    try {
      file >> data;
    }
    catch (const std::exception& e) {
      file.close();
      throw RteError(m_dataFile, e.what(), 0, 0);
    }
    file.close();
  }
  else if (extn == "yml" || extn == "yaml") {
    YAML::Node yamldata;
    try {
      yamldata = YAML::LoadFile(m_dataFile);
    }
    catch (YAML::Exception& e) {
      throw RteError(m_dataFile, "schema check failed, verify syntax", e.mark.line + 1, e.mark.column + 1);
    }

    data = YamlToJson(yamldata);
  }

  return data;
}

json YmlSchemaValidator::ReadSchema() {
  json schema;
  std::ifstream file(m_schemaFile);
  if (!file.good()) {
    throw RteError(m_schemaFile, "could not open file", 0, 0);
  }

  try {
    file >> schema;
  }
  catch (const nlohmann::detail::parse_error& e) {
    file.close();
    throw RteError(m_schemaFile, e.what(), e.pos.lines_read, e.pos.chars_read_current_line);
  }
  catch (const std::exception& e) {
    file.close();
    throw RteError(m_schemaFile, e.what(), 0, 0);
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
nlohmann::json YmlSchemaValidator::YamlToJson(const YAML::Node& root) {
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
nlohmann::json YmlSchemaValidator::ParseScalar(const YAML::Node& node) {
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

bool YmlSchemaValidator::Validate(std::list<RteError>& errList) {
  json data, schema;

  // 1) Read the data and schema for the document you want to validate
  try {
    data = ReadData();
    schema = ReadSchema();
  }
  catch (const RteError& err) {
    errList.push_back(err);
    return false;
  }
  // 2) create the validator
  nlohmann::json_schema::schema_loader loader =
    std::bind(&YmlSchemaValidator::Loader, this, std::placeholders::_1, std::placeholders::_2);
  json_validator validator(loader, nlohmann::json_schema::default_string_format_check);

  try {
    // insert this schema as the root to the validator
    // this resolves remote-schemas, sub-schemas and references via the given loader-function
    validator.set_root_schema(schema);
  }
  catch (const std::exception& e) {
    errList.push_back(RteError(m_schemaFile, e.what(), 0, 0));
    return false;
  }

  // 3) do the actual validation of the data
  YmlSchemaErrorHandler handler(m_dataFile);
  validator.validate(data, handler);

  errList = handler.GetAllErrors();
  return (errList.size() == 0) ? true : false;
}

void YmlSchemaValidator::Loader(const json_uri& uri, json& schema)
{
  std::string filename = RteUtils::ExtractFilePath(m_schemaFile, true) + uri.path();
  std::ifstream lf(filename);
  if (!lf.good()) {
    throw RteError(filename, "could not open " + uri.url(), 0, 0);
  }

  try {
    lf >> schema;
  }
  catch (const std::exception& e) {
    lf.close();
    throw RteError(filename, e.what(), 0, 0);
  }
  lf.close();
}
// end of YmlSchemaValidator.cpp
