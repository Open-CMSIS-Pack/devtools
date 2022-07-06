/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "CrossPlatformUtils.h"

#include <memory>
#include <limits.h>
#include <mach-o/dyld.h>
#include <unistd.h>

 // mac specific methods

bool CrossPlatformUtils::SetEnv(const std::string& name, const std::string& value)
{
  if (name.empty()) {
    return false;
  }
  return setenv(name.c_str(), value.c_str(), 1) == 0;
}

std::string CrossPlatformUtils::GetExecutablePath(std::error_code& ec) {
  std::string path;
  std::unique_ptr<char[]> exePath{};
  std::unique_ptr<char[]> rawPathName{};

  try {
    exePath = std::make_unique<char[]>(PATH_MAX);
    rawPathName = std::make_unique<char[]>(PATH_MAX);
  }
  catch (std::bad_alloc&) {
    ec.assign(ENOMEM, std::generic_category());
    return "";
  }

  uint32_t rawPathSize = (uint32_t)PATH_MAX;
  auto retVal = _NSGetExecutablePath(rawPathName.get(), &rawPathSize);
  if (0 == retVal) {
    char* pValue = realpath(rawPathName.get(), exePath.get());
    if (nullptr == pValue) {
      ec.assign(errno, std::generic_category());
      return "";
    }
    path = std::string(exePath.get());
  }

  if (-1 == retVal) {
    ec.assign(EOVERFLOW, std::generic_category());
  }

  return path;
}

std::string CrossPlatformUtils::GetRegistryString(const std::string& key)
{
  return GetEnv(key); // non-Windows implementation returns environment variable value
}

// end of Utils.cpp
