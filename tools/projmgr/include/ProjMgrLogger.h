/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRLOGGER_H
#define PROJMGRLOGGER_H

#include <string>

/**
 * @brief projmgr logger class
*/
class ProjMgrLogger {
public:
  /**
   * @brief class constructor
  */
  ProjMgrLogger(void);

  /**
   * @brief class destructor
  */
  ~ProjMgrLogger(void);

  /**
   * @brief print error
   * @param message
  */
  static void Error(const std::string& file, const int line, const int column, const std::string& msg);
  static void Error(const std::string& file, const std::string& msg);
  static void Error(const std::string& msg);

  /**
   * @brief print warning
   * @param message
  */
  static void Warn(const std::string& file, const int line, const int column, const std::string& msg);
  static void Warn(const std::string& file, const std::string& msg);
  static void Warn(const std::string& msg);

  /**
   * @brief print debug
   * @param message
  */
  static void Debug(const std::string& msg);
  /**
   * @brief print info
   * @param message
  */
  static void Info(const std::string& file, const int line, const int column, const std::string& msg);
  static void Info(const std::string& file, const std::string& msg);
  static void Info(const std::string& msg);

  static bool m_quiet;
};

#endif  // PROJMGRLOGGER_H
