/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "CrossPlatformUtils.h"

#include <time.h>

std::string CrossPlatformUtils::GetEnv(const std::string& name)
{
  std::string val;
  if (name.empty()) {
    return val;
  }

  char* pValue = getenv(name.c_str());
  if (pValue && *pValue) {
    val = pValue;
  }
  return val;
}

bool CrossPlatformUtils::SetEnv(const std::string& name, const std::string& value)
{
  if (name.empty()) {
    return false;
  }
#ifdef _WIN32
  return _putenv_s(name.c_str(), value.c_str()) == 0;
#else
  return setenv(name.c_str(), value.c_str(), 1) == 0;
#endif
}


std::string CrossPlatformUtils::GetCMSISPackRootDir()
{
  std::string pack_root = CrossPlatformUtils::GetEnv("CMSIS_PACK_ROOT");
  if (!pack_root.empty()) {
    return pack_root;
  }
  return GetDefaultCMSISPackRootDir();
}

std::string CrossPlatformUtils::GetDefaultCMSISPackRootDir()
{
  std::string defaultPackRoot = GetEnv(DEFAULT_PACKROOTDEF); // defined via CMakeLists.txt

#ifdef _WIN32
  if (defaultPackRoot.empty()) {
    defaultPackRoot = GetEnv("LOCALAPPDATA");
  }
  if (defaultPackRoot.empty()) {
    defaultPackRoot = GetEnv("USERPROFILE");
  }
  if (!defaultPackRoot.empty()) {
    defaultPackRoot += "\\Arm\\Packs";
  }
#else
  if (defaultPackRoot.empty()) {
    defaultPackRoot = GetEnv("XDG_CACHE_HOME");
  }
  if (defaultPackRoot.empty()) {
    defaultPackRoot = GetEnv("HOME");
  }
  if (!defaultPackRoot.empty()) {
    defaultPackRoot += "/arm/packs";
  }
#endif
  return defaultPackRoot;
}

unsigned long CrossPlatformUtils::ClockInMsec() {
  static clock_t CLOCK_PER_MSEC = CLOCKS_PER_SEC / 1000;
  unsigned long t = (unsigned long)clock();
  if (CLOCK_PER_MSEC <= 1)
    return t;
  return (t / CLOCK_PER_MSEC);
}

// end of CrossPlatformUtils.cpp
