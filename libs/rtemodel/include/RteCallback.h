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

// Message box constants
// correspond to Windows values of MB_XXX constants

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

// Class to allow RTE to call application or API functions, defaults do nothing
class RteCallback : public XMLTreeCallback
{
public:

  virtual void ClearOutput() {};

  virtual void OutputMessage(const std::string& message) {};
  virtual void OutputMessages(const std::list<std::string>& messages);

  virtual void Err(const std::string& id, const std::string& message, const std::string& file= RteUtils::EMPTY_STRING)  { };

  virtual void OutputErrMessage(const std::string& message) { };
  virtual void OutputWarnMessage(const std::string& message) { };
  virtual void OutputInfoMessage(const std::string& message) { };

   // Displays message box, type is RTE_MB_XXX, return values RTE_IDOK, etc.
  virtual long QueryMessage(const std::string& message, unsigned int type, long defaultVal);
  // Displays message box, type is RTE_MB_XXX, return values RTE_IDOK, etc.
  virtual long ShowMessageBox(const std::string& title, const std::string& message, unsigned int type, long defaultVal);
  // expands command or file using key sequences "@L", "%L", etc.
  virtual std::string ExpandString(const std::string& str) { return str; } // default simply returns the input string

  // Send message to the application main window
  virtual long SendMessageMain(unsigned int Msg, unsigned long wParam, unsigned long lParam) { return 0; };
  virtual void SetExitCode(int code) { };

  // the function is called after each pack is parsed, return 0 to stop further processing
  virtual int PackProcessed(const std::string& pack, bool success) { return 1; } // default returns 1 to continue processing

  virtual void StartProgress() { };
  virtual void IncrementProgress(int percentIncrement) { };
  virtual void StopProgress() { };

  virtual void MergeFiles(const std::string& curFile, const std::string& newFile) { };

  // global callback
  static RteCallback* GetGlobal(); // return global callback or default one, never NULL
  static void SetGlobal(RteCallback* callback);
protected:
  static RteCallback* theGlobalCallback;
};

#endif // RteCallback_H
