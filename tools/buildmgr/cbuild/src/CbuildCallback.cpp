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

  auto replace = [](string &str, const string &toReplace, const string &with) {
    size_t pos = str.find(toReplace);
    while (pos != string::npos) {
      str.replace(pos, 2, with);
      pos = str.find(toReplace, pos + toReplace.size());
    }
  };

  string res(str);
  replace(res, "$P", prjPath);
  replace(res, "#P", prjPathFile);
  replace(res, "$S", rtePath);
  replace(res, "$D", deviceName);

  return res;
}
