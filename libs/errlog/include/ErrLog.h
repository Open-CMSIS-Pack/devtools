/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ERR_LOG_H
#define ERR_LOG_H

#include "ErrOutputter.h"

#include <stdint.h>
#include <string>
#include <map>
#include <list>
#include <set>


#define   OUTBUF_SIZE     (1024 * 128)

// Flags
#define CRLF_NO       (0<<0)      // no newline
#define CRLF_B        (1<<1)      // newline before
#define CRLF_E        (1<<2)      // newline after
#define CRLF_BE       (CRLF_B | CRLF_E)


enum class MsgLevel {
  LEVEL_DEFAULT   = 1,
// --debug
  LEVEL_DEBUG     = 2,     // programm debug information
// --verbose2
  LEVEL_INFO2     = 3,     // all processing infos
// --verbose
  LEVEL_PROGRESS  = 4,     // Progress Messages or Progress Info (i.e. "."), then shifted to LEVEL_TEXT
  LEVEL_INFO      = 5,     // Information
// -w3
  LEVEL_WARNING3  = 6,     // Warnings -w3
// -w2
  LEVEL_WARNING2  = 7,     // Warnings -w2
// -w1
  LEVEL_WARNING   = 8,     // Warnings -w1
// -w0
  LEVEL_ERROR     = 9,     // Errors
  LEVEL_CRITICAL  = 10,    // Critical Errors
  LEVEL_TEXT      = 11,    // Always printed if not --quiet
// --quiet
};


struct MessageEntry {
  MessageEntry() : level(MsgLevel::LEVEL_DEFAULT), flags(0) {};
  MessageEntry(MsgLevel l, uint32_t f, std::string m): level(l), flags(f), msgText(m) {};

  MsgLevel level;              // warning / error level
  uint32_t flags;              // (1<<0) = print filename
  std::string msgText;
};



#define VAL(key, value)   std::make_pair(key,     value)

#define LINE(value)       std::make_pair("LINE",      ErrLog::CreateDecNum(value))
#define ERR(value)        std::make_pair("ERR",       ErrLog::CreateDecNum(value))
#define WARN(value)       std::make_pair("WARN",      ErrLog::CreateDecNum(value))
#define TIME(value)       std::make_pair("TIME",      ErrLog::CreateDecNum(value))
#define NUM2(value)       std::make_pair("NUM2",      ErrLog::CreateDecNum(value))
#define NUM(value)        std::make_pair("NUM",       ErrLog::CreateDecNum(value))
#define MSB(value)        std::make_pair("MSB",       ErrLog::CreateDecNum(value))
#define LSB(value)        std::make_pair("LSB",       ErrLog::CreateDecNum(value))

#define PATH(path)        VAL("PATH",     path)
#define TXT2(txt)         VAL("TEXT2",    txt)
#define TXT3(txt)         VAL("TEXT3",    txt)
#define TXT4(txt)         VAL("TEXT4",    txt)
#define TXT5(txt)         VAL("TEXT5",    txt)
#define TXT(txt)          VAL("TEXT",     txt)
#define NUMTXT(txt)       VAL("NUM",      txt)
#define COND(cond)        VAL("COND",     cond)
#define EXT(ext)          VAL("EXT",      ext)
#define COMP(comp)        VAL("COMP",     comp)
#define VENDOR(vendor)    VAL("VENDOR",   vendor)
#define VENDOR2(vendor)   VAL("VENDOR2",  vendor)
#define MCU(mcu)          VAL("MCU",      mcu)
#define MCU2(mcu)         VAL("MCU2",     mcu)
#define MSG(msg)          VAL("MSG",      msg)
#define CCLASS(cclass)    VAL("CCLASS",   cclass)
#define CGROUP(cgroup)    VAL("CGROUP",   cgroup)
#define CSUB(csub)        VAL("CSUB",     csub)
#define CVER(cver)        VAL("CVER",     cver)
#define APIVER(apiVer)    VAL("APIVER",   apiVer)
#define NAME(name)        VAL("NAME",     name)
#define NAME2(name)       VAL("NAME2",    name)
#define NAME3(name)       VAL("NAME3",    name)
#define ITEM(item)        VAL("ITEM",     item)
#define LEVEL(level)      VAL("LEVEL",    level)
#define LEVEL2(level)     VAL("LEVEL2",   level)
#define ORIGNAME(name)    VAL("ORIGNAME", name)
#define TYP(type)         VAL("TYPE",     type)
#define URL(url)          VAL("URL",      url)
#define SECTION(sec)      VAL("SECTION",  sec)
#define SPACE(space)      VAL("SPACE",    space)
#define TAG(tag)          VAL("TAG",      tag)
#define TAG2(tag)         VAL("TAG2",     tag)
#define TAG3(tag)         VAL("TAG3",     tag)
#define VALUE(val)        VAL("VALUE",    val)
#define ACCESS2(acc)      VAL("ACCESS2",  acc)
#define ACCESS(acc)       VAL("ACCESS",   acc)
#define USAGE2(usage)     VAL("USAGE2",   usage)
#define USAGE(usage)      VAL("USAGE",    usage)
#define CHR(c)            VAL("CHAR",     c)
#define RELEASEDATE(date) VAL("RELEASEDATE", date)
#define RELEASEVER(ver)   VAL("RELEASEVER", ver)
#define LATESTVER(ver)    VAL("LATESTVER", ver)
#define LATESTDATE(date)  VAL("LATESTDATE", date)
#define TODAYDATE(date)   VAL("TODAYDATE", date)
#define COMPILER(comp)    VAL("COMPILER", comp)
#define OPTION(opt)       VAL("OPTION", opt)

