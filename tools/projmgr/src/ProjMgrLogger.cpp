/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrLogger.h"

#include <iostream>

using namespace std;

static constexpr const char* PROJMGR_TOOL = " csolution: ";
static constexpr const char* PROJMGR_ERROR = "error";
static constexpr const char* PROJMGR_WARN = "warning";
static constexpr const char* PROJMGR_INFO = "info";

ProjMgrLogger::ProjMgrLogger(void) {
  // Reserved
}

ProjMgrLogger::~ProjMgrLogger(void) {
  // Reserved
}

void ProjMgrLogger::Error(const string& file, const int line, const int column, const string& msg) {
  cerr << file << ":" << line << ":" << column << " - " << PROJMGR_ERROR << PROJMGR_TOOL << msg << endl;
}

void ProjMgrLogger::Error(const string& file, const string& msg) {
  cerr << file << " - " << PROJMGR_ERROR << PROJMGR_TOOL << msg << endl;
}

void ProjMgrLogger::Error(const string& msg) {
  cerr << PROJMGR_ERROR << PROJMGR_TOOL << msg << endl;
}

void ProjMgrLogger::Warn(const string& file, const int line, const int column, const string& msg) {
  cerr << file << ":" << line << ":" << column << " - " << PROJMGR_WARN << PROJMGR_TOOL << msg << endl;
}

void ProjMgrLogger::Warn(const string& file, const string& msg) {
  cerr << file << " - " << PROJMGR_WARN << PROJMGR_TOOL << msg << endl;
}

void ProjMgrLogger::Warn(const string& msg) {
  cerr << PROJMGR_WARN << PROJMGR_TOOL << msg << endl;
}

void ProjMgrLogger::Info(const string& file, const int line, const int column, const string& msg) {
  cout << file << ":" << line << ":" << column << PROJMGR_TOOL << PROJMGR_INFO << msg << endl;
}

void ProjMgrLogger::Info(const string& file, const string& msg) {
  cout << file << " - " << PROJMGR_INFO << PROJMGR_TOOL << msg << endl;
}

void ProjMgrLogger::Info(const string& msg) {
  cout << PROJMGR_INFO << PROJMGR_TOOL << msg << endl;
}
