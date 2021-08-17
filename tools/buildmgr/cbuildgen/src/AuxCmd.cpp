/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 // AuxCmd.cpp

#include "AuxCmd.h"
#include "CbuildUtils.h"

#include "ErrLog.h"
#include "RteFsUtils.h"

#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

AuxCmd::AuxCmd(void) {
  // Reserved
}

AuxCmd::~AuxCmd(void) {
  // Reserved
}

bool AuxCmd::RunAuxCmd(int cmd, const list<string>& params, const string& except) {
  if (params.empty()) {
    LogMsg("M200");
    return false;
  }
  switch (cmd) {
    case AUX_MKDIR:
      return MkdirCmd(params);
      break;
    case AUX_RMDIR:
      return RmdirCmd(params, except);
      break;
    case AUX_TOUCH:
      return TouchCmd(params);
      break;
  }
  return false;
}

bool AuxCmd::MkdirCmd(const list<string>& params) {
  /*
  mkdirCmd:
  Make directories
  */

  error_code ec;
  for (string dir : params) {
    fs::create_directories(dir, ec);
    if (ec) {
      LogMsg("M211", PATH(dir));
      return false;
    }
  }
  return true;
}

bool AuxCmd::RmdirCmd(const list<string>& params, const string& except) {
  /*
  rmdirCmd:
  Remove files and directories recursively with 'except' option
  */

  error_code ec;
  for (const string& param : params) {
    if (!fs::is_directory(param, ec)) {
      LogMsg("M200");
      return false;
    }
  }
  fs::path pathEx = RteFsUtils::AbsolutePath(except);
  for (const string& param : params) {
    fs::path path = RteFsUtils::AbsolutePath(param);
    if (!fs::exists(path, ec)) {
      continue;
    }

    // Remove files
    for (auto& p : fs::recursive_directory_iterator(path, ec)) {
      if (fs::is_regular_file(p, ec)) {
        if (p.path().compare(pathEx) == 0) {
          continue;
        }
        fs::remove(p, ec);
        if (ec) {
          LogMsg("M212", PATH(p.path().generic_string()));
          return false;
        }
      }
    }
    // Remove child directories
    for (auto& p : fs::directory_iterator(path)) {
      if (pathEx.string().find(p.path().string()) != string::npos) {
        continue;
      }
      fs::remove_all(p, ec);
      if (ec) {
        LogMsg("M212", PATH(p.path().generic_string()));
        return false;
      }
    }
    // Remove base path if empty
    fs::remove(path, ec);
  }
  return true;
}

bool AuxCmd::TouchCmd(const list<string>& params) {
  /*
  touchCmd:
  Update file timestamp
  */

  error_code ec;
  for (string file : params) {
    if (fs::exists(file, ec)) {
      // Update timestamp
      const auto now = fs::file_time_type::clock::now();
      fs::last_write_time(file, now, ec);
    } else {
      // If file does not exist, create it
      ofstream fileStream(file);
      if (!fileStream) {
        LogMsg("M210", PATH(file));
        return false;
      }
      fileStream.close();
    }
  }
  return true;
}
