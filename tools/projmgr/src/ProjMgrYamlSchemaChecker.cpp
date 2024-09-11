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


bool ProjMgrYamlSchemaChecker::Validate(const std::string& file)
{
  // Check if the input file exist
  if (!RteFsUtils::Exists(file)) {
    ProjMgrLogger::Get().Error("file doesn't exist", "", file);
    return false;
  }

  string schemaFile = FindSchema(file);
  if(schemaFile.empty()) {
    ProjMgrLogger::Get().Warn("yaml schemas were not found, file cannot be validated", "", file);
    return true;
  }

  ClearErrors();
  // Validate schema
  bool result = ValidateFile(file, schemaFile);
  for (auto& err : GetErrors()) {
    ProjMgrLogger::Get().Error(err.m_msg, "", err.m_file, err.m_line, err.m_col);
  }
    return result;
}

std::string ProjMgrYamlSchemaChecker::FindSchema(const std::string& file) const
{
  // Get current exe path
  std::error_code ec;
  string exePath = RteUtils::ExtractFilePath( CrossPlatformUtils::GetExecutablePath(ec), true);
  if (ec) {
    ProjMgrLogger::Get().Error(ec.message());
    return RteUtils::EMPTY_STRING;
  }
  string baseFileName = RteUtils::ExtractFileBaseName(file); // remove .yml
  string schemaFileName = RteUtils::ExtractFileExtension(baseFileName);  // remove prefix
  if(schemaFileName.empty()) { // cdefault.yml case
    schemaFileName = baseFileName;
  }
  schemaFileName += ".schema.json";
  return RteFsUtils::FindFileInEtc(schemaFileName, exePath);
}

// end of ProjMgrYamlSchemaChecker
