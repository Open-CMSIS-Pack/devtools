/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ErrLog.h"

#include <cstdio>
#include <cstring>
#include <cstdarg>

using namespace std;

const string ErrLog::NEW_LINE_STRING = "\n";

ErrLog::ErrLogDestroyer ErrLog::theErrLogDestroyer;
ErrLog* ErrLog::theErrLog = nullptr;  // the application-wide ErrLog Object
MsgTable PdscMsg::m_messageTable;
MsgTableStrict PdscMsg::m_messageTableStrict;
MsgLevel g_msgLevel;


const string& PdscMsg::GetSubstitute(const string &key) const
{
  auto it = m_substitutes.find(key);
  if(it != m_substitutes.end()) {
    return it->second;
  }

  static string errStr;
  errStr = "<";
  errStr += key;
  errStr += ">";

  return errStr;
}

void PdscMsg::SetMsg(const string &num, int32_t line, int32_t col)
{
  ClearData();

  m_num   = num;
  m_line  = line;
  m_col   = col;
}

void PdscMsg::Clear ()
{
  ClearData();

  m_substitutes.clear();
}

void PdscMsg::ClearData ()
{
  m_num   = "";
  m_line  = -1;
  m_col   = -1;
}

MsgLevel PdscMsg::GetMsgLevel() const
{
  MsgLevel level;
  MessageEntry* mgsEntry = GetMessageEntry(m_num);

  level = mgsEntry? mgsEntry->level : GetMessageEntry("M000")->level;

  if(ErrLog::Get()->IsStrictMode()) {
    if(m_messageTableStrict.find(m_num) != m_messageTableStrict.end()) {
      level = m_messageTableStrict[m_num];
    }
  }

  return level;
}

const string PdscMsg::PDSC_FormatMessage() const
{
  MessageEntry* me = GetMessageEntry(GetMsgNum());
  if(!me) {
    return GetMessageEntry("M000")->msgText;   // Message not found
  }

  int i=0;
  const char *text=0;
  const char *fmt = me->msgText.c_str();
  string msgText;

  do {
    text = fmt, i=0;
    while(*fmt && *fmt != '%') {
      fmt++, i++;            // skip to start '%'
    }
    if(!*fmt) {
      break;
    }

    msgText += string(text, i);                       // get text

    text = ++fmt, i=0;
    while(*fmt && *fmt != '%') {
      fmt++, i++;            // skip to end '%'
    }
    if(!*fmt) {
      break;
    }

    msgText += GetSubstitute(string(text, i));        // get text
  } while(*fmt++);

  if(!*fmt) {                                         // get remaining text
    msgText += text;
  }

  return msgText;
}

MessageEntry* PdscMsg::GetMessageEntry(const string& key)
{
  auto it = m_messageTable.find(key);
  if(it != m_messageTable.end()) {
    return &it->second;
  }

  return NULL;
}

void PdscMsg::AddMessages(const MsgTable& table)
{
  m_messageTable.insert(table.begin(), table.end());
}

void PdscMsg::AddMessagesStrict(const MsgTableStrict& table)
{
  m_messageTableStrict.insert(table.begin(), table.end());
}

ErrLog::ErrLog ():
m_outBuf(0),
m_ErrConsumer(nullptr),
m_ErrOutputter(nullptr),
m_quietMode(false),
m_strictMode(false),
m_msgOutLevel(MsgLevel::LEVEL_WARNING3),
m_tmpLevelVerbose(false),
m_errCnt(0),
m_warnCnt(0),
m_bSuppressAllInfo (false),
m_bSuppressAllWarning (false),
m_bSuppressAllError (false),
m_allowSuppressError (false)
{
  m_outBuf = new char[OUTBUF_SIZE];

  if(!theErrLog){
    theErrLog = this;
  }

  InitLevelStrTable();
  InitMessageTable();
}


ErrLog::~ErrLog ()
{
  if(theErrLog == this) {
    theErrLog = nullptr;
  }

  if(!m_quietMode) {
    Save();
  }

   if(m_ErrOutputter) {
    delete m_ErrOutputter;
    m_ErrOutputter = nullptr;
  }

  delete[] m_outBuf;
}

void ErrLog::InitMessageTable()
{
  PdscMsg::AddMessages(msgTable);
  PdscMsg::AddMessagesStrict(msgStrictTable);
}

IErrConsumer* ErrLog::SetErrConsumer(IErrConsumer* errConsumer)
{
  IErrConsumer* prev = m_ErrConsumer;
  m_ErrConsumer = errConsumer;

  return prev;
}

