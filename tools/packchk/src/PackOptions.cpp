/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "PackOptions.h"
#include "ProductInfo.h"

#include "CrossPlatformUtils.h"
#include "ErrLog.h"
#include "RteUtils.h"
#include "RteFsUtils.h"

#include "XMLTree.h"

using namespace std;

/**
 * @brief class constructor
*/
CPackOptions::CPackOptions() :
  m_bIgnoreOtherPdscFiles(false),
  m_bDisableValidation(false),
  m_pedanticLevel(PedanticLevel::NONE)
{
}

/**
* @brief class destructor
*/
CPackOptions::~CPackOptions()
{
}

/**
 * @brief returns options flag (ignore other PDSC files in the same pack)
 * @return
*/
bool CPackOptions::GetIgnoreOtherPdscFiles()
{
  return m_bIgnoreOtherPdscFiles;
}

/**
 * @brief returns options flag (disable validate pdsc file)
 * @return
*/
bool CPackOptions::GetDisableValidation()
{
  return m_bDisableValidation;
}

/**
 * @brief returns name of text file for pack name creation
 * @return string name
*/
const string& CPackOptions::GetPackTextfileName()
{
  if(m_packNamePath.empty()) {
    return m_packNamePath;
  }

  size_t pos = 0;
  do {
    pos = m_packNamePath.find_first_of("\t\r\n ");
    if(pos != string::npos) {
      char c = m_packNamePath[pos];
      m_packNamePath.erase(pos, 1);
      LogMsg("M213", NUM(c));
    }
  } while(pos != string::npos);

  return(m_packNamePath);
}

/**
 * @brief halt on "--break" for debug purposes
 * @return passed / failed
*/
bool CPackOptions::HaltProgramExecution()
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
const string& CPackOptions::GetPdscFullpath()
{
  return m_packToCheck;
}

/**
 * @brief returns path for log file
 * @return filename
*/
const std::string& CPackOptions::GetLogPath()
{
  return m_logPath;
}

/**
 * @brief returns path for schema file
 * @return filename
*/
const std::string& CPackOptions::GetXsdPath()
{
  return m_xsdPath;
}

/**
 * @brief add input PDSC file to a list of reference files
 * @param filename string file name
 * @return passed / failed
*/
bool CPackOptions::AddRefPdscFile(const string& filename)
{
  if(filename.empty()) {
    return false;
  }

  m_packsToRef.insert(filename);

  return true;
}

/**
 * @brief set path for package under test
 * @param path string filename
 * @return passed / failed
 */
bool CPackOptions::SetPackNamePath(const std::string& path)
{
  if(path.empty()) {
    return false;
  }

  m_packNamePath = path;

  return true;
}

/**
 * @brief returns a list of PDSC reference files
 * @return list of string
*/
const set<string>& CPackOptions::GetPdscRefFullpath()
{
  return m_packsToRef;
}

/**
 * @brief returns reference URL to check against
 * @return string url
*/
const string& CPackOptions::GetUrlRef()
{
  return m_urlRef;
}

/**
 * @brief set PDSC file under test
 * @param filename string name
 * @return passed / failed
 */
bool CPackOptions::SetFileUnderTest(const string& filename)
{
  if(!m_packToCheck.empty()) {  // allow one input file
    LogMsg("M202");
    return false;
  }

  m_packToCheck = RteFsUtils::AbsolutePath(filename).generic_string();

  if(!RteFsUtils::Exists(m_packToCheck)) {
    LogMsg("M204", PATH(m_packToCheck));
    m_packToCheck.clear();

    return false;
  }

  return true;
}

/**
 * @brief adds a reference PDSC file
 * @param includeFile string filename
 * @return passed / failed
 */
bool CPackOptions::AddRefPackFile(const std::string& includeFile)
{
  if(includeFile.empty()) {
    return false;
  }

  string fullpath = RteFsUtils::AbsolutePath(includeFile).generic_string();
  AddRefPdscFile(fullpath);

  return true;
}

/**
 * @brief set log file
 * @param logFile string filename
 * @return passed / failed
 */
bool CPackOptions::SetLogFile(const string& logFile)
{
  if(logFile.empty()) {
    return false;
  }

  m_logPath = RteUtils::ExtractFilePath(logFile, true);
  ErrLog::Get()->SetLogFileName(logFile);

  return true;
}

/**
 * @brief set default pack.xsd file
 * @return passed / failed
 */
bool CPackOptions::SetXsdFile()
{
  // Get current exe path
  std::error_code ec;
  string exePath = RteUtils::ExtractFilePath(
      CrossPlatformUtils::GetExecutablePath(ec), true);
  if (ec) {
    LogMsg("M216", MSG(ec.message()));
    return false;
  }

  string msgPath;

  // Search schema in priority order
  vector<string> relSearchOrder = { "./", "../etc/", "../../etc/" };
  string schemaFilePath;
  for (auto& relPath : relSearchOrder) {
    schemaFilePath = exePath + relPath + "PACK.xsd";

    if(!msgPath.empty()) {
      msgPath += ", ";
    }
    msgPath += relPath;

    if (RteFsUtils::Exists(schemaFilePath)) {
      m_xsdPath = fs::canonical(schemaFilePath, ec).generic_string();
      if (m_xsdPath.empty()) {
        LogMsg("M204", PATH(schemaFilePath));
        return false;
      }
      return true;
    }
  }

  LogMsg("M218", PATH(exePath), MSG(msgPath));

  return false;
}

