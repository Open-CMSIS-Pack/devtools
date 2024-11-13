/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ParseOptions.h"
#include "ErrLog.h"

#include <cxxopts.hpp>
#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;

/**
 * @brief class constructor
 * @param packOptions Options object to set parsed options
*/
ParseOptions::ParseOptions(SvdOptions& options) :
  m_options(options)
{
}

/**
 * @brief class destructor
*/
ParseOptions::~ParseOptions()
{
}

/**
 * @brief option "x,diag-suppress"
 * @param suppress string message
 * @return passed / failed
*/
bool ParseOptions::AddDiagSuppress(const std::string& suppress)
{
  return m_options.AddDiagSuppress(suppress);
}

/**
 * @brief option "w"
 * @param warnLevel warn level
 * @return
*/
bool ParseOptions::SetWarnLevel(const string& warnLevel)
{
  uint32_t level = 0;
  if(warnLevel.empty() || warnLevel == "all") {
    level = 3;
  }
  else {
    level = stoi(warnLevel);
  }

  return m_options.SetWarnLevel(level);
}

/**
 * @brief option "v,verbose"
 * @param bVerbose set verbose mode
 * @return passed / failed
 */
bool ParseOptions::SetVerbose(bool bVerbose)
{
  return m_options.SetVerbose(bVerbose);
}

/**
 * @brief option "filename under test"
 * @param filename string input filename
 * @return passed / failed
 */
bool ParseOptions::SetTestFile(const string& filename)
{
  return m_options.SetFileUnderTest(filename);
}

/**
 * @brief option "filename under test"
 * @param filename string input filename
 * @return passed / failed
 */
bool ParseOptions::SetOutputDirectory(const string& filename)
{
  return m_options.SetOutputDirectory(filename);
}

/**
 * @brief option argv[0], configures executable name if possible
 * @param programPath string name
 * @return passed / failed
 */
bool ParseOptions::ConfigureProgramName(string programPath)
{
  return m_options.ConfigureProgramName(programPath);
}

/**
 * @brief option "b,log"
 * @param logFile string log file name
 * @return
*/
bool ParseOptions::SetLogFile(const string& logFile)
{
  return m_options.SetLogFile(logFile);
}

/**
 * @brief option "n"
 * @param filename Override output file name
 * @return
*/
bool ParseOptions::SetOutFilenameOverride(const string& filename)
{
  return m_options.SetOutFilenameOverride(filename);
}

/**
 * @brief option "generate="
 * @param
 * @return passed / failed
*/
bool ParseOptions::ParseOptGenerate(const string& opt)
{
  if(opt == "header") {
    m_options.SetGenerateHeader();
  }
  else if(opt == "partition") {
    m_options.SetGeneratePartition();
  }
  else if(opt == "sfd") {
    m_options.SetGenerateSfd();
  }
  else if(opt == "sfr") {
    m_options.SetGenerateSfd();
    m_options.SetGenerateSfr();
  }
  else if(opt == "peripheralMap") {
    m_options.SetGenerateMapPeripheral();
  }
  else if(opt == "registerMap") {
    m_options.SetGenerateMapRegister();
  }
  else if(opt == "fieldMap") {
    m_options.SetGenerateMapField();
  }

  return true;
}

/**
 * @brief option "fields="
 * @param
 * @return passed / failed
*/
bool ParseOptions::ParseOptFields(const string& opt)
{
  string tmpOpt = opt;

  if(tmpOpt == "struct") {
    m_options.SetCreateFields();
  }
  else if(tmpOpt == "struct-ansic") {
    m_options.SetCreateFieldsAnsiC();
    m_options.SetCreateFields(); // ??
  }
  else if(tmpOpt == "macro") {
    m_options.SetCreateMacros();
  }
  else if(tmpOpt == "enum") {
    m_options.SetCreateEnumValues();
  }

  return true;
}

/**
 * @brief option "debug="
 * @param
 * @return passed / failed
*/
bool ParseOptions::ParseOptDebug(const string& opt)
{
  string tmpOpt = opt;

  if(tmpOpt == "struct") {
    m_options.SetDebugStruct();
  }
  else if(tmpOpt == "header") {
    m_options.SetDebugHeaderfile();
  }
  else if(tmpOpt == "sfd") {
    m_options.SetDebugSfd();
  }
  else if(tmpOpt == "break") {
    m_options.HaltProgramExecution();
  }

  return true;
}

bool ParseOptions::SetQuiet()
{
  m_options.SetQuietMode();

  return true;
}

bool ParseOptions::SetNoCleanup()
{
  m_options.SetNoCleanup();

  return true;
}

bool ParseOptions::SetUnderTest()
{
  m_options.SetUnderTest();

  return true;
}

bool ParseOptions::SetAllowSuppressError()
{
  m_options.SetAllowSuppressError();

  return true;
}

