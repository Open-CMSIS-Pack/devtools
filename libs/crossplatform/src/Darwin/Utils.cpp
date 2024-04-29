/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "CrossPlatformUtils.h"

#include <memory>
#include <sys/stat.h>
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

bool CrossPlatformUtils::CanExecute(const std::string& file)
{
  return access(file.c_str(), X_OK) == 0;
}

CrossPlatformUtils::REG_STATUS CrossPlatformUtils::GetLongPathRegStatus() {
  return REG_STATUS::NOT_SUPPORTED;
}

std::filesystem::perms CrossPlatformUtils::GetCurrentUmask() {
  // Only way to get current umask is to change it, so revert the possible change directly.
  mode_t value = ::umask(0);
  ::umask(value);

  std::filesystem::perms perm = std::filesystem::perms::none;

  // Map the mode_t value to coresponding fs::perms value
  constexpr struct {
    mode_t m;
    std::filesystem::perms p;
  } mode_to_perm_map[] = {
    {S_IRUSR, std::filesystem::perms::owner_read},
    {S_IWUSR, std::filesystem::perms::owner_write},
    {S_IXUSR, std::filesystem::perms::owner_exec},
    {S_IRGRP, std::filesystem::perms::group_read},
    {S_IWGRP, std::filesystem::perms::group_write},
    {S_IXGRP, std::filesystem::perms::group_exec},
    {S_IROTH, std::filesystem::perms::others_read},
    {S_IWOTH, std::filesystem::perms::others_write},
    {S_IXOTH, std::filesystem::perms::others_exec}
  };
  for (auto [m, p] : mode_to_perm_map) {
    if (value & m) {
      perm |= p;
    }
  }

  return perm;
}

const std::string CrossPlatformUtils::PopenCmd(const std::string& cmd) {
  return cmd;
}

// end of Utils.cpp
