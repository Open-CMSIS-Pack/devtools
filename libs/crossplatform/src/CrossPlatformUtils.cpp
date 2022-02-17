/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "CrossPlatformUtils.h"
#include "constants.h"

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

  if (defaultPackRoot.empty()) {
    defaultPackRoot = GetEnv(LOCAL_APP_DATA);
  }
  if (defaultPackRoot.empty()) {
    defaultPackRoot = GetEnv(USER_PROFILE);
  }
  if (!defaultPackRoot.empty()) {
    defaultPackRoot += PACK_ROOT_DIR;
  }

  return defaultPackRoot;
}

unsigned long CrossPlatformUtils::ClockInMsec() {
  static clock_t CLOCK_PER_MSEC = CLOCKS_PER_SEC / 1000;
  unsigned long t = (unsigned long)clock();
  if (CLOCK_PER_MSEC <= 1)
    return t;
  return (t / CLOCK_PER_MSEC);
}
