/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrLogger.h"
#include "ProjMgrUtils.h"
#include "RteFsUtils.h"

#include <iostream>

using namespace std;

static constexpr const char* PROJMGR_TOOL = " csolution: ";
static constexpr const char* PROJMGR_ERROR = "error";
static constexpr const char* PROJMGR_WARN = "warning";
static constexpr const char* PROJMGR_DEBUG = "debug";
static constexpr const char* PROJMGR_INFO = "info";

bool ProjMgrLogger::m_quiet = false;

// singleton instance
static unique_ptr<ProjMgrLogger> theProjMgrLogger = 0;

ProjMgrLogger::ProjMgrLogger(void) {
  // Reserved
}

ProjMgrLogger::~ProjMgrLogger(void) {
  // Reserved
}

ProjMgrLogger& ProjMgrLogger::Get() {
  if (!theProjMgrLogger) {
    theProjMgrLogger = make_unique<ProjMgrLogger>();
  }
  return *theProjMgrLogger.get();
}

void ProjMgrLogger::ProjMgrLogger::Clear() {
  m_errors.clear();
  m_warns.clear();
  m_infos.clear();
}

void ProjMgrLogger::Error(const string& msg, const string& context,
  const string& file, const int line, const int column) {  
  const string mark = (line > 0 ? ":" + to_string(line) : "") + (column > 0 ? ":" + to_string(column) : "");
  CollectionUtils::PushBackUniquely(m_errors[context],
    (file.empty() ? "" : RteUtils::ExtractFileName(file) + mark + " - ") + msg);
  cerr << (file.empty() ? "" : file + mark + " - ") << PROJMGR_ERROR << PROJMGR_TOOL << msg << endl;
}

void ProjMgrLogger::Warn(const string& msg, const string& context,
  const string& file, const int line, const int column) {
  const string mark = (line > 0 ? ":" + to_string(line) : "") + (column > 0 ? ":" + to_string(column) : "");
  CollectionUtils::PushBackUniquely(m_warns[context],
    (file.empty() ? "" : RteUtils::ExtractFileName(file) + mark + " - ") + msg);
  if (!m_quiet) {
    cerr << (file.empty() ? "" : file + mark + " - ") << PROJMGR_WARN << PROJMGR_TOOL << msg << endl;
  }
}

void ProjMgrLogger::Info(const string& msg, const string& context,
  const string& file, const int line, const int column) {
  const string mark = (line > 0 ? ":" + to_string(line) : "") + (column > 0 ? ":" + to_string(column) : "");
  CollectionUtils::PushBackUniquely(m_infos[context],
    (file.empty() ? "" : RteUtils::ExtractFileName(file) + mark + " - ") + msg);
  if (!m_quiet) {
    cout << (file.empty() ? "" : file + mark + " - ") << PROJMGR_INFO << PROJMGR_TOOL << msg << endl;
  }
}

void ProjMgrLogger::Debug(const string& msg) {
  if (!m_quiet) {
    cerr << PROJMGR_DEBUG << PROJMGR_TOOL << msg << endl;
  }
}
