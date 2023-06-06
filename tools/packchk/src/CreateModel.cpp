/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "Validate.h"
#include "CreateModel.h"

#include "ErrLog.h"
#include "RteFsUtils.h"
#include "XmlChecker.h"

#include <list>
#include <string>

using namespace std;

/**
 * @brief class constructor
 * @param rteModel
*/
CreateModel::CreateModel(RteGlobalModel& rteModel) :
  m_reader(rteModel)
{
}

/**
* @brief class destructor
*/
CreateModel::~CreateModel()
{
}

/**
 * @brief check if there are other PDSC files in the same pack
 * @param pdscFullPath string path to PDSC
 * @return passed / failed
*/
bool CreateModel::CheckForOtherPdscFiles(const string& pdscFullPath)
{
  if(pdscFullPath == "") {
    return false;
  }

  std::list<std::string> pdscFiles;
  LogMsg("M064", PATH(pdscFullPath));

  // Search for PDSC files
  string path = RteUtils::ExtractFilePath(pdscFullPath, 0);
  RteFsUtils::GetPackageDescriptionFiles(pdscFiles, path, 1024);

  // Multiple PDSC file found in package?
  if(pdscFiles.size() > 1) {
    ErrLog::Get()->SetFileName(pdscFullPath);
    PrintPdscFiles(pdscFiles);
    ErrLog::Get()->SetFileName("");
    return false;
  }

  LogMsg("M010");

  return true;
}

/**
 * @brief prints list of PDSC files found in package
 * @param pdscFiles list of PDSC files
 * @return passed / failed
 */
bool CreateModel::PrintPdscFiles(std::list<std::string>& pdscFiles)
{
  if(pdscFiles.empty()) {
    return false;
  }

  string filesList;
  for(auto& fileName : pdscFiles) {
    filesList += "\n  ";
    filesList += fileName;
  }

  filesList += "\n  ";
  LogMsg("M206", VAL("FILES", filesList));

  return true;
}

/**
 * @brief add a PDSC file for testing
 * @param pdscFile string file under test
 * @param bSkipCheckForOtherPdsc skip check if there are other PDSC files in package
 * @return passed / failed
 */
bool CreateModel::AddPdsc(const string& pdscFile, bool bSkipCheckForOtherPdsc /* = false */)
{
  if(pdscFile.empty()) {
    LogMsg("M202");
    return false;
  }

  LogMsg("M051", PATH(pdscFile));

  if(!RteFsUtils::Exists(pdscFile)) {
    LogMsg("M204", PATH(pdscFile));
    return false;
  }
  if(RteFsUtils::IsDirectory(pdscFile)) {
    LogMsg("M202", PATH(pdscFile));
    return false;
  }

  if(!bSkipCheckForOtherPdsc) {
    if(!CheckForOtherPdscFiles(pdscFile)) {
      LogMsg("M203", PATH(pdscFile));
      return false;
    }
  }

  if(m_validatePdsc) {
    if(!XmlChecker::Validate(pdscFile, m_schemaFile)) {
      ; // continue checking
    }
  }

  if(!m_reader.AddFile(pdscFile)) {
    LogMsg("M201", PATH(pdscFile));
    return false;
  }

  LogMsg("M010");

  return true;
}

/**
 * @brief add reference PDSC files to resolve dependencies, e.g. to ARM_CMSIS
 * @param pdscRefFiles string reference PDSC files
 * @return passed / failed
 */
bool CreateModel::AddRefPdsc(const std::set<std::string>& pdscRefFiles)
{
  for(auto& refPdsc : pdscRefFiles) {
    if(!AddPdsc(refPdsc, true)) {
      return false;
    }
  }

  return true;
}

bool CreateModel::SetPackXsd(const std::string& packXsdFile)
{
  m_validatePdsc = true;

  if(packXsdFile.empty()) {
    return false;
  }

  if(!RteFsUtils::Exists(packXsdFile)) {
    LogMsg("M219", PATH(packXsdFile));
    return false;
  }

  m_schemaFile = RteFsUtils::AbsolutePath(packXsdFile).generic_string();;
  return true;
}

/**
 * @brief start reading all PDSC files
 * @return passed / failed
 */
bool CreateModel::ReadAllPdsc()
{
  if(!m_reader.ReadAll()) {
    return false;
  }

  return true;
}
