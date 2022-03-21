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
  // Check if the input file exist
  if (!RteFsUtils::Exists(input)) {
    ProjMgrLogger::Error(input, " file doesn't exist");
    return false;
  }

  // Get schema file path
  string schemaFile;
  if(!GetSchemaFile(schemaFile, type)) {
    ProjMgrLogger::Warn(input, "yaml schemas were not found, file cannot be validated");
    return true;
  }

  m_errList.clear();
  // Validate schema
  bool result = SchemaChecker::Validate(input, schemaFile, m_errList);
  for (auto err : m_errList) {
    ProjMgrLogger::Error(err.m_file, err.m_line, err.m_col, err.m_msg);
  }

  return result;
}

SchemaErrors& ProjMgrYamlSchemaChecker::GetErrors() {
  return m_errList;
}

bool ProjMgrYamlSchemaChecker::GetSchemaFile(string& schemaFile, const ProjMgrYamlSchemaChecker::FileType& type) {
  schemaFile = RteUtils::EMPTY_STRING;

  // Get the schema file name
  string schemaFileName;
  switch (type)
  {
  case ProjMgrYamlSchemaChecker::FileType::DEFAULT:
    schemaFileName = "cdefault.schema.json";
    break;
  case ProjMgrYamlSchemaChecker::FileType::SOLUTION:
    schemaFileName = "csolution.schema.json";
    break;
  case ProjMgrYamlSchemaChecker::FileType::PROJECT:
    schemaFileName = "cproject.schema.json";
    break;
  case ProjMgrYamlSchemaChecker::FileType::LAYER:
    schemaFileName = "clayer.schema.json";
    break;
  default:
    ProjMgrLogger::Error("Unknown file type");
    return false;
  }

  // Get current exe path
  std::error_code ec;
  string exePath = RteUtils::ExtractFilePath(
    CrossPlatformUtils::GetExecutablePath(ec), true);
  if (ec) {
    ProjMgrLogger::Error(ec.message());
    return false;
  }

  // Search schema in priority order
  vector<string> relSearchOrder = { "./", "../etc/", "../../etc/" };
  string schemaFilePath;
  for (auto& relPath : relSearchOrder) {
    schemaFilePath = exePath + relPath + schemaFileName;
    if (RteFsUtils::Exists(schemaFilePath)) {
      schemaFile = fs::canonical(schemaFilePath, ec).generic_string();
      if (ec) {
        ProjMgrLogger::Error(ec.message());
        return false;
      }
      return true;
    }
  }
  return false;
}
