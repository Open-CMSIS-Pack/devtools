/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRLOGGER_H
#define PROJMGRLOGGER_H

#include <map>
#include <string>
#include <vector>

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
   * @brief get logger instance
   * @return logger reference
  */
  static ProjMgrLogger& Get();

  /**
   * @brief clear logger messages
  */
  void Clear();

  /**
   * @brief print error
   * @param message
  */
  void Error(const std::string& msg, const std::string& context = std::string(),
    const std::string& file = std::string(), const int line = 0, const int column = 0);

  /**
   * @brief print warning
   * @param message
  */
  void Warn(const std::string& msg, const std::string& context = std::string(),
    const std::string& file = std::string(), const int line = 0, const int column = 0);

  /**
   * @brief print info
   * @param message
  */
  void Info(const std::string& msg, const std::string& context = std::string(),
    const std::string& file = std::string(), const int line = 0, const int column = 0);

  /**
   * @brief print debug
   * @param message
  */
  static void Debug(const std::string& msg);


  static bool m_quiet;

  /**
   * @brief get errors
   * @return reference to errors map
  */
  std::map<std::string, std::vector<std::string>>& GetErrors() { return m_errors; }

  /**
   * @brief get warnings
   * @return reference to warnings map
  */
  std::map<std::string, std::vector<std::string>>& GetWarns() { return m_warns; }

  /**
   * @brief get info messages
   * @return reference to info messages map
  */
  std::map<std::string, std::vector<std::string>>& GetInfos() { return m_infos; }

protected:
  std::map<std::string, std::vector<std::string>> m_errors;
  std::map<std::string, std::vector<std::string>> m_warns;
  std::map<std::string, std::vector<std::string>> m_infos;
};

#endif  // PROJMGRLOGGER_H
