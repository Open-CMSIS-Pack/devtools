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
  const string& pdscRef = pKg->GetAttribute("vendor");
  const string& pkgVersion = pKg->GetVersionString();
  const string& pkgVendor = pKg->GetName();

  if(filename.empty()) {
    return false;
  }

  string outFile = pdscRef + "." + pkgVendor + "." + pkgVersion + PKG_FEXT;
  FILE* fp = fopen(filename.c_str(), "w");
  if(!fp) {
    LogMsg("M205", PATH(filename));
    return false;
  }

  fwrite(outFile.c_str(), 1, outFile.length(), fp);
  fclose(fp);

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

  // Add PDSC files to check (currently limited to one)
  const string& pdscFile = m_packOptions.GetPdscFullpath();
  if(!createModel.AddPdsc(pdscFile, m_packOptions.GetIgnoreOtherPdscFiles())) {
    return false;
  }

  // Add reference files
  const set<string>& pdscRefFiles = m_packOptions.GetPdscRefFullpath();
  createModel.AddRefPdsc(pdscRefFiles);

  LogMsg("M015");
  LogMsg("M023", VAL("CHECK", "1: Read PDSC files"));

  bool bOk = true;

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
  ParseOptions parseOptions(m_packOptions);
  ParseOptions::Result result = parseOptions.Parse(argc, argv);
  switch(result) {
    case ParseOptions::Result::Ok:
      break;
    case ParseOptions::Result::ExitNoError:
      return 0;
    case ParseOptions::Result::Error:
      return 1;
  }

  const string header = m_packOptions.GetHeader();
  LogMsg("M001", TXT(header));

  // Add date and time to log file
  if(!m_packOptions.GetLogPath().empty()) {
    string dateTime = m_packOptions.GetCurrentDateTime();
    LogMsg("M002", TXT("Log created on "), TXT2(dateTime));
  }

  bool bOk = CheckPackage();

  LogMsg("M016");
  LogMsg("M022", ERR(ErrLog::Get()->GetErrCnt()), WARN(ErrLog::Get()->GetWarnCnt()));

  if(ErrLog::Get()->GetErrCnt() || !bOk) {
    return 1;
  }

  return 0;
}
