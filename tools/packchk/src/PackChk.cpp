/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "PackChk.h"
#include "PackOptions.h"
#include "ValidateSemantic.h"
#include "ValidateSyntax.h"
#include "CreateModel.h"

#include "RteUtils.h"
#include "RteFsUtils.h"
#include "ErrLog.h"
#include "ErrOutputterSaveToStdoutOrFile.h"
#include "ParseOptions.h"

using namespace std;

/**
 * @brief class PackChk constructor
*/
PackChk::PackChk()
{
  ErrLog::Get()->SetOutputter(new ErrOutputterSaveToStdoutOrFile());
  InitMessageTable();
}

/**
* @brief class PackChk destructor
*/
PackChk::~PackChk()
{
}

/**
 * @brief initialize the ErrLog Messages table for error reporting
 * @return passed / failed
*/
bool PackChk::InitMessageTable()
{
  PdscMsg::AddMessages(msgTable);
  PdscMsg::AddMessagesStrict(msgStrictTable);

  return true;
}

/**
 * @brief Create Text file with Pack name
 * @param filename string name of file
 * @param pKg package under test
 * @return passed / failed
*/
bool PackChk::CreatePacknameFile(const string& filename, RtePackage* pKg)
{
  if(filename.empty()) {
    return false;
  }

  RteItem* releases = pKg->GetReleases();
  if(!releases) {
    return false;
  }

  const auto& childs = releases->GetChildren();
  if(childs.empty()) {
    return false;
  }

  const string& pdscRef = pKg->GetAttribute("vendor");
  const string& pkgVendor = pKg->GetName();

  const auto release = *childs.begin();
  const string& pkgVersion = release->GetVersionString();

  string absPath = RteFsUtils::AbsolutePath(filename).generic_string();
  string content = pdscRef + "." + pkgVendor + "." + pkgVersion + PKG_FEXT;
  if(!RteFsUtils::CreateTextFile(absPath, content)) {
    LogMsg("M205", PATH(absPath));
    return false;
  }

  return true;
}

/**
 * @brief run through all test steps
 * @return passed / failed
*/
bool PackChk::CheckPackage()
{
  LogMsg("M061");
  CreateModel createModel(m_rteModel);

  // Validate all PDSC files against Pack.xsd
  if(!m_packOptions.GetDisableValidation()) {
    if(!createModel.SetPackXsd(m_packOptions.GetXsdPath())) {
      return false;
    }
  }

  // Add PDSC files to check (currently limited to one)
  const string& pdscFile = m_packOptions.GetPdscFullpath();
  if(!createModel.AddPdsc(pdscFile, m_packOptions.GetIgnoreOtherPdscFiles())) {
    return false;
  }

  // Add reference files
  const set<string>& pdscRefFiles = m_packOptions.GetPdscRefFullpath();
  createModel.AddRefPdsc(pdscRefFiles);

  bool bOk = true;

  LogMsg("M015");
  LogMsg("M023", VAL("CHECK", "1: Read PDSC files"));

  // Read all PDSC files
  if(!createModel.ReadAllPdsc()) {
    bOk = false;
  }

  // Validate Model
  LogMsg("M015");
  LogMsg("M023", VAL("CHECK", "2: Static Data & Dependencies check"));
  ValidateSyntax validateSyntax(m_rteModel, m_packOptions);
  if(!validateSyntax.Check()) {
    bOk = false;
  }

  // Validate dependencies
  LogMsg("M015");
  LogMsg("M023", VAL("CHECK", "3: RTE Model based Data & Dependencies check"));
  ValidateSemantic validateSemantic(m_rteModel, m_packOptions);
  if(!validateSemantic.Check()) {
    bOk = false;
  }

  // Create File with Packet Name
  const string& packnameFile = m_packOptions.GetPackTextfileName();
  if(!packnameFile.empty()) {
    const RtePackageMap& packs = m_rteModel.GetPackages();
    if(!packs.empty()) {
      RtePackage* pKg = 0;

      for(auto &[pKey, pVal] : packs) {
        if(!pVal) {
          continue;
        }

        pKg = pVal;
        if(pKg->GetPackageFileName() == pdscFile) {
          break;
        }
      }

      if(pKg) {
        CreatePacknameFile(packnameFile, pKg);
      }
    }
  }

  LogMsg("M016");
  LogMsg("M022", ERR(ErrLog::Get()->GetErrCnt()), WARN(ErrLog::Get()->GetWarnCnt()));

  return bOk;
}

/**
 * @brief PackChk wrapper main entry point. Parses arguments and executes the tests
 * @param argc command line argument
 * @param argv command line argument
 * @param envp command line argument (not used)
 * @return passed / failed
*/
int PackChk::Check(int argc, const char* argv[], const char* envp[])
{
  const string header = m_packOptions.GetHeader();
  LogMsg("M001", TXT(header));

  ParseOptions parseOptions(m_packOptions);
  ParseOptions::Result result = parseOptions.Parse(argc, argv);

  // Add date and time to log file
  if(!m_packOptions.GetLogPath().empty()) {
    string dateTime = m_packOptions.GetCurrentDateTime();
    LogMsg("M002", TXT("Log created on "), TXT2(dateTime));
  }

  switch(result) {
    case ParseOptions::Result::Ok:
      break;
    case ParseOptions::Result::ExitNoError:
      return 0;
    case ParseOptions::Result::Error:
      if(!ErrLog::Get()->GetErrCnt()) {
        LogMsg("M105");
      }
      return 1;
  }

  bool bOk = CheckPackage();

  if(ErrLog::Get()->GetErrCnt() || !bOk) {
    return 1;
  }

  CPackOptions::PedanticLevel pedantic = m_packOptions.GetPedantic();
  if(pedantic != CPackOptions::PedanticLevel::NONE) {
    if(ErrLog::Get()->GetWarnCnt()) {
      return 1;
    }
  }

  return 0;
}
