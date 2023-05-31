/*
* Copyright (c) 2020-2021 Arm Limited. All rights reserved.
*
* SPDX-License-Identifier: Apache-2.0
*/

#ifndef PARSEOPTIONS_H
#define PARSEOPTIONS_H

#include "PackOptions.h"

#include <string>


class ParseOptions {
public:
  ParseOptions(CPackOptions& packOptions);
  ~ParseOptions();

  enum class Result {
    Ok = 0,
    ExitNoError,
    Error,
  };

  Result Parse(int argc, const char* argv[]);

protected:
  bool SetWarnLevel(const std::string& warnLevel);
  bool SetPedantic(const std::string& pedanticLevel);
  bool SetTestPdscFile(const std::string& filename);
  bool SetLogFile(const std::string& m_logFile);
  bool SetXsdFile();
  bool SetXsdFile(const std::string& m_xsdFile);
  bool AddDiagSuppress(const std::string& suppress);
  bool AddRefPackFile(const std::string& includeFile);
  bool SetPackNamePath(const std::string& packNamePath);
  bool SetCheckSvd(bool bCheck);
  bool SetUrlRef(const std::string& urlRef);
  bool SetVerbose(bool bVerbose);
  bool SetIgnoreOtherPdscFiles(bool bIgnore);
  bool SetAllowSuppresssError(bool bAllow = true);
  bool SetDisableValidation(bool bDisable);

private:
  CPackOptions& m_packOptions;
};

#endif // PARSEOPTIONS_H