ErrOutputter* ErrLog::SetOutputter(ErrOutputter* errOutputter)
{
  ErrOutputter* prev = m_ErrOutputter;
  m_ErrOutputter = errOutputter;

  return prev;
}

void ErrLog::Save()
{
  if(!m_ErrOutputter) {
    return;
  }

  m_ErrOutputter->Save();
}

void ErrLog::SetLogFileName(const std::string &fileName)
{
  if(!m_ErrOutputter) {
    return;
  }

  m_ErrOutputter->SetLogFileName(fileName);
}

const std::list<std::string>& ErrLog::GetLogMessages() const
{
  if (m_ErrOutputter) {
    return m_ErrOutputter->GetLogMessages();
  }
  static const list<string> EMPTY_STRING_LIST;
  return EMPTY_STRING_LIST;
}

void ErrLog::ClearLogMessages()
{
  if (m_ErrOutputter) {
    m_ErrOutputter->Clear();
  }
  ResetMsgCount();
}

void ErrLog::TxtOut(const char *text, ...)
{
  if (text == NULL || *text == '\0') {
    return;
  }
  va_list   marker;
  va_start (marker, text);
  vsnprintf(m_outBuf, OUTBUF_SIZE, text, marker);
  va_end(marker);

  MsgOut(m_outBuf);
}

 void ErrLog::MsgOut (const std::string& msg)
 {
   if(m_ErrOutputter) {
     m_ErrOutputter->MsgOut(msg);
   }
 }

 void ErrLog::NewLine()
 {
   MsgOut(NEW_LINE_STRING);
 }

void ErrLog::InitLevelStrTable()
{
  m_msgLevelTable[MsgLevel::LEVEL_DEFAULT]  = "";
  m_msgLevelTable[MsgLevel::LEVEL_DEBUG]    = "";
  m_msgLevelTable[MsgLevel::LEVEL_INFO2]    = "";
  m_msgLevelTable[MsgLevel::LEVEL_PROGRESS] = "";
  m_msgLevelTable[MsgLevel::LEVEL_INFO]     = "";
  m_msgLevelTable[MsgLevel::LEVEL_WARNING3] = "INFO";
  m_msgLevelTable[MsgLevel::LEVEL_WARNING2] = "WARNING";
  m_msgLevelTable[MsgLevel::LEVEL_WARNING]  = "WARNING";
  m_msgLevelTable[MsgLevel::LEVEL_ERROR]    = "ERROR";
  m_msgLevelTable[MsgLevel::LEVEL_CRITICAL] = "CRITICAL ERROR";
  m_msgLevelTable[MsgLevel::LEVEL_TEXT]     = "";
}

const string& ErrLog::GetMsgLevelText(MsgLevel msgLevel)
{
  const string &lvlStr = m_msgLevelTable[msgLevel];

  if(lvlStr.empty()) {
    return m_msgLevelTable[MsgLevel::LEVEL_DEFAULT];
  }

  return lvlStr;
}

int ErrLog::Message (const PdscMsg &msg)
{
  PDSC_PrintMessage(msg);

  return 0;
}

int ErrLog::Message (const string &num, int32_t line, int32_t col)
{
  PdscMsg msg;

  msg.SetMsg(num, line, col);
  PDSC_PrintMessage(msg);

  return 0;
}

int ErrLog::Message (const string &num, PAIR(1), int32_t line, int32_t col)
{
  PdscMsg msg;

  msg.SetMsg(num, line, col);
  msg.AddSubstitude(substitute1);
  PDSC_PrintMessage(msg);

  return 0;
}

int ErrLog::Message (const string &num, PAIR(1), PAIR(2), int32_t line, int32_t col)
{
  PdscMsg msg;

  msg.SetMsg(num, line, col);
  msg.AddSubstitude(substitute1);
  msg.AddSubstitude(substitute2);
  PDSC_PrintMessage(msg);

  return 0;
}

int ErrLog::Message (const string &num, PAIR(1), PAIR(2), PAIR(3), int32_t line, int32_t col)
{
  PdscMsg msg;

  msg.SetMsg(num, line, col);
  msg.AddSubstitude(substitute1);
  msg.AddSubstitude(substitute2);
  msg.AddSubstitude(substitute3);
  PDSC_PrintMessage(msg);

  return 0;
}

int ErrLog::Message (const string &num, PAIR(1), PAIR(2), PAIR(3), PAIR(4), int32_t line, int32_t col)
{
  PdscMsg msg;

  msg.SetMsg(num, line, col);
  msg.AddSubstitude(substitute1);
  msg.AddSubstitude(substitute2);
  msg.AddSubstitude(substitute3);
  msg.AddSubstitude(substitute4);
  PDSC_PrintMessage(msg);

  return 0;
}

