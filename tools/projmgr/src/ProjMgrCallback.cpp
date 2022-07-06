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

void ProjMgrCallback::OutputMessage(const string& message)
{
  if (!message.empty()) {
    m_warningMessages.push_back(message);
  }
}

// end of ProjMgrCallback.cpp
