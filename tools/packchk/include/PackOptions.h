/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PACKOPTIONS_H
#define PACKOPTIONS_H

#include <cstdint>
#include <string>
#include <set>


class CPackOptions {
public:
  CPackOptions();
  ~CPackOptions();

  enum class PedanticLevel { NONE = 0, WARNING, INFO };


  bool SetWarnLevel(const uint32_t warnLevel);
  bool SetPedantic(const PedanticLevel pedanticLevel);
  PedanticLevel GetPedantic();
  bool SetLogFile(const std::string& m_logFile);
  bool SetXsdFile();
  bool SetXsdFile(const std::string& m_xsdFile);
  bool AddDiagSuppress(const std::string& suppress);
  bool SetVerbose(bool bVerbose);
  bool SetFileUnderTest(const std::string& filename);
  bool AddRefPackFile(const std::string& includeFile);
  bool SetPackNamePath(const std::string& packNamePath);
  bool SetUrlRef(const std::string& urlRef);
  bool SetIgnoreOtherPdscFiles(bool bIgnore);
  bool GetIgnoreOtherPdscFiles();
  bool SetDisableValidation(bool bDisable);
  bool GetDisableValidation();
  bool AddRefPdscFile(const std::string& filename);
  bool HaltProgramExecution();
  bool SetAllowSuppresssError(bool bAllow);

  std::string GetCurrentDateTime();

  const std::string GetHeader();
  const std::string GetVersion();
  const std::string GetVersionInfo();
  const std::string GetProgramName();
  const std::string& GetUrlRef();
  const std::string& GetPackTextfileName();
  const std::string& GetPdscFullpath();
  const std::string& GetLogPath();
  const std::string& GetXsdPath();

  const std::set<std::string>& GetPdscRefFullpath();

  bool IsSkipOnPdscTest(const std::string& filename);

private:
  bool m_bIgnoreOtherPdscFiles;
  bool m_bDisableValidation;
  PedanticLevel m_pedanticLevel;

  std::string m_urlRef;    // package URL reference, check the URL of the PDSC against this value. if not std::set it is compared against the Keil Pack Server URL
  std::string m_packNamePath;
  std::string m_packToCheck;
  std::string m_logPath;
  std::string m_xsdPath;   // PACK.xsd file path, use to validate the input PDSC file
  std::set<std::string> m_packsToRef;
};

#endif // PACKOPTIONS_H
