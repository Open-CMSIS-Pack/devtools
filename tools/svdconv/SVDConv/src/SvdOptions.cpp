/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdOptions.h"
#include "ProductInfo.h"

#include "ErrLog.h"
#include "RteUtils.h"
#include "RteFsUtils.h"

#include "XMLTree.h"

#include <chrono>

using namespace std;

/**
 * @brief class constructor
*/
SvdOptions::SvdOptions() :
  m_bGenerateMapPeripheral(false),
  m_bGenerateMapRegister(false),
  m_bGenerateMapField(false),
  m_bGenerateHeader(false),
  m_bGeneratePartition(false),
  m_bGenerateSfd(false),
  m_bGenerateSfr(false),
  n_bCreateFields(false),
  n_bCreateFieldsAnsiC(false),
  m_bCreateMacros(false),
  m_bCreateEnumValues(false),
  m_bSuppressPath(false),
  m_bCreateFolder(false),
  m_bShowMissingEnums(false),
  m_bUnderTest(false),
  m_bNoCleanup(false),
  m_bDebugStruct(false),
  m_bDebugHeaderfile(false),
  m_bDebugSfd(false)
{
}

/**
* @brief class destructor
*/
SvdOptions::~SvdOptions()
{
}

/**
 * @brief halt on "--break" for debug purposes
 * @return passed / failed
*/
bool SvdOptions::HaltProgramExecution()
{
  printf("\nProgram halted, press <Enter> to continue execution.");
  (void)getchar();
  printf("Continue...");

  return true;
}

/**
 * @brief returns full path to PDSC under test
 * @return
*/
const string& SvdOptions::GetSvdFullpath()
{
  return m_svdToCheck;
}

/**
 * @brief returns full path to PDSC under test
 * @return
*/
string SvdOptions::GetSvdFileName()
{
  return RteUtils::ExtractFileName(m_svdToCheck);
}

void SvdOptions::SetStrict(bool bStrict /*= true*/)
{
   ErrLog::Get()->SetStrictMode();
}

/**
 * @brief returns path for log file
 * @return filename
*/
const std::string& SvdOptions::GetLogPath()
{
  return m_logPath;
}

/**
 * @brief set log file
 * @param logFile string filename
 * @return passed / failed
 */
bool SvdOptions::SetLogFile(const string& logFile)
{
  if(logFile.empty()) {
    return false;
  }

  m_logPath = RteUtils::ExtractFilePath(RteUtils::RemoveQuotes(logFile), true);
  if(!RteFsUtils::Exists(m_logPath)) {
    if(IsCreateFolder()) {
      MakeSurePathExists(m_logPath);
    }
  }

  m_logPath = RteUtils::RemoveQuotes(logFile);
  ErrLog::Get()->SetLogFileName(m_logPath);

  return true;
}

bool SvdOptions::IsGenerateMap() const
{
  return IsGenerateMapPeripheral() || \
         IsGenerateMapRegister()   || \
         IsGenerateMapField();
}

/**
 * @brief sets the program name if it can be determined from argv[0]
 * @param programPath string name
 * @return passed / failed
 */
bool SvdOptions::ConfigureProgramName(const string& programPath)
{
  string programName;

  if(programPath.empty()) {
    programName = ORIGINAL_FILENAME;
  }
  else {
    programName = RteUtils::ExtractFileName(programPath);
  }

  if(programName.empty()) {
    programName = ORIGINAL_FILENAME;
  }

  m_programName = programName;

  return true;
}

/**
 * @brief returns the program version string
 * @return string version
*/
string SvdOptions::GetVersion()
{
  string text = string(VERSION_STRING);

  return text;
}

/**
 * @brief returns the program header string
 * @return string header
*/
string SvdOptions::GetHeader()
{
  string header = string(PRODUCT_NAME);
  header += " ";
  header += GetVersion();
  header += " ";
  header += string(COPYRIGHT_NOTICE);
  header += "\n";

  return header;
}

/**
 * @brief returns current date / time string
 * @return string date/time
*/
string SvdOptions::GetCurrentDateTime()
{
  time_t result = time(nullptr);
  const char* timeAsc = asctime(std::localtime(&result));
  string timeText { "<unknown>" };
  if(timeAsc) {
    timeText = timeAsc;

    if(*timeText.rbegin() == '\n') {
      timeText.pop_back();    // erase '\n'
    }
  }

  return timeText;
}

