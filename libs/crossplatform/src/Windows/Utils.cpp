/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "CrossPlatformUtils.h"

#include <cassert>
#include <limits.h>
#include <windows.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>

using namespace std;
// windows-specific methods

bool CrossPlatformUtils::SetEnv(const std::string& name, const std::string& value)
{
  if (name.empty()) {
    return false;
  }
  return _putenv_s(name.c_str(), value.c_str()) == 0;
}

std::string CrossPlatformUtils::GetExecutablePath(std::error_code& ec) {
  char exePath[MAX_PATH] = { 0 };

  auto bytesRead = GetModuleFileNameA(NULL, exePath, MAX_PATH);
  if ((0 == bytesRead) || (bytesRead >= MAX_PATH)) {
    ec.assign(GetLastError(), std::generic_category());
  }
  return std::string(exePath, bytesRead >= MAX_PATH ? MAX_PATH : bytesRead);
}

static std::string GetRegValue(HKEY regKey, const std::string& keyName)
{
  std::string value;
  if (keyName.empty()) {
    return value;
  }
  std::string valueName;
  HKEY      hRegKey = NULL;
  if (RegOpenKeyEx(regKey, keyName.c_str(), 0, KEY_QUERY_VALUE, &hRegKey) != ERROR_SUCCESS) {
    // try to split the key
    string::size_type pos = keyName.find_last_of("\\");
    if (pos == string::npos) {
      return value;
    }
    valueName = keyName.substr(pos + 1);

    if (RegOpenKeyEx(regKey, keyName.substr(0, pos).c_str(), 0, KEY_QUERY_VALUE, &hRegKey) != ERROR_SUCCESS) {
      return value;
    }
  }
  DWORD dwLength = MAX_PATH;
  DWORD dwType;
  char       szBuf[MAX_PATH];
  szBuf[0] = 0;                          // invalidate
  if (RegQueryValueEx(hRegKey, valueName.c_str(), NULL, &dwType, (BYTE*)szBuf, &dwLength) == ERROR_SUCCESS) {
    // expand environment variables if needed
    char szExpanded[0x8000]; // 32 K buffer
    DWORD res = ExpandEnvironmentStrings(szBuf, szExpanded, 0x8000);
    if(res > 0 && res < 0x8000) {
      value = szExpanded;
    } else {
      value = szBuf;
    }
  }
  RegCloseKey(hRegKey);
  return value;
}

std::string CrossPlatformUtils::GetRegistryString(const std::string& key)
{
  string name = key;
  string::size_type pos = 0;
  for (pos = name.find('/'); pos != string::npos; pos = name.find('/', pos)) {
    name[pos] = '\\';
  }

  // check if a specific key is requested, otherwise try both
  bool bCurUser = true; // by default try first current user
  bool bLocalMachine = true;
  bool bEnvVar = true; // last resort
  pos = name.find("HKEY_CURRENT_USER\\");
  if (pos == 0) {
    bLocalMachine = false; // only current user is requested
    bEnvVar = false;
    name = name.substr(strlen("HKEY_CURRENT_USER\\"));
  }
  pos = key.find("HKEY_LOCAL_MACHINE\\");
  if (pos == 0) {
    bCurUser = false; // only local machine is requested
    bEnvVar = false;
    name = name.substr(strlen("HKEY_LOCAL_MACHINE\\"));
  }
  std::string value;
  if (bCurUser) {
    value = GetRegValue(HKEY_CURRENT_USER, name);
  }
  if (bLocalMachine && value.empty()) {
    value = GetRegValue(HKEY_LOCAL_MACHINE, name);
  }
  if (bEnvVar && value.empty()) {
    value = GetEnv(key);
  }

  return value;
}

bool CrossPlatformUtils::CanExecute(const std::string& file)
{
  // on Windows we only check file extension: exe, com or bat;
  std::string::size_type pos = file.find_last_of('.');
  if (pos == string::npos) {
    return false;
  }
  std::string ext = file.substr(pos + 1);
  return
    _stricmp(ext.c_str(), "exe") == 0 ||
    _stricmp(ext.c_str(), "com") == 0 ||
    _stricmp(ext.c_str(), "bat") == 0;
}

CrossPlatformUtils::REG_STATUS CrossPlatformUtils::GetLongPathRegStatus()
{
  HKEY hRegKey = NULL;
  DWORD dwValue, dwLength = sizeof(DWORD), dwType = REG_DWORD;

  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\FileSystem", 0, KEY_READ, &hRegKey) == ERROR_SUCCESS) {
    if (RegQueryValueEx(hRegKey, "LongPathsEnabled", NULL, &dwType, (LPBYTE)&dwValue, &dwLength) == ERROR_SUCCESS) {
      RegCloseKey(hRegKey);
      return (dwValue == 0) ? REG_STATUS::DISABLED : REG_STATUS::ENABLED;
    }
  }
  return REG_STATUS::DISABLED;
}

std::filesystem::perms CrossPlatformUtils::GetCurrentUmask() {
  // Only way to get current umask is to change it, so revert the possible change directly.
  unsigned int value = ::_umask(0);
  ::_umask(value);

  std::filesystem::perms perm = std::filesystem::perms::none;

  // Map the mode_t value to coresponding fs::perms value
  constexpr struct {
    unsigned int m;
    std::filesystem::perms p;
  } mode_to_perm_map[] = {
    {_S_IREAD, std::filesystem::perms::owner_read},
    {_S_IWRITE, std::filesystem::perms::owner_write},
    {_S_IEXEC, std::filesystem::perms::owner_exec}
  };
  for (auto [m, p] : mode_to_perm_map) {
    if (value & m) {
      perm |= p;
    }
  }

  return perm;
}

// surround the whole 'cmd' with quotes since they get stripped in _popen,
// avoiding breaking the command when there are multiple quotes pairs inside it
const std::string CrossPlatformUtils::PopenCmd(const std::string& cmd) {
  return "\"" + cmd + "\"";
}

// end of Utils.cpp
