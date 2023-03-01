/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SVDConv.h"
#include "SfrccInterface.h"
#include "ErrLog.h"
#include "SvdUtils.h"
#include "RteFsUtils.h"

#ifdef _WIN32
#include <shlwapi.h>
#endif

#include <string>
#include <set>
#include <list>
#include <map>
using namespace std;


SfrccInterface::SfrccInterface()
{
  char modulePath[1028] = { 0 };

#ifdef _WIN32
  GetModuleFileName(NULL, modulePath, 1024);
#endif

  string modPath = modulePath;
  string::size_type pos = modPath.find_last_of('\\');  // do not change this to slash!
  if(pos != string::npos && pos > 1) {
    modPath.erase(pos, modPath.length());
    SetModulePath(modPath);
  }
}

SfrccInterface::~SfrccInterface()
{
}

string SfrccInterface::GetPathToSfrcc()
{
  const string& modulePath = GetModulePath();
  if(modulePath.empty()) {
    return "SfrCC2.exe";
  }

  string pathToSfrcc;
  pathToSfrcc += modulePath;
  pathToSfrcc += '/';
  pathToSfrcc += "SfrCC2.exe";

  if(!RteFsUtils::Exists(pathToSfrcc)) {
    LogMsg("M109", NAME(pathToSfrcc));
    pathToSfrcc.clear();
  }

  return pathToSfrcc;
}

bool SfrccInterface::Compile(const string& fileName, bool bCleanup /*= true*/)
{
#ifdef _WIN32
  string pathToSfrCC = GetPathToSfrcc();
  if(pathToSfrCC.empty()) {
    return false;
  }

  string cmd;
  cmd += "\"";

  cmd += "\"";
  cmd += pathToSfrCC;
  cmd += "\"";

  cmd += " \"";
  cmd += fileName;
  cmd += "\"";

  if(!bCleanup) {
    cmd += " NOCLEANUP";
  }

  cmd += "\"";

  FILE* pPipe = _popen(cmd.c_str(), "rt");
  if(!pPipe) {
    LogMsg("M124", PATH(cmd));
    return false;
  }

  char *logBuf = new char[2048];
  if(!logBuf) {
    return false;
  }

  string sfrccMsg;
  while(fgets(logBuf, 2048, pPipe)) {
    sfrccMsg += logBuf;
  }

  int32_t ret = _pclose(pPipe);
  delete[] logBuf;

  if(!sfrccMsg.empty()) {
    LogMsg("M125", MSG(sfrccMsg));      // verbose message
  }

  if(ret >= 2) {
    LogMsg("M127");   // Errors
  }
  else if(ret >= 1) {
    LogMsg("M128");   // Warnings
  }
#endif
  return true;
}
