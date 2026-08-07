/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestPlatform.h"

#include <filesystem>
#include <string_view>

bool TestPlatform::supports(TestPlatformCapability) noexcept
{
  return false;
}

std::filesystem::path TestPlatform::writeFailurePath()
{
  return {};
}

std::filesystem::path TestPlatform::creationFailurePath(std::string_view)
{
  return {};
}
