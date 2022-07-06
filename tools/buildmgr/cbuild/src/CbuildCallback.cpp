/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CbuildCallback.h"

#include "CbuildKernel.h"
#include "CbuildModel.h"

using namespace std;

CbuildCallback::CbuildCallback() : RteCallback()
{
}

CbuildCallback::~CbuildCallback()
{
  ClearErrorMessages();
}

void CbuildCallback::ClearOutput()
{
  ClearErrorMessages();
}

void CbuildCallback::Err(const string& id, const string& message, const string& file)
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

void CbuildCallback::OutputErrMessage(const string& message)
{
  if(!message.empty()) {
    m_errorMessages.push_back(message);
  }
}

string CbuildCallback::ExpandString(const string& str) {
  const CbuildModel* model = CbuildKernel::Get()->GetModel();
  const string &prjPath(model->GetProjectPath());
  const string &prjPathFile(model->GetProjectFile());
  const string &rtePath(model->GetRtePath());
  const string &deviceName(model->GetDeviceName());

  if (prjPath.empty() || prjPathFile.empty() || rtePath.empty() || deviceName.empty()) {
    return RteUtils::EMPTY_STRING;
  }

  const RteTarget* activeTarget = model->GetTarget();
  const string& boardName = activeTarget ? activeTarget->GetAttribute("Bname") : RteUtils::EMPTY_STRING;

  string res(str);
  RteUtils::ReplaceAll(res, "$P", prjPath);
  RteUtils::ReplaceAll(res, "#P", prjPathFile);
  RteUtils::ReplaceAll(res, "$S", rtePath);
  RteUtils::ReplaceAll(res, "$D", deviceName);
  RteUtils::ReplaceAll(res, "$B", boardName);

  return res;
}