#define THISLEVEL()       VAL("LEVEL",    this->GetSvdLevelStr(this->GetSvdLevel()))

#define PAIR(x)           std::pair<std::string, std::string> substitute##x

#define SUBS_MAP          std::map  <std::string, std::string>
#define SUBS_PAIR         std::pair <std::string, std::string>

typedef std::map  <std::string, MessageEntry> MsgTable;
typedef std::map  <std::string, MsgLevel>   MsgTableStrict;


class PdscMsg {
public:
                                PdscMsg             () : m_line(-1), m_col(-1) {}
  virtual                      ~PdscMsg             () {}
  virtual   void                Clear               ();
  virtual   void                ClearData           ();

  const std::string&            GetSubstitute       (const std::string &key) const;
  const SUBS_MAP               &GetSubstitutes      () const                                            { return m_substitutes;             }
  void                          AddSubstitude       (const std::string &key, const std::string &val)    { m_substitutes[key] = val;         }
  void                          AddSubstitude       (const SUBS_PAIR& substitude)                       { m_substitutes.insert(substitude); }
  const std::string&            GetMsgNum           () const                                            { return m_num;                     }
  void                          SetMsg              (const std::string &msg, int32_t line, int32_t col);
  const std::string             PDSC_FormatMessage  () const;
  MsgLevel                      GetMsgLevel         () const;
  int32_t                       GetLineNo           () const { return(m_line); }
  uint32_t                      GetCrLf             () const { return(GetMessageEntry(m_num)? GetMessageEntry(m_num)->flags & CRLF_BE : 0); }

  static void AddMessages       (const MsgTable& table);
  static void AddMessagesStrict (const MsgTableStrict& table);

protected:
  static MessageEntry          *GetMessageEntry(const std::string& key);

  std::string             m_num;               // Warning / Error number
  int32_t                 m_line;              // Message Line number (set -1 when unused)
  int32_t                 m_col;               // Message Col number  (set -1 when unused)
  SUBS_MAP                m_substitutes;       // Substitutes for %SUBSTITUDE% commands in Message
  static MsgTable         m_messageTable;
  static MsgTableStrict   m_messageTableStrict;
};

class IErrConsumer  // an abstract interface class to redirect messages
{
public:
  virtual ~IErrConsumer() {}
  // consumes (outputs or ignores a message), returns true if consumed and should not be output in the regular way
  virtual bool Consume(const PdscMsg& msg, const std::string& fileName) = 0;
};


class ErrLog {
public:
   ErrLog ();

protected:
  virtual ~ErrLog (); // protected destructor to prevent heap objects

// singleton operation:
public:
  // global application object
  static ErrLog* Get() {
    if (!theErrLog) {
      theErrLog = new ErrLog();
    }
    return theErrLog;
  }

public:
  void Save();
  virtual void InitMessageTable();

  IErrConsumer* GetErrConsumer() const { return m_ErrConsumer; }
  IErrConsumer* SetErrConsumer(IErrConsumer* errConsumer);

  ErrOutputter* GetOutputter() const { return m_ErrOutputter; }
  ErrOutputter* SetOutputter(ErrOutputter* errOutputter);

  virtual void SetLogFileName(const std::string &fileName);
  virtual void TxtOut(const char *text, ...);
  virtual void MsgOut(const std::string& msg);
  virtual void NewLine();

  const std::list<std::string>& GetLogMessages() const;
  void ClearLogMessages();

  int           GetErrCnt             () const {return(m_errCnt);    }
  void          IncErrCnt             () { m_errCnt++;               }
  void          ResetErrCount         () { m_errCnt = 0;             }
  int           GetWarnCnt            () const { return(m_warnCnt);  }
  void          IncWarnCnt            () { m_warnCnt++;              }
  void          ResetWarnCount        () { m_warnCnt = 0;            }
  void          ResetMsgCount         () { m_errCnt = m_warnCnt = 0; }

