/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRLOGGER_H
#define PROJMGRLOGGER_H

#include <map>
#include <string>
#include <sstream>
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

  /**
   * @brief returns reference to active output stream: cout (default) or string buffer (in silent mode)
   * @return reference to active output stream
  */
  static std::ostream& out();

  /**
   * @brief check if in quiet mode
   * @return true if quiet or silent
  */
  static bool IsQuiet() { return m_quiet || m_silent; }

  /**
  * @brief flag to suppress infos and warnings
  */
  static bool m_quiet;

 /**
  * @brief flag to suppress all output and redirect cout to string buffer
  */
  static bool m_silent;


  /**
   * @brief get errors
   * @return reference to errors map
  */
  const std::map<std::string, std::vector<std::string>>& GetErrors() const { return m_errors; }

  /**
   * @brief get warnings
   * @return reference to warnings map
  */
  const std::map<std::string, std::vector<std::string>>& GetWarns() const { return m_warns; }

  /**
   * @brief get info messages
   * @return reference to info messages map
  */
  const std::map<std::string, std::vector<std::string>>& GetInfos() const { return m_infos; }


  /**
   * @brief get errors
   * @return reference to errors map
  */
  const std::vector<std::string>& GetErrorsForContext(const std::string& context = std::string()) const;

  /**
   * @brief get warnings
   * @return reference to warnings map
  */
  const std::vector<std::string>& GetWarnsForContext(const std::string& context = std::string()) const;

  /**
   * @brief get info messages
   * @return reference to info messages map
  */
  const std::vector<std::string>& GetInfosForContext(const std::string& context = std::string()) const;


  /**
   * @brief get sting stream
   * @return reference to internal string stream
  */
  const std::ostringstream& GetStringStream() const { return m_ss; }

protected:
  std::map<std::string, std::vector<std::string>> m_errors;
  std::map<std::string, std::vector<std::string>> m_warns;
  std::map<std::string, std::vector<std::string>> m_infos;

  std::ostringstream m_ss; // stream for unsorted messages sent directly to output

};

#endif  // PROJMGRLOGGER_H
