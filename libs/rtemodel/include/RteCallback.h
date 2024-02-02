#ifndef RteCallback_H
#define RteCallback_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteCallback.h
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "XMLTree.h"

#include "RteUtils.h"

// Message box constants corresponding to Windows values of MB_XXX constants
static const unsigned int RTE_IDOK     = 1;
static const unsigned int RTE_IDCANCEL = 2;
static const unsigned int RTE_IDABORT  = 3;
static const unsigned int RTE_IDRETRY  = 4;
static const unsigned int RTE_IDIGNORE = 5;
static const unsigned int RTE_IDYES    = 6;
static const unsigned int RTE_IDNO     = 7;

static const unsigned int RTE_MB_OK               = 0x00000000;
static const unsigned int RTE_MB_OKCANCEL         = 0x00000001;
static const unsigned int RTE_MB_ABORTRETRYIGNORE = 0x00000002;
static const unsigned int RTE_MB_YESNOCANCEL      = 0x00000003;
static const unsigned int RTE_MB_YESNO            = 0x00000004;
static const unsigned int RTE_MB_RETRYCANCEL      = 0x00000005;

static const unsigned int RTE_MB_ICONHAND        = 0x00000010;
static const unsigned int RTE_MB_ICONQUESTION    = 0x00000020;
static const unsigned int RTE_MB_ICONEXCLAMATION = 0x00000030;
static const unsigned int RTE_MB_ICONASTERISK    = 0x00000040;

class RteKernel;
class RteGenerator;
/**
 * @brief Class to allow RTE to call application or API functions, defaults do nothing
*/
class RteCallback : public XMLTreeCallback
{
public:

  /**
   * @brief default constructor
  */
  RteCallback();

  /**
   * @brief clear output buffer or console
  */
  virtual void ClearOutput() {};

  /**
   * @brief output specified message
   * @param message string to output specified message
  */
  virtual void OutputMessage(const std::string& message) {};

  /**
   * @brief output collection of error messages
   * @param messages given list of error messages
  */
  virtual void OutputMessages(const std::list<std::string>& messages);

  /**
   * @brief output formatted error message including the given specifications and filename
   * @param id error ID string
   * @param message error message
   * @param file related file name to include in the message if any
  */
  virtual void Err(const std::string& id, const std::string& message, const std::string& file= RteUtils::EMPTY_STRING)  { };

  /**
   * @brief output specified error message
   * @param message error message to output
  */
  virtual void OutputErrMessage(const std::string& message) { };

  /**
   * @brief output specified warning
   * @param message warning to output
  */
  virtual void OutputWarnMessage(const std::string& message) { };

  /**
   * @brief cache specified informational message
   * @param message info to output
  */
  virtual void OutputInfoMessage(const std::string& message) { };

  /**
   * @brief display a message box with specified message
   * @param message a message to display
   * @param type message type for a message box (RTE_MB_XXX)
   * @param defaultVal default value to return if no message box is displayed (e.g. in a headless mode)
   * @return value such as RTE_IDOK, RTE_IDCANCEL, or the default value
  */
  virtual long QueryMessage(const std::string& message, unsigned int type, long defaultVal);

  /**
   * @brief display a message box
   * @param title given title for the message box
   * @param message the message to display
   * @param type message type (RTE_MB_XXX)
   * @param defaultVal default value to return if no message box is displayed (e.g. in a headless mode)
   * @return value such as RTE_IDOK, RTE_IDCANCEL or the given default value
  */
  virtual long ShowMessageBox(const std::string& title, const std::string& message, unsigned int type, long defaultVal);

  /**
   * @brief expand command or file using key sequences "@L", "%L", etc.
   * @param str string to expand
   * @return expanded string
  */
  virtual std::string ExpandString(const std::string& str);

  /**
   * @brief send message to the application main window by calling a function specific to OS
   * @param Msg message to send
   * @param wParam parameter specific to Windows SendMessage function
   * @param lParam parameter specific to Windows SendMessage function
   * @return default return zero
  */
  virtual long SendMessageMain(unsigned int Msg, unsigned long wParam, unsigned long lParam) { return 0; };

  /**
   * @brief set the application exit code that can be used to indicate error or success
   * @param code application exit code to set
  */
  virtual void SetExitCode(int code) { };

  /**
   * @brief the function is called after the specified pack is parsed
   * @param pack specified pack
   * @param success flag indicating parsing result
   * @return 0 to stop further processing, 1 to continue
  */
  virtual int PackProcessed(const std::string& pack, bool success) { return 1; }

  /**
   * @brief start displaying progress
  */
  virtual void StartProgress() { };

  /**
   * @brief increment the progress to display
   * @param percentIncrement percentage to increment in the progress display
  */
  virtual void IncrementProgress(int percentIncrement) { };

  /**
   * @brief stop displaying progress
  */
  virtual void StopProgress() { };

  /**
   * @brief merge source file specified by curFile into destination one specified by newFile
   * @param curFile source file to merge from (backup of file modified by the user)
   * @param newFile destination file (fresh copy of the file from pack)
  */
  virtual void MergeFiles(const std::string& curFile, const std::string& newFile) { };

  /**
 * @brief merge source file specified by curFile into destination one specified by newFile.
   Initial copy of the base file can be used if available.
   Default calls 2-way MergeFiles()
 * @param curFile source file to merge from (backup of file modified by the user)
 * @param newFile destination file (fresh copy of the file from pack)
 * @param baseFile a copy of the unmodified original file used initially or during last merge
*/
  virtual void MergeFiles3Way(const std::string& curFile, const std::string& newFile, const std::string& baseFile)
  {
    MergeFiles(curFile, newFile);
  };

  /**
   * @brief obtains globally defined external generator
   * @param id generator id
   * @return pointer to RteGenerator if found, nullptr otherwise
  */
  virtual RteGenerator* GetExternalGenerator(const std::string& id) const;

  /**
   * @brief get global RteCallback object
   * @return return RteCallback pointer or default one, never NULL
  */
  static RteCallback* GetGlobal();

  /**
   * @brief set RteCallback object
   * @param callback given RteCallback object
  */
  static void SetGlobal(RteCallback* callback);

  /**
   * @brief set RteKernel to use
   * @param rteKernel pointer to RteKernel object
  */
  void SetRteKernel(RteKernel* rteKernel) { m_rteKernel = rteKernel;}

  /**
   * @brief get RteKernel object
   * @return pointer RteKernel  object or nullptr;
  */
  const RteKernel* GetRteKernel() const { return m_rteKernel; };

protected:
  RteKernel* m_rteKernel;
  static RteCallback* theGlobalCallback;
};

#endif // RteCallback_H
