/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ParseOptions_H
#define ParseOptions_H

#include "SvdOptions.h"

#include <string>
#include <vector>


class ParseOptions
{
public:
  ParseOptions(SvdOptions &options);
  ~ParseOptions();

  enum class Result
  {
    Ok = 0,
    ExitNoError,
    Error,
  };

  Result ParseOptsFile(int argc, const char *argv[]);
  Result AddOptsFromFile(std::string fileName, std::vector<std::string>& newOpts);
  Result ParseOptsFileLine(const std::string& line, std::vector<std::string>& newOpts);
  Result ParseOpts(int argc, const char *argv[]);
  Result Parse(int argc, const char *argv[]);
  bool   PrintCommandLine();

protected:
  bool SetWarnLevel(const std::string &warnLevel);
  bool SetTestFile(const std::string &filename);
  bool SetOutputDirectory(const std::string& filename);
  bool SetLogFile(const std::string &m_logFile);
  bool SetOutFilenameOverride(const std::string& filename);
  bool AddDiagSuppress(const std::string &suppress);
  bool SetVerbose(bool bVerbose);
  bool ConfigureProgramName(std::string programPath);
  bool CreateArgumentString(int argc, const char* argv[]);
  bool SetQuiet();
  bool SetNoCleanup();
  bool SetUnderTest();
  bool SetAllowSuppressError();
  bool SetSuppressWarnings();
  bool SetStrict();
  bool SetShowMissingEnums();
  bool SetCreateFolder();
  bool SetSuppressPath();


  bool ParseOptGenerate(const std::string& opt);
  bool ParseOptFields(const std::string& opt);
  bool ParseOptDebug(const std::string& opt);


private:
  SvdOptions   &m_options;
  std::string   m_cmdLine;

};

#endif // ParseOptions_H
