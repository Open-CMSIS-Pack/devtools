/*
* Copyright (c) 2020-2021 Arm Limited. All rights reserved.
*
* SPDX-License-Identifier: Apache-2.0
*/
#include "ParseOptions.h"
#include "PackOptions.h"

#include <cxxopts.hpp>
#include <iostream>

using namespace std;

/**
 * @brief class constructor
 * @param packOptions CPackOptions object to set parsed options
*/
ParseOptions::ParseOptions(CPackOptions& packOptions) :
  m_packOptions(packOptions)
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
  return m_packOptions.AddDiagSuppress(suppress);
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

  return m_packOptions.SetWarnLevel(level);
}

bool ParseOptions::SetPedantic(const string& pedanticLevel)
{
  if(pedanticLevel.empty() || pedanticLevel == "info") {
    return m_packOptions.SetPedantic(CPackOptions::PedanticLevel::INFO);
  }
  else if(pedanticLevel == "warning") {
    return m_packOptions.SetPedantic(CPackOptions::PedanticLevel::WARNING);
  }
  else {
    return false;
  }
}

/**
 * @brief option "v,verbose"
 * @param bVerbose set verbose mode
 * @return passed / failed
 */
bool ParseOptions::SetVerbose(bool bVerbose)
{
  return m_packOptions.SetVerbose(bVerbose);
}

/**
 * @brief option "--allow-suppress-error"
 * @param bVerbose set verbose mode
 * @return passed / failed
 */
bool ParseOptions::SetAllowSuppresssError(bool bAllow /*= true*/)
{
  return m_packOptions.SetAllowSuppresssError(bAllow);
}

/**
 * @brief option "filename under test"
 * @param filename string input filename
 * @return passed / failed
 */
bool ParseOptions::SetTestPdscFile(const string& filename)
{
  return m_packOptions.SetFileUnderTest(filename);
}

/**
 * @brief option "i,include"
 * @param includeFile string reference file
 * @return passed / failed
 */
bool ParseOptions::AddRefPackFile(const std::string& includeFile)
{
  if(includeFile.empty()) {
    return false;
  }

  return m_packOptions.AddRefPackFile(includeFile);
}

/**
 * @brief option "b,log"
 * @param logFile string log file name
 * @return
*/
bool ParseOptions::SetLogFile(const string& logFile)
{
  return m_packOptions.SetLogFile(logFile);
}

/**
 * @brief option "s,xsd"
 * @return
 */
bool ParseOptions::SetXsdFile()
{
  return m_packOptions.SetXsdFile();
}

/**
 * @brief option "s,xsd"
 * @param logFile string xsd file name
 * @return
*/
bool ParseOptions::SetXsdFile(const string& xsdFile)
{
  return m_packOptions.SetXsdFile(xsdFile);
}

/**
 * @brief option "n"
 * @param packNamePath string filename
 * @return passed / failed
 */
bool ParseOptions::SetPackNamePath(const string& packNamePath)
{
  m_packOptions.SetPackNamePath(packNamePath);

  return true;
}

/**
 * @brief option "u"
 * @param urlRef string url
 * @return passed / failed
 */
bool ParseOptions::SetUrlRef(const string& urlRef)
{
  m_packOptions.SetUrlRef(urlRef);

  return true;
}

/**
 * @brief option "ignore-other-pdsc"
 * @param bIgnore true/false
 * @return passed / failed
*/
bool ParseOptions::SetIgnoreOtherPdscFiles(bool bIgnore)
{
  return m_packOptions.SetIgnoreOtherPdscFiles(bIgnore);
}

/**
 * @brief option "disable-validation"
 * @param bIgnore true/false
 * @return passed / failed
*/
bool ParseOptions::SetDisableValidation(bool bDisable)
{
  return m_packOptions.SetDisableValidation(bDisable);
}

/**
 * @brief parses all options
 * @param argc command line
 * @param argv command line
 * @return passed / failed
 */
