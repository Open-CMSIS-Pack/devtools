/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SVDConv.h"
#include "SvdOptions.h"
#include "XMLTreeSlim.h"
#include "ErrLog.h"
#include "ErrOutputterSaveToStdoutOrFile.h"
#include "SvdModel.h"
#include "SvdDevice.h"
#include "SvdGenerator.h"
#include "RteFsUtils.h"
#include "CrossPlatformUtils.h"
#include "ProductInfo.h"
#include "ParseOptions.h"

#include <ostream>
#include <string>
#include <set>
#include <list>
#include <map>
#include <csignal>

using namespace std;

SvdModel* m_svdModel;


/**
 * @brief exception handler for other than C++/STL exceptions
*/
void Sighandler(int signum)
{
  string criticalErrMsg = "Exception or Segmentation fault occurred!\n  ";
  criticalErrMsg += to_string(signum);
  criticalErrMsg += " : ";

  switch(signum) {
    case SIGINT:
      criticalErrMsg += "interrupt";
      break;
    case SIGILL:
      criticalErrMsg += "illegal instruction - invalid function image";
      break;
    case SIGFPE:
      criticalErrMsg += "floating point exception";
      break;
    case SIGSEGV:
      criticalErrMsg += "segment violation";
      break;
    case SIGTERM:
      criticalErrMsg += "Software termination signal from kill";
      break;
    //case SIGBREAK:
    //  criticalErrMsg += "Ctrl-Break sequence";
    //  break;
    case SIGABRT:
    //case SIGABRT_COMPAT:
      criticalErrMsg += "abnormal termination triggered by abort call";
      break;
    default:
      criticalErrMsg += "unknown exception";
      break;
  }

  cout << criticalErrMsg << endl;
  LogMsg("M104", MSG(criticalErrMsg));
  ErrLog::Get()->Save();
  exit(2);
}


/**
 * @brief class SvdConv constructor
*/
SvdConv::SvdConv()
{
  ErrLog::Get()->SetOutputter(new ErrOutputterSaveToStdoutOrFile());
  InitMessageTable();
}

/**
* @brief class SvdConv destructor
*/
SvdConv::~SvdConv()
{
}

/**
 * @brief initialize the ErrLog Messages table for error reporting
 * @return passed / failed
*/
bool SvdConv::InitMessageTable()
{
  PdscMsg::AddMessages(msgTable);
  PdscMsg::AddMessagesStrict(msgStrictTable);

  return true;
}

/**
 * @brief SvdConv wrapper main entry point. Parses arguments and executes the tests
 * @param argc command line argument
 * @param argv command line argument
 * @param envp command line argument (not used)
 * @return passed / failed
*/
int SvdConv::Check(int argc, const char* argv[], const char* envp[])
{
  for (auto s : { SIGSEGV, SIGINT, SIGILL, SIGFPE, SIGSEGV, SIGTERM, /*SIGBREAK,*/ SIGABRT, /*SIGABRT_COMPAT*/ } ) {
    signal(s, Sighandler);  // catch fault
  }

  try {
#if 0   // Exception Test Code
    int *testPtr = (int *) 0x12345678;
    *testPtr = 0;
    string testStr = "a";
    testStr.compare(testStr.length() - 2, 2, "ab");
#endif

    ParseOptions parseOptions(m_svdOptions);
    const string header = m_svdOptions.GetHeader();

    ParseOptions::Result result = parseOptions.Parse(argc, argv);

    if(!m_svdOptions.GetLogPath().empty()) {
      cout << header << endl;
    }

    switch(result) {
      case ParseOptions::Result::Ok:
        break;
      case ParseOptions::Result::ExitNoError:
        return 0;
      case ParseOptions::Result::Error:
        return 1;
    }

    // Add date and time to log file
    if(!m_svdOptions.GetLogPath().empty()) {
      string dateTime = m_svdOptions.GetCurrentDateTime();
      LogMsg("M002", TXT("Log created on "), TXT2(dateTime));
    }

    parseOptions.PrintCommandLine();
    ErrLog::Get()->CheckSuppressMessages();
    LogMsg("M061");  // Checking Package Description

    CheckSvdFile();
  }
  catch(std::exception& e) {
    string criticalErrMsg = "STL exception occurred: ";
    criticalErrMsg += e.what();
    cout << criticalErrMsg << endl;
    LogMsg("M104", MSG(criticalErrMsg));
    ErrLog::Get()->Save();
    return 2;
  }
  catch(...) {
    string criticalErrMsg = "Unknown exception occurred!";
    cout << criticalErrMsg << endl;
    LogMsg("M104", MSG(criticalErrMsg));
    ErrLog::Get()->Save();
    return 2;
  }

  int errCnt  = ErrLog::Get()->GetErrCnt();
  int warnCnt = ErrLog::Get()->GetWarnCnt();

  LogMsg("M016");
  LogMsg("M022", ERR(errCnt), WARN(warnCnt));

  if(!m_svdOptions.GetLogPath().empty()) {
    cout << "Found " << errCnt << " Error(s) and " << warnCnt << " Warning(s)." << endl;
  }

  if(errCnt) {
    return 2;
  }
  else if(warnCnt) {
    return 1;
  }

  return 0;
}

