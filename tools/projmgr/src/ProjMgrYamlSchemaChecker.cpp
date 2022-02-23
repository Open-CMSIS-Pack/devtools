/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrYamlSchemaChecker.h"
#include "ProjMgrLogger.h"

#include "CrossPlatformUtils.h"
#include "RteFsUtils.h"

#include <string>

using namespace std;

ProjMgrYamlSchemaChecker::ProjMgrYamlSchemaChecker(void) {
  // Reserved
}

ProjMgrYamlSchemaChecker::~ProjMgrYamlSchemaChecker(void) {
  // Reserved
}

bool ProjMgrYamlSchemaChecker::Validate(const string& input,
  ProjMgrYamlSchemaChecker::FileType type)
{
  // Get schema path
  if (m_schemaPath.empty()) {
    std::error_code ec;
    m_schemaPath = RteUtils::ExtractFilePath(
      CrossPlatformUtils::GetExecutablePath(ec), true);
    if (ec) {
      ProjMgrLogger::Error(ec.message());
      return false;
    }
    m_schemaPath += "../etc/";
  }

  // Check if the input file exist
  if (!RteFsUtils::Exists(input)) {
    ProjMgrLogger::Error(input, " file doesn't exist");
    return false;
  }

  // Get the schema file name
  string schemaFile;
  switch (type)
  {
  case ProjMgrYamlSchemaChecker::FileType::SOLUTION:
    schemaFile = "csolution.schema.json";
    break;
  case ProjMgrYamlSchemaChecker::FileType::PROJECT:
    schemaFile = "cproject.schema.json";
    break;
  case ProjMgrYamlSchemaChecker::FileType::LAYER:
    schemaFile = "clayer.schema.json";
    break;
  default:
    ProjMgrLogger::Error("Unknown file type");
    return false;
  }

  string schemaFilePath = m_schemaPath + "/" + schemaFile;
  // Check if schema file exist
  if (!RteFsUtils::Exists(schemaFilePath)) {
    ProjMgrLogger::Warn(input, "yaml schemas were not found, file cannot be validated");
    return true;
  }

  schemaFilePath = fs::canonical(schemaFilePath).generic_string();
  m_errList.clear();

  // validate schema
  bool result = SchemaChecker::Validate(input, schemaFilePath, m_errList);
  for (auto err : m_errList) {
    ProjMgrLogger::Error(err.m_file, err.m_line, err.m_col, err.m_msg);
  }

  return result;
}

SchemaErrors& ProjMgrYamlSchemaChecker::GetErrors() {
  return m_errList;
}

int ProjMgrYamlSchemaChecker::add(int a, int b) {
  return a + b;
}

int ProjMgrYamlSchemaChecker::sub(int a, int b) {
  return b - a;
}
int ProjMgrYamlSchemaChecker::mul(int a, int b) {
  return a*b;
}
int ProjMgrYamlSchemaChecker::div(int a, int b) {
  return b/a;
}