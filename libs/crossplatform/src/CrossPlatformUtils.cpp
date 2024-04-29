/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "CrossPlatformUtils.h"
#include "CrossPlatform.h"
#include "constants.h"

#include <time.h>
#include <array>
#include <functional>

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
    defaultPackRoot += (defaultPackRoot.empty() ? "" : CACHE_DIR);
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

const std::string& CrossPlatformUtils::GetHostType()
{
  // OS name is defined at compile time
  static const std::string os_name = HOST_TYPE;
  return os_name;
}


const std::pair<std::string, int> CrossPlatformUtils::ExecCommand(const std::string& cmd)
{
  std::array<char, 128> buffer;
  std::string result;
  int ret_code = -1;
  std::function<int(FILE*)> close = _pclose;
  std::function<FILE* (const char*, const char*)> open = _popen;

  auto deleter = [&close, &ret_code](FILE* cmd) { ret_code = close(cmd); };
  {
    const std::unique_ptr<FILE, decltype(deleter)> pipe(open(PopenCmd(cmd).c_str(), "r"), deleter);
    if (pipe) {
      while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
      }
    }
  }
  return std::make_pair(result, ret_code);
}

// end of CrossplatformUtils.cpp