/**
 * @brief set pack.xsd file
 * @param xsdFile string filename
 * @return passed / failed
 */
bool CPackOptions::SetXsdFile(const string& xsdFile)
{
  if(xsdFile.empty()) {
    return false;
  }

  m_xsdPath = RteFsUtils::AbsolutePath(xsdFile).generic_string();
  if(!RteFsUtils::Exists(m_xsdPath)) {
    LogMsg("M219", PATH(m_xsdPath));
    m_xsdPath.clear();

    return false;
  }

  return true;
}

/**
 * @brief sets reference URL to check against
 * @param ref string url
 * @return passed / failed
 */
bool CPackOptions::SetUrlRef(const string& ref)
{
  if(!ref.length()) {
    return false;
  }

  string urlRef = ref;
  size_t pos = 0;
  do {
    pos = urlRef.find_first_of('\"');
    if(pos != string::npos) {
      urlRef.erase(pos, 1);
    }
  } while(pos != string::npos);

  if(!urlRef.length()) {
    return false;
  }

  m_urlRef = urlRef;

  return true;
}

/**
 * @brief set to ignore if there are any other PDSC files in pack
 * @param bIgnore bool true/false
 * @return passed / failed
 */
bool CPackOptions::SetIgnoreOtherPdscFiles(bool bIgnore)
{
  m_bIgnoreOtherPdscFiles = bIgnore;

  return true;
}

bool CPackOptions::SetDisableValidation(bool bDisable)
{
  m_bDisableValidation = bDisable;

  return true;
}

/**
 * @brief returns the program version string
 * @return string version
*/
const string CPackOptions::GetVersionInfo()
{
  return ORIGINAL_FILENAME + string(" ") + VERSION_STRING + " " + COPYRIGHT_NOTICE;
}

/**
 * @brief returns the program version string
 * @return string version
*/
const string CPackOptions::GetVersion()
{
  string text = string(VERSION_STRING);

  return text;
}

/**
 * @brief returns the program header string
 * @return string header
*/
const string CPackOptions::GetHeader()
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
string CPackOptions::GetCurrentDateTime()
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
const string CPackOptions::GetProgramName()
{
  return ORIGINAL_FILENAME;
}

/**
 * @brief add messages Mxxx to suppress while logging messages.
 *        use "!Mxxx" to only show this messages(s) (inverts logic)
 * @param suppress message to suppress
 * @return passed / failed
*/
bool CPackOptions::AddDiagSuppress(const std::string& suppress)
{
  if(suppress.empty()) {
    return false;
  }

  string num;
  if(suppress[0] == '!') {
    num = suppress.substr(1);
    ErrLog::Get()->AddDiagShowOnly(num);
  }
  else {
    num = suppress;
    ErrLog::Get()->AddDiagSuppress(num);
  }

  if(num == "M351") {
    ErrLog::Get()->AddDiagSuppress("M502");
    ErrLog::Get()->AddDiagSuppress("M504");
    LogMsg("M005", TXT(num), TXT2(": Also suppressing Messages "), TXT3("M502"), TXT4(" and "), TXT5("M504"));
  }
  else if(num == "M362") {
    ErrLog::Get()->AddDiagSuppress("M502");
    ErrLog::Get()->AddDiagSuppress("M504");
    LogMsg("M005", TXT(num), TXT2(": Also suppressing Messages "), TXT3("M502"), TXT4(" and "), TXT5("M504"));
  }

  ErrLog::Get()->CheckSuppressMessages();

  return true;
}
/**
 * @brief set the warning level to report messages
 * @param warnLevel uint32_t warn level
 * @return passed / failed
 */
bool CPackOptions::SetWarnLevel(const uint32_t warnLevel)
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
 * @brief set the pedantic level to return with error flag
 * @param warnLevel PEDANTIC_LEVEL pedantic level
 * @return passed / failed
 */
bool CPackOptions::SetPedantic(const PedanticLevel pedanticLevel)
{
  m_pedanticLevel = pedanticLevel;

  return true;
}

/**
 * @brief get the pedantic level to return with error flag
 * @param none
 * @return level
 */
CPackOptions::PedanticLevel CPackOptions::GetPedantic()
{
  return m_pedanticLevel;
}

/**
 * @brief enabled verbose output (processing messages)
 * @param bVerbose true/false
 * @return passed / failed
 */
bool CPackOptions::SetVerbose(bool bVerbose)
{
  if(bVerbose) {
    ErrLog::Get()->SetLevel(MsgLevel::LEVEL_PROGRESS);
  }

  return true;
}

/**
 * @brief enable suppressing of error message
 * @param bVerbose set verbose mode
 * @return passed / failed
 */
bool CPackOptions::SetAllowSuppresssError(bool bAllow)
{
  ErrLog::Get()->SetAllowSuppressError(bAllow);
  return true;
}

/**
 * @brief test if the current PDSC file is under test or a reference file
 * @param filename string name to check
 * @return skip file or not
*/
bool CPackOptions::IsSkipOnPdscTest(const string& filename)
{
  if(m_packsToRef.find(filename) == m_packsToRef.end()) {
    return false;
  }

  return true;
}
