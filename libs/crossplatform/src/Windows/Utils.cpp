/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "CrossPlatformUtils.h"

#include <cassert>
#include <limits.h>
#include <Windows.h>

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
