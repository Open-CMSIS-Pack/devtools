/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CrossPlatformUtils.h"
#include "constants.h"

#include <sys/stat.h>
#include <limits.h>
#include <memory>
#include <unistd.h>

// linux-specific methods

bool CrossPlatformUtils::SetEnv(const std::string& name, const std::string& value)
{
  if (name.empty()) {
    return false;
  }
  return setenv(name.c_str(), value.c_str(), 1) == 0;
}

std::string CrossPlatformUtils::GetExecutablePath(std::error_code& ec) {
  struct stat sb;
  ssize_t bytesRead, bufSize;

  // This might fail in case of "/proc/self/exe" not available
  if (lstat(PROC_SELF_EXE, &sb) == -1) {
    ec.assign(errno, std::generic_category());
    return "";
  }
  bufSize = sb.st_size;

  if (0 == sb.st_size) {
    bufSize = PATH_MAX;
  }

  std::unique_ptr<char[]> exePath{};
  try {
    exePath = std::make_unique<char[]>(bufSize);
  }
  catch(std::bad_alloc&) {
    ec.assign(ENOMEM, std::generic_category());
    return "";
  }

  bytesRead = readlink(PROC_SELF_EXE, exePath.get(), bufSize);
  if (bytesRead < 0) {
    ec.assign(errno, std::generic_category());
    return "";
  }

  if (bytesRead >= bufSize) {
    ec.assign(EOVERFLOW, std::generic_category());
    bytesRead = bufSize;
  }

  return std::string(exePath.get(), bytesRead);
}

std::string CrossPlatformUtils::GetRegistryString(const std::string& key)
{
  return GetEnv(key); // non-Windows implementation returns environment variable value
}

bool CrossPlatformUtils::CanExecute(const std::string& file)
{
  return access(file.c_str(), X_OK) == 0;
}

// end of Utils.cpp