bool ParseOptions::SetSuppressWarnings()
{
  m_options.SetSuppressWarnings();

  return true;
}

bool ParseOptions::SetStrict()
{
  m_options.SetStrict();

  return true;
}

bool ParseOptions::SetShowMissingEnums()
{
  m_options.SetShowMissingEnums();

  return true;
}

bool ParseOptions::SetCreateFolder()
{
  m_options.SetCreateFolder();

  return true;
}

bool ParseOptions::SetSuppressPath()
{
  m_options.SetSuppressPath();

  return true;
}

/**
 * @brief parses all options
 * @param argc command line
 * @param argv command line
 * @return passed / failed
 */
ParseOptions::Result ParseOptions::Parse(int argc, const char* argv[])
{
  const char *argp = NULL;

  for(int i=0; i<argc; i++) {
    argp = argv[i];
    if(!argp || argp[0] == '\0') {
      continue;
    }
    if(argp[0] == '@') {
      return ParseOptsFile(argc, argv);
    }
  }

  return ParseOpts(argc, argv);
}

ParseOptions::Result ParseOptions::ParseOptsFileLine(const string& line, vector<string>& newOpts)
{
  string newOpt;
  bool bStringFound = false;

  for(const auto c : line) {
    if(c == '\"') {
      bStringFound ^= 1;
    }

    if(!bStringFound && c == '#') {   // comment, skip line from here
      break;
    }

    if(!bStringFound && isspace(c)) {
      if(!newOpt.empty()) {
        newOpts.push_back(newOpt);
        newOpt.clear();
      }
      continue;
    }

    newOpt += c;
  }

  if(!newOpt.empty()) {
    newOpts.push_back(newOpt);
  }

  return Result::Ok;
}

ParseOptions::Result ParseOptions::AddOptsFromFile(string fileName, vector<string>& newOpts)
{
  if(fileName.empty()) {
    return Result::Error;
  }

  ifstream optsFile;
  optsFile.open(fileName, ios::in);

  if (!optsFile.is_open()) {
    return Result::Error;
  }

  string oneLine;
  while(getline (optsFile, oneLine)) {
    ParseOptsFileLine(oneLine, newOpts);
  }

  return Result::Ok;
}

ParseOptions::Result ParseOptions::ParseOptsFile(int argc, const char* argv[])
{
  vector<string> newOpts;
  const char *argp = NULL;

  for(int i=0; i<argc; i++) {
    argp = argv[i];

    if(!argp || argp[0] == '\0') {
      continue;
    }

    if(argp[0] == '@') {
      AddOptsFromFile(&argp[1], newOpts);
      continue;
    }

    newOpts.push_back(argp);
  }

  vector<const char*> newArgv;
  transform(newOpts.begin(), newOpts.end(),  back_inserter(newArgv), [](const string& s) { return s.c_str(); } );

  return ParseOpts(newArgv.size(), newArgv.data());
}

bool ParseOptions::CreateArgumentString(int argc, const char* argv[])
{
  for(int i=0; i<argc; i++) {
    if(!m_cmdLine.empty()) {
      m_cmdLine += " ";
    }

    m_cmdLine += argv[i];
  }

  return true;
}

bool ParseOptions::PrintCommandLine()
{
  LogMsg("M024", VAL("OPTS", m_cmdLine));
  LogMsg("M016");

  return true;
}

