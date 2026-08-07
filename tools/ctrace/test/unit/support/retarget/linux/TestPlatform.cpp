/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestPlatform.h"

#include <filesystem>
#include <string>
#include <string_view>

bool TestPlatform::supports(TestPlatformCapability capability) noexcept
{
  switch (capability) {
  case TestPlatformCapability::DirectoryReadFailure:
  case TestPlatformCapability::LinuxSpecialFiles:
  case TestPlatformCapability::PosixPermissions:
    return true;
  }
  return false;
}

std::filesystem::path TestPlatform::writeFailurePath()
{
  return "/dev/full";
}

std::filesystem::path TestPlatform::creationFailurePath(std::string_view fileName)
{
  return std::filesystem::path("/proc") / std::string(fileName);
}
