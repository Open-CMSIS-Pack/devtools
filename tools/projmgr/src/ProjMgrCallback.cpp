/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrCallback.h"
#include "ProjMgrKernel.h"

using namespace std;

ProjMgrCallback::ProjMgrCallback() : RteCallback()
{
}

ProjMgrCallback::~ProjMgrCallback()
{
  ClearErrorMessages();
}

void ProjMgrCallback::ClearOutput()
{
  ClearErrorMessages();
}

void ProjMgrCallback::Err(const string& id, const string& message, const string& file)
{
  string msg = "Error " + id;
  if (!message.empty()) {
    msg += ": " + message;
  }
  if (!file.empty()) {
    msg += ": " + file;
  }
  OutputErrMessage(msg);
}

void ProjMgrCallback::OutputErrMessage(const string& message)
{
  if(!message.empty()) {
    m_errorMessages.push_back(message);
  }
}

string ProjMgrCallback::ExpandString(const string& str) {

  const auto kernel = ProjMgrKernel::Get();
  if (!kernel) {
    return RteUtils::EMPTY_STRING;
  }
  const auto model = kernel->GetGlobalModel();
  if (!model) {
    return RteUtils::EMPTY_STRING;
  }
  const auto activeProject = model->GetActiveProject();
  if (!activeProject) {
    return RteUtils::EMPTY_STRING;
  }
  const auto activeTarget = activeProject->GetActiveTarget();
  if (!activeTarget) {
    return RteUtils::EMPTY_STRING;
  }
  const string& prjPath(activeProject->GetProjectPath());
  const string& prjPathFile(prjPath + activeProject->GetName() + ".cprj");
  const string& packPath(activeTarget->GetDevicePackage()->GetAbsolutePackagePath());
  const string& deviceName(activeProject->GetActiveTarget()->GetDeviceName());

  if (prjPath.empty() || prjPathFile.empty() || packPath.empty() || deviceName.empty()) {
    return RteUtils::EMPTY_STRING;
  }

  auto replace = [](string& str, const string& toReplace, const string& with) {
    size_t pos = str.find(toReplace);
    while (pos != string::npos) {
      str.replace(pos, 2, with);
      pos = str.find(toReplace, pos + toReplace.size());
    }
  };

  string res(str);
  replace(res, "$P", prjPath);
  replace(res, "#P", prjPathFile);
  replace(res, "$S", packPath);
  replace(res, "$D", deviceName);

  return res;
}