SVD_ERR SvdConv::CheckSvdFile()
{
  uint32_t tAll = CrossPlatformUtils::ClockInMsec();

  SVD_ERR svdRes = SVD_ERR_SUCCESS;
  XMLTreeSlim* xmlTree;
  const string& path = m_svdOptions.GetSvdFullpath();

  const string version = VERSION_STRING;
  const string descr = PRODUCT_NAME;
  const string copyright = COPYRIGHT_NOTICE;

  LogMsg("M051", PATH(path));
  if(!RteFsUtils::Exists(path)) {
    LogMsg("M123", PATH(path));
    svdRes = SVD_ERR_NOT_FOUND;
    return svdRes;
  }

  xmlTree = new XMLTreeSlim();
  xmlTree->AddFileName(path);

  // ----------------------  Read XML  ----------------------
  uint32_t t1 = CrossPlatformUtils::ClockInMsec();
	bool success = xmlTree->ParseAll();
  uint32_t t2 = CrossPlatformUtils::ClockInMsec() - t1;

  if(success) { LogMsg("M040", NAME("Reading SVD File"), TIME(t2)); }
	else        { LogMsg("M111", NAME("Reading SVD File"));           }

  // ----------------------  Construct Model  ----------------------
  if (m_svdOptions.IsUnderTest()) {
    string inFile = m_svdOptions.GetSvdFileName();
    try {
      const fs::path inPath = inFile;
      const auto inFilename = inPath.filename();
      const auto fnString = inFilename.string();
     ErrLog::Get()->SetFileName(fnString);
    }
    catch (const fs::filesystem_error&) {
     ErrLog::Get()->SetFileName(inFile);
    }
  }
  else if (m_svdOptions.IsSuppressPath()) {
    string inFile = m_svdOptions.GetSvdFileName();
   ErrLog::Get()->SetFileName(inFile);
  }
  else {
   ErrLog::Get()->SetFileName(path);
  }

  t1 = CrossPlatformUtils::ClockInMsec();
  m_svdModel = new SvdModel(0);
  m_svdModel->SetInputFileName(path);
  m_svdModel->SetShowMissingEnums();
  success = m_svdModel->Construct(xmlTree);
  t2 = CrossPlatformUtils::ClockInMsec() - t1;

  if(success) { LogMsg("M040", NAME("Constructing Model"), TIME(t2)); }
	else        { LogMsg("M111", NAME("Constructing Model"));           }

  // ----------------------  Delete XML Tree  ----------------------
  t1 = CrossPlatformUtils::ClockInMsec();
  delete xmlTree;
  t2 = CrossPlatformUtils::ClockInMsec() - t1;

  if(success) { LogMsg("M040", NAME("Deleting XML Tree"), TIME(t2));  }
	else        { LogMsg("M111", NAME("Deleting XML Tree"));            }

  // ----------------------  Calculate Model  ----------------------
  t1 = CrossPlatformUtils::ClockInMsec();
  success = m_svdModel->CalculateModel();
  t2 = CrossPlatformUtils::ClockInMsec() - t1;

  if(success) { LogMsg("M040", NAME("Calculating Model"), TIME(t2));  }
	else        { LogMsg("M111", NAME("Calculating Model"));            }
  // ----------------------  Validate Model  ----------------------
  t1 = CrossPlatformUtils::ClockInMsec();
	success = m_svdModel->Validate();
  t2 = CrossPlatformUtils::ClockInMsec() - t1;

  if(success) { LogMsg("M040", NAME("Validating Model"), TIME(t2)); }
	else        { LogMsg("M111", NAME("Validating Model"));           }

  // ----------------------  GetModel: device  ----------------------
  SvdDevice  *device = m_svdModel->GetDevice();

  if(device && m_svdOptions.IsCreateFields() && !m_svdOptions.IsCreateFieldsAnsiC()) {     // if fields are generated, we have annon unions
    device->SetHasAnnonUnions();
  }

  // ----------------------  Create Generator  ----------------------
  SvdGenerator *generator = new SvdGenerator(m_svdOptions);
  string outDir = m_svdOptions.GetOutputDirectory();

  // ----------------------  Generate Listings  ----------------------
  if(m_svdOptions.IsGenerateMap()) {
    t1 = CrossPlatformUtils::ClockInMsec();

    if(device) {
      generator->SetSvdFileName(path);
      generator->SetProgramInfo(version, descr, copyright);

      if(m_svdOptions.IsGenerateMapPeripheral()) {
        success = generator->PeripheralListing  (device, outDir);
      }
      if(m_svdOptions.IsGenerateMapRegister()) {
        success = generator->RegisterListing    (device, outDir);
      }
      if(m_svdOptions.IsGenerateMapField()) {
        success = generator->FieldListing       (device, outDir);
      }
    }
    t2 = CrossPlatformUtils::ClockInMsec() - t1;

    if(success) { LogMsg("M040", NAME("Generate Listing File"), TIME(t2));}
	  else        { LogMsg("M111", NAME("Generate Listing File"));          }
  }

  // ----------------------  Generate CMSIS Headerfile  ----------------------
  if(m_svdOptions.IsGenerateHeader()) {
    t1 = CrossPlatformUtils::ClockInMsec();
    if(device) {
      generator->SetSvdFileName(path);
      generator->SetProgramInfo(version, descr, copyright);
      success = generator->CmsisHeaderFile(device, outDir);
    }
    t2 = CrossPlatformUtils::ClockInMsec() - t1;

    if(success) { LogMsg("M040", NAME("Generate CMSIS Headerfile"), TIME(t2)); }
	  else        { LogMsg("M111", NAME("Generate CMSIS Headerfile"));           }
  }

  // ----------------------  Generate CMSIS Partitionfile  ----------------------
  if(m_svdOptions.IsGeneratePartition()) {
    t1 = CrossPlatformUtils::ClockInMsec();
    if(device) {
      generator->SetSvdFileName(path);
      generator->SetProgramInfo(version, descr, copyright);
      success = generator->CmsisPartitionFile(device, outDir);
    }
    t2 = CrossPlatformUtils::ClockInMsec() - t1;

    if(success) { LogMsg("M040", NAME("Generate CMSIS Partitionfile"), TIME(t2)); }
	  else        { LogMsg("M111", NAME("Generate CMSIS Partitionfile"));           }
  }

  // ----------------------  Generate SFD File  ----------------------
  if(m_svdOptions.IsGenerateSfd()) {
    t1 = CrossPlatformUtils::ClockInMsec();
    if(device) {
      generator->SetSvdFileName(path);
      generator->SetProgramInfo(version, descr, copyright);
      success = generator->SfdFile(device, outDir);
    }
    t2 = CrossPlatformUtils::ClockInMsec() - t1;

    if(success) { LogMsg("M040", NAME("Generate System Viewer SFD File"), TIME(t2)); }
	  else        { LogMsg("M111", NAME("Generate System Viewer SFD File"));           }
  }

  // ----------------------  Generate SFR File  ----------------------
  if(m_svdOptions.IsGenerateSfr()) {
    t1 = CrossPlatformUtils::ClockInMsec();
    if(device) {
      generator->SetSvdFileName(path);
      generator->SetProgramInfo(version, descr, copyright);
      success = generator->SfrFile(device, outDir);
    }
    t2 = CrossPlatformUtils::ClockInMsec() - t1;

    if(success) { LogMsg("M040", NAME("Generate System Viewer SFR File"), TIME(t2)); }
	  else        { LogMsg("M111", NAME("Generate System Viewer SFR File"));           }
  }

  // ----------------------  Delete Generator  ----------------------
  delete generator;

  // ----------------------  Delete Model  ----------------------
  t1 = CrossPlatformUtils::ClockInMsec();
  delete m_svdModel;
  t2 = CrossPlatformUtils::ClockInMsec() - t1;

  if(success) { LogMsg("M040", NAME("Deleting Model"), TIME(t2)); }
	else        { LogMsg("M111", NAME("Deleting Model"));           }

  t2 = CrossPlatformUtils::ClockInMsec() - tAll;
  LogMsg("M041", TIME(t2));

  return svdRes;
}