ParseOptions::Result ParseOptions::Parse(int argc, const char* argv[])
{
  const string header = m_packOptions.GetHeader();
  const string fileName = m_packOptions.GetProgramName();
  bool bOk = true;
  cxxopts::ParseResult parseResult;

  try {
    cxxopts::Options options(fileName, header);

    options
      .set_width(80)
      .custom_help("[-V] [--version] [-h] [--help]\n          [OPTIONS...]")
      .positional_help("<PDSC file>")
      .add_options("packchk", {
        {"input", "Input PDSC", cxxopts::value<std::string>()->default_value("")},
        {"i,include", "PDSC file(s) as dependency reference", cxxopts::value<std::vector<std::string>>()},
        {"b,log", "Log file", cxxopts::value<string>()},
        {"x,diag-suppress", "Suppress Messages", cxxopts::value<std::vector<std::string>>()},
        {"s,xsd", "Specify PACK.xsd path.", cxxopts::value<string>()},
        {"v,verbose", "Verbose mode. Prints extra process information", cxxopts::value<bool>()->default_value("false")},
        {"w,warning", "Warning level [0|1|2|3|all]", cxxopts::value<string>()->default_value("all")},  /* -w0 .. -w3, -wall */
        {"u,url", "Verifies that the specified URL matches with the <url> element in the *.PDSC file", cxxopts::value<string>()->default_value("")},
        {"n,name", "Text file for pack file name", cxxopts::value<string>()->default_value("")},
        {"V,version", "Print version"},
        {"h,help", "Print usage"},
        {"disable-validation", "Disable the pdsc validation against the PACK.xsd.", cxxopts::value<bool>()->default_value("false")},
        {"allow-suppress-error", "Allow to suppress error messages", cxxopts::value<bool>()->default_value("false")},
        {"break", "Debug halt after start", cxxopts::value<bool>()->default_value("false")},
        {"ignore-other-pdsc", "Ignores other PDSC files in working folder", cxxopts::value<bool>()->default_value("false")},
        {"pedantic", "Return with error value on warning", cxxopts::value<bool>()->default_value("false")},
      });

    options.parse_positional({"input"});
    parseResult = options.parse(argc, argv);

    // Debug break
    if (parseResult.count("break")) {
      m_packOptions.HaltProgramExecution();
    }

    // Print version
    if (parseResult.count("version")) {
      cout << m_packOptions.GetVersionInfo() << endl;
      return Result::ExitNoError;
    }

    // Print usage
    if (argc < 2 || parseResult.count("help")) {
      cout << options.help() << endl;
      return Result::ExitNoError;
    }

    if(parseResult.count("log")) {
      if(!SetLogFile(parseResult["log"].as<string>())) {
        bOk = false;
      }
    }

    if(parseResult.count("v")) {
      if(!SetVerbose(true)) {
        bOk = false;
      }
    }

    if (parseResult.count("include"))  {
      auto& v = parseResult["include"].as<std::vector<std::string>>();
      for(const auto& s : v) {
        if(!AddRefPackFile(s)) {
          bOk = false;
        }
      }
    }

    if (parseResult.count("allow-suppress-error"))  {
      if(!SetAllowSuppresssError()) {
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

    if(parseResult.count("input")) {
      if(!SetTestPdscFile(parseResult["input"].as<std::string>())) {
        bOk = false;
      }
    }

    if(parseResult.count("u")) {
      if(!SetUrlRef(parseResult["u"].as<string>())) {
        bOk = false;
      }
    }

    if(parseResult.count("w")) {
      if(!SetWarnLevel(parseResult["w"].as<string>())) {
        bOk = false;
      }
    }

    if(parseResult.count("pedantic")) {
      if(!SetPedantic("warning")) {
        bOk = false;
      }
    }

    if(parseResult.count("n")) {
      if(!SetPackNamePath(parseResult["n"].as<string>())) {
        bOk = false;
      }
    }
    if(parseResult.count("ignore-other-pdsc")) {
      if(!SetIgnoreOtherPdscFiles(parseResult["ignore-other-pdsc"].as<bool>())) {
        bOk = false;
      }
    }
    if(parseResult.count("disable-validation")) {
      if(!SetDisableValidation(parseResult["disable-validation"].as<bool>())) {
        bOk = false;
      }
    }
    else {
      if(parseResult.count("xsd")) {
        if(!SetXsdFile(parseResult["xsd"].as<string>())) {
          bOk = false;
        }
      } else {
        // Get default pack.xsd
        if(!SetXsdFile()) {
          bOk = false;
        }
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