int ErrLog::Message (const string &num, PAIR(1), PAIR(2), PAIR(3), PAIR(4), PAIR(5), int32_t line, int32_t col)
{
  PdscMsg msg;

  msg.SetMsg(num, line, col);
  msg.AddSubstitude(substitute1);
  msg.AddSubstitude(substitute2);
  msg.AddSubstitude(substitute3);
  msg.AddSubstitude(substitute4);
  msg.AddSubstitude(substitute5);
  PDSC_PrintMessage(msg);

  return 0;
}

int ErrLog::Message (const string &num, PAIR(1), PAIR(2), PAIR(3), PAIR(4), PAIR(5), PAIR(6), int32_t line, int32_t col)
{
  PdscMsg msg;

  msg.SetMsg(num, line, col);
  msg.AddSubstitude(substitute1);
  msg.AddSubstitude(substitute2);
  msg.AddSubstitude(substitute3);
  msg.AddSubstitude(substitute4);
  msg.AddSubstitude(substitute5);
  msg.AddSubstitude(substitute6);
  PDSC_PrintMessage(msg);

  return 0;
}

int ErrLog::Message (const string &num, PAIR(1), PAIR(2), PAIR(3), PAIR(4), PAIR(5), PAIR(6), PAIR(7), int32_t line, int32_t col)
{
  PdscMsg msg;

  msg.SetMsg(num, line, col);
  msg.AddSubstitude(substitute1);
  msg.AddSubstitude(substitute2);
  msg.AddSubstitude(substitute3);
  msg.AddSubstitude(substitute4);
  msg.AddSubstitude(substitute5);
  msg.AddSubstitude(substitute6);
  msg.AddSubstitude(substitute7);
  PDSC_PrintMessage(msg);

  return 0;
}

int ErrLog::Message (const string &num, PAIR(1), PAIR(2), PAIR(3), PAIR(4), PAIR(5), PAIR(6), PAIR(7), PAIR(8), int32_t line, int32_t col)
{
  PdscMsg msg;

  msg.SetMsg(num, line, col);
  msg.AddSubstitude(substitute1);
  msg.AddSubstitude(substitute2);
  msg.AddSubstitude(substitute3);
  msg.AddSubstitude(substitute4);
  msg.AddSubstitude(substitute5);
  msg.AddSubstitude(substitute6);
  msg.AddSubstitude(substitute7);
  msg.AddSubstitude(substitute8);
  PDSC_PrintMessage(msg);

  return 0;
}

int ErrLog::Message (const string &num, PAIR(1), PAIR(2), PAIR(3), PAIR(4), PAIR(5), PAIR(6), PAIR(7), PAIR(8), PAIR(9), int32_t line, int32_t col)
{
  PdscMsg msg;

  msg.SetMsg(num, line, col);
  msg.AddSubstitude(substitute1);
  msg.AddSubstitude(substitute2);
  msg.AddSubstitude(substitute3);
  msg.AddSubstitude(substitute4);
  msg.AddSubstitude(substitute5);
  msg.AddSubstitude(substitute6);
  msg.AddSubstitude(substitute7);
  msg.AddSubstitude(substitute8);
  msg.AddSubstitude(substitute9);
  PDSC_PrintMessage(msg);

  return 0;
}

int ErrLog::Message (const string &num, PAIR(1), PAIR(2), PAIR(3), PAIR(4), PAIR(5), PAIR(6), PAIR(7), PAIR(8), PAIR(9), PAIR(10), int32_t line, int32_t col)
{
  PdscMsg msg;

  msg.SetMsg(num, line, col);
  msg.AddSubstitude(substitute1);
  msg.AddSubstitude(substitute2);
  msg.AddSubstitude(substitute3);
  msg.AddSubstitude(substitute4);
  msg.AddSubstitude(substitute5);
  msg.AddSubstitude(substitute6);
  msg.AddSubstitude(substitute7);
  msg.AddSubstitude(substitute8);
  msg.AddSubstitude(substitute9);
  msg.AddSubstitude(substitute10);
  PDSC_PrintMessage(msg);

  return 0;
}

bool ErrLog::CheckSuppressMessages()
{
  if(ErrLog::Get()->IsAllowSuppressError()) {
    return true;
  }

  for(auto it = m_diagSuppressMsg.begin(); it != m_diagSuppressMsg.end(); ) {
    auto itcurrent = it++;
    const string& num = *itcurrent;
    PdscMsg msg;
    msg.SetMsg(num, -1, -1);

    MsgLevel msgLevel = msg.GetMsgLevel();
    if(msgLevel == MsgLevel::LEVEL_ERROR) {
      LogMsg("M017", MSG(num));
       m_diagSuppressMsg.erase(itcurrent);
    }
  }

  return true;
}

