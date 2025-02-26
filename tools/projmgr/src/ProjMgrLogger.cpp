/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrLogger.h"
#include "ProjMgrUtils.h"

#include "RteFsUtils.h"
#include "RteUtils.h"

#include <iostream>

using namespace std;

static constexpr const char* PROJMGR_TOOL = " csolution: ";
static constexpr const char* PROJMGR_ERROR = "error";
static constexpr const char* PROJMGR_WARN = "warning";
static constexpr const char* PROJMGR_DEBUG = "debug";
static constexpr const char* PROJMGR_INFO = "info";

bool ProjMgrLogger::m_quiet = false;
bool ProjMgrLogger::m_silent = false;

// singleton instance
static unique_ptr<ProjMgrLogger> theProjMgrLogger = 0;

  ProjMgrLogger::ProjMgrLogger() {
}

  ProjMgrLogger::~ProjMgrLogger() {
  Clear();
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
  m_ss.str(""); // Clear previous contents
  m_ss.clear();
}

void ProjMgrLogger::Error(const string& msg, const string& context,
  const string& file, const int line, const int column) {
  const string mark = (line > 0 ? ":" + to_string(line) : "") + (column > 0 ? ":" + to_string(column) : "");
  CollectionUtils::PushBackUniquely(m_errors[context],
    (file.empty() ? "" : RteUtils::ExtractFileName(file) + mark + " - ") + msg);
  if(!m_silent) {
    cerr << (file.empty() ? "" : file + mark + " - ") << PROJMGR_ERROR << PROJMGR_TOOL << msg << endl;
  }
}

void ProjMgrLogger::Warn(const string& msg, const string& context,
  const string& file, const int line, const int column) {
  const string mark = (line > 0 ? ":" + to_string(line) : "") + (column > 0 ? ":" + to_string(column) : "");
  CollectionUtils::PushBackUniquely(m_warns[context],
    (file.empty() ? "" : RteUtils::ExtractFileName(file) + mark + " - ") + msg);
  if (!IsQuiet()) {
    cerr << (file.empty() ? "" : file + mark + " - ") << PROJMGR_WARN << PROJMGR_TOOL << msg << endl;
  }
}

void ProjMgrLogger::Info(const string& msg, const string& context,
  const string& file, const int line, const int column) {
  const string mark = (line > 0 ? ":" + to_string(line) : "") + (column > 0 ? ":" + to_string(column) : "");
  CollectionUtils::PushBackUniquely(m_infos[context],
    (file.empty() ? "" : RteUtils::ExtractFileName(file) + mark + " - ") + msg);
  if (!IsQuiet()  ) {
    cout << (file.empty() ? "" : file + mark + " - ") << PROJMGR_INFO << PROJMGR_TOOL << msg << endl;
  }
}

void ProjMgrLogger::Debug(const string& msg) {
  if (!IsQuiet()) {
    cerr << PROJMGR_DEBUG << PROJMGR_TOOL << msg << endl;
  }
}

std::ostream& ProjMgrLogger::out() {
  if(m_silent) {
    return Get().m_ss;
  }
  return cout;
}

const std::vector<std::string>& ProjMgrLogger::GetErrorsForContext(const std::string& context) const
{
  return get_or_default_const_ref(m_errors, context, RteUtils::EMPTY_STRING_VECTOR);
}

const std::vector<std::string>& ProjMgrLogger::GetWarnsForContext(const std::string& context) const
{
  return get_or_default_const_ref(m_warns, context, RteUtils::EMPTY_STRING_VECTOR);
}

const std::vector<std::string>& ProjMgrLogger::GetInfosForContext(const std::string& context) const
{
  return get_or_default_const_ref(m_infos, context, RteUtils::EMPTY_STRING_VECTOR);
}


// end of ProjMgrLogger.cpp
