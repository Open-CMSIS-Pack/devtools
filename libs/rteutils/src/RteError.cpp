/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "RteError.h"
#include "RteConstants.h"

#include <vector>
#include <sstream>
using namespace std;

RteError::RteError(const std::string& filePath, const std::string& errMsg,
  int line, int column) noexcept :
  m_severity(Severity::SevERROR),
  m_line(line),
  m_col(column),
  m_msg(errMsg),
  m_file(filePath)
{
}
RteError::RteError(Severity severity, const std::string& filePath, const std::string& errMsg,
  int line, int column) noexcept :
  m_severity(severity),
  m_line(line),
  m_col(column),
  m_msg(errMsg),
  m_file(filePath)
{
}

std::string RteError::ToString() const
{
  return RteError::FormatError(m_severity, m_file, m_msg, m_line, m_col);
}

std::string RteError::FormatError(Severity severity, const std::string& filePath, const std::string& errMsg,
  int line, int column)
{
  static const std::vector<std::string> s_SeverityStrings = {"", "Info", "Warning", "Error","Fatal error"};

  stringstream ss;
  if(!filePath.empty()) {
    ss << filePath;
    if(line ) {
      ss << RteConstants::OBRACE_CHAR << line;
      if(column) {
        ss << RteConstants::COMMA_CHAR << column;
      }
      ss << RteConstants::CBRACE_CHAR;
    }
    ss << RteConstants::COLON_CHAR << RteConstants::SPACE_CHAR;
  }
  ss << s_SeverityStrings[severity];
  if(severity > Severity::SevNONE) {
    ss << RteConstants::COLON_CHAR << RteConstants::SPACE_CHAR;
  }
  ss << errMsg;
  return ss.str();
}

// End of RteError.cpp