bool ErrLog::SuppressMessage (const std::string &msgNum)
{
  bool suppress = false;
  const char *numStr = msgNum.c_str();

  int num = atoi(&numStr[1]);
  if(num < 40) {
    return false;
  }

  if(!m_diagShowOnlyMsg.empty()) {
    auto it = m_diagShowOnlyMsg.find(msgNum);
    if(it == m_diagShowOnlyMsg.end()) suppress = true;
  }
  else {
    auto it = m_diagSuppressMsg.find(msgNum);
    if(it != m_diagSuppressMsg.end()) suppress = true;
  }

  return suppress;
}

void ErrLog::PDSC_PrintMessage(const PdscMsg &msg)
{
  static int prevWasMsg = 0, prevSuppressed = 0;

  MsgLevel msgLevel = msg.GetMsgLevel ();
  g_msgLevel = msgLevel;

  if(m_bSuppressAllInfo) {
    if(msgLevel == MsgLevel::LEVEL_WARNING3 || msgLevel == MsgLevel::LEVEL_INFO || msgLevel == MsgLevel::LEVEL_INFO2) {
      prevSuppressed = 1;
      return;
    }
  }
  if(m_bSuppressAllWarning) {
    if(msgLevel == MsgLevel::LEVEL_WARNING || msgLevel == MsgLevel::LEVEL_WARNING2) {
      prevSuppressed = 1;
      return;
    }
  }

  if(SuppressMessage(msg.GetMsgNum())) {
    prevSuppressed = 1;
    return;
  }
  if(prevSuppressed && msg.GetMsgNum() == "M010") {   // also suppress " OK"
    return;
  }

  // Do not count suppressed Messages
  if(msgLevel == MsgLevel::LEVEL_CRITICAL || msgLevel == MsgLevel::LEVEL_ERROR  ) {
    IncErrCnt ();
  } else if(msgLevel >= MsgLevel::LEVEL_WARNING2 && msgLevel <= MsgLevel::LEVEL_WARNING) {
    IncWarnCnt();
  }

  if(m_quietMode) {
    return;
  }

  if (m_ErrConsumer && m_ErrConsumer->Consume(msg, m_fileName)) {
    return;
  }

  prevSuppressed = 0;
  int lineNo = msg.GetLineNo();
  unsigned doCRLF = msg.GetCrLf();

  if(m_tmpLevelVerbose || msgLevel >= m_msgOutLevel) {
    string message = msg.PDSC_FormatMessage();

    if(doCRLF & CRLF_B) {
      NewLine();
    }

    if(msgLevel <= MsgLevel::LEVEL_INFO || msgLevel == MsgLevel::LEVEL_TEXT) {
      // Text only
      if(prevWasMsg) {
        NewLine();    // print newline
      }
      prevWasMsg = 0;
      string numStr = msg.GetMsgNum();
      int num = atoi(&numStr.c_str()[1]);

      if(num >= 40) {
        TxtOut("%s: ", msg.GetMsgNum().c_str());     // print MSG num
      }
      MsgOut(message);
      if(lineNo != -1) {
        TxtOut(" (Line %i)", lineNo);
      }
    }
    else {
      prevWasMsg = 1;
      // Line1: *** ERROR M001 : (Line 42) InputFile.pdsc
      // Line1: *** WARNING M002 : (Line 42) InputFile.pdsc
      NewLine();
      TxtOut("*** %s %s:", GetMsgLevelText(msgLevel).c_str(), msg.GetMsgNum().c_str());

      if(!m_fileName.empty()) {
        TxtOut(" %s", m_fileName.c_str());
      }
      if(lineNo != -1) {
        TxtOut(" (Line %i) ", lineNo);
      }

      // Line2: Message
      TxtOut("\n  ");
      TxtOut("%s", message.c_str());
    }
    if(doCRLF & CRLF_E) {
      NewLine();
    }
  }
}

int ErrLog::WillMsgPrint(const string &num) const
{
  PdscMsg msg;
  msg.SetMsg  (num, -1, -1);

  if(msg.GetMsgLevel() >= m_msgOutLevel) {
    return 1;
  }

  return 0;
}

// Utils
string ErrLog::CreateDecNum(unsigned int num)
{
  return std::to_string(num);
}

// end of ErrLog.cpp
