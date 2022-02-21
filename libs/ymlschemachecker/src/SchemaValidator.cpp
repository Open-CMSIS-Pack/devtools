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
      throw SchemaError(m_dataFile, "could not open file", 0, 0 );
    }

    try {
      file >> data;
    }
    catch (const std::exception& e) {
      file.close();
      throw SchemaError(m_dataFile, e.what(), 0, 0);
    }
    file.close();
  }
  else if (extn == "yml" || extn == "yaml") {
    YAML::Node yamldata;
    try {
      yamldata = YAML::LoadFile(m_dataFile);
    }
    catch (YAML::Exception& e) {
      throw SchemaError(m_dataFile, e.what(), e.mark.line + 1, e.mark.column + 1);
    }

    data = YamlToJson(yamldata);
  }

  return data;
}

json SchemaValidator::ReadSchema() {
  json schema;
  std::ifstream file(m_schemaFile);
  if (!file.good()) {
    throw SchemaError(m_schemaFile, "could not open file", 0, 0);
  }

  try {
    file >> schema;
  }
  catch (const nlohmann::detail::parse_error& e) {
    file.close();
    throw SchemaError(m_schemaFile, e.what(), e.pos.lines_read, e.pos.chars_read_current_line);
  }
  catch (const std::exception& e) {
    file.close();
    throw SchemaError(m_schemaFile, e.what(), 0, 0);
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
    data   = ReadData();
    schema = ReadSchema();
  }
  catch (const SchemaError& err) {
    errList.push_back(err);
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
    errList.push_back(SchemaError(m_schemaFile, e.what(), 0, 0));
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
    throw SchemaError(filename, "could not open " + uri.url(), 0, 0);
  }

  try {
    lf >> schema;
  }
  catch (const std::exception& e) {
    lf.close();
    throw SchemaError(filename, e.what(), 0, 0);
  }
  lf.close();
}
