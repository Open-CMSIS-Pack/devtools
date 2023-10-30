/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RTE_ERROR_H
#define RTE_ERROR_H

#include <string>
#undef SevWARNING
#undef SevERROR
/**
 * @brief Struct representing an error in a file, for instance a syntax or semantic error or schema violation
*/
struct RteError
{
  /**
   * @brief enum for the error severity
  */
  enum Severity {
    SevNONE,     // severity is not set (default)
    SevINFO,     // "Info"
    SevWARNING,  // "Warning"
    SevERROR,    // "Error"
    SevFATAL     // "Fatal error"
  };

  /**
   * @brief default constructor
  */
  RteError(Severity severity = Severity::SevERROR) noexcept : m_severity(severity), m_line(0), m_col (0) {}
  /**
   * @brief Parametrized constructor
   * @param filePath file where error is found
   * @param errMsg error message
   * @param line one-based line location, 0 if not available
   * @param column one-based column location, 0 if not available
  */
  RteError(const std::string& filePath, const std::string& errMsg, int line = 0, int column = 0) noexcept;

  /**
   * @brief Parametrized constructor
   * @param severity error severity
   * @param filePath file where error is found
   * @param errMsg error message
   * @param line one-based line location, 0 if not available
   * @param column one-based column location, 0 if not available
  */
  RteError(Severity severity, const std::string& filePath, const std::string& errMsg, int line = 0, int column = 0) noexcept;

  /**
   * @brief copy constructor, uses default implementation
  */
  RteError(const RteError&) = default;

  /**
   * @brief assignment operator, uses default implementation
  */
  RteError& operator=(const RteError&) = default;

  /**
   * @brief move assignment operator, uses default implementation
  */
  RteError& operator=(RteError&&) noexcept = default;


  /**
   * @brief Return formatted string representation of this error
   * @return error string
  */
  std::string ToString() const;

  /**
   * @brief Return formatted string representation of an error
   * @param filePath file where error is found
   * @param errMsg error message
   * @param line one-based line location, 0 if not available
   * @param column one-based column location, 0 if not available
   * @return error string
  */
  static std::string FormatError(const std::string& filePath, const std::string& errMsg, unsigned line = 0, unsigned column = 0) {
    return FormatError(Severity::SevNONE, filePath, errMsg, line, column);
  }

  /**
   * @brief Return formatted string representation of an error
   * @param severity error severity
   * @param filePath file where error is found
   * @param errMsg error message
   * @param line one-based line location, 0 if not available
   * @param column one-based column location, 0 if not available
   * @return error string
  */
  static std::string FormatError(Severity severity, const std::string& filePath, const std::string& errMsg,
    int line = 0, int column = 0);

  Severity    m_severity;
  int    m_line;
  int    m_col;
  std::string m_msg;
  std::string m_file;
};

#endif // RTE_ERROR_H
