/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRCALLBACK_H
#define PROJMGRCALLBACK_H

#include "RteCallback.h"

/**
 * @brief extension to RTE Callback
*/
class ProjMgrCallback : public RteCallback {
public:
  /**
   * @brief class constructor
  */
  ProjMgrCallback();

  /**
   * @brief class destructor
  */
  virtual ~ProjMgrCallback();

  /**
   * @brief obtain error messages
   * @return list of all error messages
  */
  const std::list<std::string>& GetErrorMessages() const {
    return m_errorMessages;
  }

  /**
 * @brief obtain warning messages
 * @return list of all warning messages
*/
  const std::list<std::string>& GetWarningMessages() const {
    return m_warningMessages;
  }

  /**
   * @brief clear all error messages
  */
  void ClearErrorMessages() {
    m_errorMessages.clear();
  }

  /**
   * @brief clear all warning messages
  */
  void ClearWarningMessages() {
    m_warningMessages.clear();
  }

  /**
   * @brief clear all output messages
  */
  void ClearOutput() override;

  /**
   * @brief add message to output message list
   * @param message error message to be added
  */
  void OutputErrMessage(const std::string& message) override;

  /**
   * @brief output specified message
   * @param message string to output specified message
  */
  void OutputMessage(const std::string& message) override;

  /**
   * @brief create error message string and add it to message list
   * @param id error Id
   * @param message error message
   * @param object file in which error occured
  */
  void Err(const std::string& id, const std::string& message, const std::string& object = RteUtils::EMPTY_STRING) override;

protected:
  std::list<std::string> m_errorMessages;
  std::list<std::string> m_warningMessages;

};
#endif // PROJMGRCALLBACK_H