  int           Message               (const PdscMsg &msg);
  int           Message               (const std::string &num,                                                                                             int32_t line=-1, int32_t col=-1);
  int           Message               (const std::string &num, PAIR(1),                                                                                    int32_t line=-1, int32_t col=-1);
  int           Message               (const std::string &num, PAIR(1), PAIR(2),                                                                           int32_t line=-1, int32_t col=-1);
  int           Message               (const std::string &num, PAIR(1), PAIR(2), PAIR(3),                                                                  int32_t line=-1, int32_t col=-1);
  int           Message               (const std::string &num, PAIR(1), PAIR(2), PAIR(3), PAIR(4),                                                         int32_t line=-1, int32_t col=-1);
  int           Message               (const std::string &num, PAIR(1), PAIR(2), PAIR(3), PAIR(4), PAIR(5),                                                int32_t line=-1, int32_t col=-1);
  int           Message               (const std::string &num, PAIR(1), PAIR(2), PAIR(3), PAIR(4), PAIR(5), PAIR(6),                                       int32_t line=-1, int32_t col=-1);
  int           Message               (const std::string &num, PAIR(1), PAIR(2), PAIR(3), PAIR(4), PAIR(5), PAIR(6), PAIR(7),                              int32_t line=-1, int32_t col=-1);
  int           Message               (const std::string &num, PAIR(1), PAIR(2), PAIR(3), PAIR(4), PAIR(5), PAIR(6), PAIR(7), PAIR(8),                     int32_t line=-1, int32_t col=-1);
  int           Message               (const std::string &num, PAIR(1), PAIR(2), PAIR(3), PAIR(4), PAIR(5), PAIR(6), PAIR(7), PAIR(8), PAIR(9),            int32_t line=-1, int32_t col=-1);
  int           Message               (const std::string &num, PAIR(1), PAIR(2), PAIR(3), PAIR(4), PAIR(5), PAIR(6), PAIR(7), PAIR(8), PAIR(9), PAIR(10),  int32_t line=-1, int32_t col=-1);

  void          SetLevel              (MsgLevel level)                  { m_msgOutLevel = level;                    }
  MsgLevel      GetLevel              () const                          { return(m_msgOutLevel);                    }
  void          SetTmpLevelVerbose    (bool enable)                     { m_tmpLevelVerbose = enable;               }
  void          SetLevelToWarning     ()                                { SetLevel(MsgLevel::LEVEL_WARNING);        }
  void          SetLevelToError       ()                                { SetLevel(MsgLevel::LEVEL_ERROR);          }
  void          SetFileName           (const std::string &fileName)     { m_fileName = fileName;                    }
  void          SetFormatedMsg        (const std::string &msg)          { m_msg = msg;                              }
  void          PDSC_PrintMessage     (const PdscMsg &msg);
  int           WillMsgPrint          (const std::string &num) const;
  void          AddDiagSuppress       (std::string msgNum)              { m_diagSuppressMsg.insert(msgNum);         }
  void          AddDiagShowOnly       (std::string msgNum)              { m_diagShowOnlyMsg.insert(msgNum);         }

  bool          SuppressMessage       (const std::string &msgNum);

  void          SetQuietMode          (bool quiet = true)               { m_quietMode   = quiet;                    }
  bool          IsQuietMode           () const                          { return m_quietMode;                       }
  void          SetStrictMode         (bool strict = true)              { m_strictMode = strict;                    }
  bool          IsStrictMode          () const                          { return m_strictMode;                      }

  void          SetAllowSuppressError (bool allow = true)               { m_allowSuppressError = allow;             }
  bool          IsAllowSuppressError  () const                          { return m_allowSuppressError;              }

  void          InitLevelStrTable     ();

  void          SuppressAllInfo       (bool suppress = true)  { m_bSuppressAllInfo    = suppress; }
  void          SuppressAllWarning    (bool suppress = true)  { m_bSuppressAllWarning = suppress; }
  void          SuppressAllError      (bool suppress = true)  { m_bSuppressAllError   = suppress; }

  const std::string& GetMsgLevelText  (MsgLevel msgLevel);

  bool          CheckSuppressMessages ();

  // Utils
  static std::string CreateDecNum(unsigned int num);
  static const std::string NEW_LINE_STRING;

protected:
  char*                   m_outBuf;
  IErrConsumer*           m_ErrConsumer;     // not deleted in destructor
  ErrOutputter*           m_ErrOutputter;    // gets deleted in desructor!

  bool                    m_quietMode;
  bool                    m_strictMode;

  MsgLevel                m_msgOutLevel;
  bool                    m_tmpLevelVerbose;
  std::string             m_fileName;
  std::string             m_msg;                    // Formated Message
  int                     m_errCnt;
  int                     m_warnCnt;
  std::set<std::string>   m_diagSuppressMsg;
  std::set<std::string>   m_diagShowOnlyMsg;

  std::map<MsgLevel, std::string>   m_msgLevelTable;

  bool                  m_bSuppressAllInfo;
  bool                  m_bSuppressAllWarning;
  bool                  m_bSuppressAllError;
  bool                  m_allowSuppressError;

private:
  class ErrLogDestroyer {
  public:
    ~ErrLogDestroyer() { ErrLog::Destroy(); }
  };

  static void  Destroy() { delete theErrLog; theErrLog = nullptr; }
  static ErrLogDestroyer theErrLogDestroyer;
  static ErrLog* theErrLog;  // the application-wide ErrLog Object

  static const MsgTable msgTable;
  static const MsgTableStrict msgStrictTable;
};


#define LogMsg            ErrLog::Get()->Message

#endif
