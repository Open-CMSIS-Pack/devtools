/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteCallback.cpp
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteCallback.h"

#include "RteKernel.h"
#include "RteProject.h"
#include "RteTarget.h"
#include "RteBoard.h"

#include "RteUtils.h"

using namespace std;

RteCallback* RteCallback::theGlobalCallback = nullptr;

RteCallback::RteCallback():
m_rteKernel(nullptr)
{

}


void RteCallback::OutputMessages(const std::list<string>& messages)
{
    for (auto& msg: messages) {
      OutputMessage(msg);
    }
};

long RteCallback::QueryMessage(const string& message, unsigned int type, long defaultVal)
{
  OutputMessage(message);
  return defaultVal;
}

long RteCallback::ShowMessageBox(const string& title, const string& message, unsigned int type, long defaultVal)
{
  OutputMessage(message);
  return defaultVal;
}

RteGenerator* RteCallback::GetExternalGenerator(const std::string& id) const
{
  if(GetRteKernel()) {
    return GetRteKernel()->GetExternalGenerator(id);
  }
  return nullptr;
}

RteCallback* RteCallback::GetGlobal()
{
  if(theGlobalCallback) {
    return theGlobalCallback;
  }
  static RteCallback nullCallback;
  return &nullCallback;
}

void RteCallback::SetGlobal(RteCallback* callback)
{
  theGlobalCallback = callback;
}

string RteCallback::ExpandString(const string& str) {

  const RteKernel* kernel = GetRteKernel();
  if (!kernel) {
    return RteUtils::EMPTY_STRING;
  }

  const auto activeProject = kernel->GetActiveProject();
  if (!activeProject) {
    return RteUtils::EMPTY_STRING;
  }
  const auto activeTarget = kernel->GetActiveTarget();
  if (!activeTarget) {
    return RteUtils::EMPTY_STRING;
  }
  const auto devicePackage = activeTarget->GetDevicePackage();
  if (!devicePackage) {
    return RteUtils::EMPTY_STRING;
  }

  const string& prjPath(activeProject->GetProjectPath());
  const string& prjPathFile(prjPath + activeProject->GetName() + ".cprj");
  const string& packPath(devicePackage->GetAbsolutePackagePath());
  const string& deviceName(activeTarget->GetDeviceName());
  const string& generatorInputFile(activeTarget->GetGeneratorInputFile());

  if (prjPath.empty() || prjPathFile.empty() || packPath.empty() || deviceName.empty()) {
    return RteUtils::EMPTY_STRING;
  }

  const string& boardName = activeTarget->GetAttribute("Bname");

  string res(str);
  RteUtils::ReplaceAll(res, "$P", prjPath);
  RteUtils::ReplaceAll(res, "#P", prjPathFile);
  RteUtils::ReplaceAll(res, "$S", packPath);
  RteUtils::ReplaceAll(res, "$D", deviceName);
  RteUtils::ReplaceAll(res, "$B", boardName);
  RteUtils::ReplaceAll(res, "$G", generatorInputFile);

  return res;
}

// end of RteCallback.cpp
