/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __ERR_OUTPUTTER_H__
#define __ERR_OUTPUTTER_H__

#include <string>
#include <list>


// outputs error strings destination(s) (file, console, pipe, etc.)
class ErrOutputter
{
public:
  virtual ~ErrOutputter() {}

  virtual void MsgOut(const std::string& msg) { // called by ErrLog:msgOut
    // the default implementation simply saves messages into internal buffer
    m_logText.push_back(msg);
  }

  virtual void Save() { // called by ErrLog::Save saves/outputs collected messages
    // default does nothing
  }

  virtual void Clear() { m_logText.clear(); }  // clears collected strings

  void SetLogFileName(const std::string &fileName) { m_logFileName = fileName; }
  const std::string& GetLogFileName() const { return m_logFileName; }

  const std::list<std::string>&  GetLogMessages() const { return m_logText; }

protected:
  std::list<std::string>  m_logText;
  std::string             m_logFileName;
};


#endif //__ERR_OUTPUTTER_H__
