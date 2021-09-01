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

using namespace std;

RteCallback* RteCallback::theGlobalCallback = nullptr;


void RteCallback::OutputMessages(const std::list<string>& messages)
{
    for (std::list<string>::const_iterator it = messages.begin(); it != messages.end(); it++) {
      OutputMessage(*it);
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

 // global callback
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
// end of RteCallback.cpp