/**
 * @brief returns the name of this executable
 * @return string name
*/
const string& SvdOptions::GetProgramName()
{
  return m_programName;
}

/**
 * @brief add messages Mxxx to suppress while logging messages.
 *        use "!Mxxx" to only show this messages(s) (inverts logic)
 * @param suppress message to suppress
 * @return passed / failed
*/
bool SvdOptions::AddDiagSuppress(const std::string& suppress)
{
  if(suppress.empty()) {
    return false;
  }

  if(suppress[0] == '!') {
    string num = suppress.substr(1);
    ErrLog::Get()->AddDiagShowOnly(num);
  }
  else {
    ErrLog::Get()->AddDiagSuppress(suppress);
  }

  ErrLog::Get()->CheckSuppressMessages();

  return true;
}
/**
 * @brief set the warning level to report messages
 * @param warnLevel uint32_t warn level
 * @return passed / failed
 */
bool SvdOptions::SetWarnLevel(const uint32_t warnLevel)
{
  switch(warnLevel) {
    case 0:
      ErrLog::Get()->SetLevel(MsgLevel::LEVEL_ERROR);
      break;
    case 1:
      ErrLog::Get()->SetLevel(MsgLevel::LEVEL_WARNING);
      break;
    case 2:
      ErrLog::Get()->SetLevel(MsgLevel::LEVEL_WARNING2);
      break;
    default:
    case 3:
      ErrLog::Get()->SetLevel(MsgLevel::LEVEL_WARNING3);
      break;
  }

  return true;
}

/**
 * @brief enabled verbose output (processing messages)
 * @param bVerbose true/false
 * @return passed / failed
 */
bool SvdOptions::SetVerbose(bool bVerbose)
{
  if(bVerbose) {
    ErrLog::Get()->SetLevel(MsgLevel::LEVEL_PROGRESS);
  }

  return true;
}

/**
 * @brief set PDSC file under test
 * @param filename string name
 * @return passed / failed
 */
bool SvdOptions::SetFileUnderTest(const string& filename)
{
  if(!m_svdToCheck.empty()) {  // allow one input file
    LogMsg("M202");
    return false;
  }

  string svdToCheck = RteUtils::BackSlashesToSlashes(RteUtils::RemoveQuotes(filename));
  m_svdToCheck = RteFsUtils::AbsolutePath(svdToCheck).generic_string();

  if(!RteFsUtils::Exists(m_svdToCheck)) {
    LogMsg("M123", PATH(m_svdToCheck));
    m_svdToCheck.clear();

    return false;
  }

  return true;
}

bool SvdOptions::MakeSurePathExists(const string& path)
{
  return RteFsUtils::CreateDirectories(path);
}

bool SvdOptions::SetOutFilenameOverride(const string& filename)
{
  m_outfileOverride = RteUtils::ExtractFileBaseName(filename);

  return true;
}

const string& SvdOptions::GetOutFilenameOverride() const
{
  return m_outfileOverride;
}

/**
 * @brief set output directory
 * @param filename string name
 * @return passed / failed
 */
bool SvdOptions::SetOutputDirectory(const string& filename)
{
  if(!m_outputDir.empty()) {  // allow one
    //LogMsg("M202");
    return false;
  }

  string outputDir = RteUtils::BackSlashesToSlashes(RteUtils::RemoveQuotes(filename));
  m_outputDir = RteFsUtils::AbsolutePath(outputDir).generic_string();

  if(!RteFsUtils::Exists(m_outputDir)) {
    if(IsCreateFolder()) {
      MakeSurePathExists(m_outputDir);
    }
    else {
      LogMsg("M123", PATH(m_outputDir));
      m_outputDir.clear();
      return false;
    }
  }

  return true;
}

const std::string& SvdOptions::GetOutputDirectory()
{
  return m_outputDir;
}

void SvdOptions::SetQuietMode(bool bQuiet /* = true */)
{
  ErrLog::Get()->SetQuietMode(bQuiet);
}

void SvdOptions::SetAllowSuppressError(bool bSuppress /* = true */)
{
  ErrLog::Get()->SetAllowSuppressError(bSuppress);
}

void SvdOptions::SetSuppressWarnings(bool bSuppress /* = true */)
{
  ErrLog::Get()->SetLevel(MsgLevel::LEVEL_ERROR);
}