ParseOptions::Result ParseOptions::ParseOpts(int argc, const char* argv[])
{
  CreateArgumentString(argc, argv);

  if(argv[0]) {
    ConfigureProgramName(argv[0]);
  }

  const string header = m_options.GetHeader();
  const string& fileName = m_options.GetProgramName();
  bool bOk = true;
  cxxopts::ParseResult parseResult;

  try {
    cxxopts::Options options(fileName, header);

    options.add_options()
      ( "input"                 , "Input PDSC"                                                , cxxopts::value<std::string>()->default_value(""))
      ( "o,outdir"              , "Output directory"                                          , cxxopts::value<string>() )
      ( "generate"              , "Generate header, partition or SDF/SFR file"                , cxxopts::value<std::vector<std::string>>() )
      ( "fields"                , "Specify field generation: enum/macro/struct/struct-ansic"  , cxxopts::value<std::vector<std::string>>() )
      ( "suppress-path"         , "Suppress inFile path on check output"                      , cxxopts::value<bool>()->default_value("false") )
      ( "create-folder"         , "Always create required folders"                            , cxxopts::value<bool>()->default_value("false") )
      ( "show-missingEnums"     , "Show SVD elements where enumerated values could be added"  , cxxopts::value<bool>()->default_value("false") )
      ( "strict"                , "Strict error checking (RECOMMENDED!)"                      , cxxopts::value<bool>()->default_value("false") )
      ( "b,log"                 , "Log file"                                                  , cxxopts::value<string>() )
      ( "x,diag-suppress"       , "Suppress Messages"                                         , cxxopts::value<std::vector<std::string>>() )
      ( "suppress-warnings"     , "Suppress all WARNINGs"                                     , cxxopts::value<bool>()->default_value("false") )
      ( "w"                     , "Warning level"                                             , cxxopts::value<string>()->default_value("all") )  /* -w0 .. -w3, -wall */
      ( "allow-suppress-error"  , "Allow to suppress error messages"                          , cxxopts::value<bool>()->default_value("false") )
      ( "v,verbose"             , "Verbose mode. Prints extra process information"            , cxxopts::value<bool>()->default_value("false") )
      ( "under-test"            , "Use when running in cloud environment"                     , cxxopts::value<bool>()->default_value("false") )
      ( "nocleanup"             , "Do not delete intermediate files"                          , cxxopts::value<bool>()->default_value("false") )
      ( "quiet"                 , "No output on console"                                      , cxxopts::value<bool>()->default_value("false") )
      ( "debug"                 , "Add information to generated files: struct/header/sfd/break" , cxxopts::value<std::vector<std::string>>() )
      ( "n"                     , "SFD Output file name"                                      , cxxopts::value<string>() )
      ( "V,version"               , "Show program version")
      ( "h,help"                , "Print usage")
      ;

    options.parse_positional({"input"});
    parseResult = options.parse(argc, argv);

    if (parseResult.count("version")) {
      cout << m_options.GetHeader() << endl;
      return Result::ExitNoError;
    }
    if ((argc < 2) || (parseResult.count("help"))) {
      cout << options.help() << endl;
      return Result::ExitNoError;
    }

    if(parseResult.count("quiet")) {
      if(!SetQuiet()) {
        bOk = false;
      }
    }
    if(parseResult.count("create-folder")) {
      if(!SetCreateFolder()) {
        bOk = false;
      }
    }
    if(parseResult.count("log")) {
      if(!SetLogFile(parseResult["log"].as<string>())) {
        bOk = false;
      }
    }
    if(parseResult.count("n")) {
      if(!SetOutFilenameOverride(parseResult["n"].as<string>())) {
        bOk = false;
      }
    }
    if(parseResult.count("v")) {
      if(!SetVerbose(true)) {
        bOk = false;
      }
    }
    if(parseResult.count("w")) {
      if(!SetWarnLevel(parseResult["w"].as<string>())) {
        bOk = false;
      }
    }
    if(parseResult.count("strict")) {
      if(!SetStrict()) {
        bOk = false;
      }
    }
    if(parseResult.count("suppress-warnings")) {
      if(!SetSuppressWarnings()) {
        bOk = false;
      }
    }
    if(parseResult.count("allow-suppress-error")) {
      if(!SetAllowSuppressError()) {
        bOk = false;
      }
    }
    if (parseResult.count("diag-suppress"))  {
      auto& v = parseResult["diag-suppress"].as<std::vector<std::string>>();
      for(const auto& s : v) {
        if(!AddDiagSuppress(s)) {
          bOk = false;
        }
      }
    }
    if(parseResult.count("under-test")) {
      if(!SetUnderTest()) {
        bOk = false;
      }
    }
    if(parseResult.count("nocleanup")) {
      if(!SetNoCleanup()) {
        bOk = false;
      }
    }
    if(parseResult.count("input")) {
      if(!SetTestFile(parseResult["input"].as<std::string>())) {
        bOk = false;
      }
    }
    if(parseResult.count("outdir")) {
      if(!SetOutputDirectory(parseResult["outdir"].as<string>())) {
        bOk = false;
      }
    }
    if (parseResult.count("generate"))  {
      auto& v = parseResult["generate"].as<std::vector<std::string>>();
      for(const auto& s : v) {
        if(!ParseOptGenerate(s)) {
          bOk = false;
        }
      }
    }
    if (parseResult.count("fields"))  {
      auto& v = parseResult["fields"].as<std::vector<std::string>>();
      for(const auto& s : v) {
        if(!ParseOptFields(s)) {
          bOk = false;
        }
      }
    }
    if (parseResult.count("debug"))  {
      auto& v = parseResult["debug"].as<std::vector<std::string>>();
      for(const auto& s : v) {
        if(!ParseOptDebug(s)) {
          bOk = false;
        }
      }
    }
    if(parseResult.count("suppress-path")) {
      if(!SetSuppressPath()) {
        bOk = false;
      }
    }
    if(parseResult.count("show-missingEnums")) {
      if(!SetShowMissingEnums()) {
        bOk = false;
      }
    }
  }
  catch (cxxopts::OptionException& e) {
    cerr << fileName << " error: " << e.what() << endl;
    return Result::Error;
  }

  if(!bOk) {
    return Result::Error;
  }

  return Result::Ok;
}
