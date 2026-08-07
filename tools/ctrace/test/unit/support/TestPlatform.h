/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_TEST_UNIT_SUPPORT_TESTPLATFORM_H
#define CTRACE_TEST_UNIT_SUPPORT_TESTPLATFORM_H

#include <filesystem>
#include <string_view>

/** @brief Identifies target-platform facilities used by portable unit tests. */
enum class TestPlatformCapability {
  DirectoryReadFailure,
  LinuxSpecialFiles,
  PosixPermissions,
};

/** @brief Provides target-platform test services without exposing platform macros to tests. */
class TestPlatform final {
public:
  /** @brief Reports whether the target supports the requested test facility. */
  static bool supports(TestPlatformCapability capability) noexcept;

  /** @brief Returns a target path whose writes fail after opening. */
  static std::filesystem::path writeFailurePath();

  /** @brief Returns a target path that rejects creation of the named file. */
  static std::filesystem::path creationFailurePath(std::string_view fileName);

private:
  /** @brief Prevents construction of this stateless test utility. */
  TestPlatform() = delete;
};

#endif // CTRACE_TEST_UNIT_SUPPORT_TESTPLATFORM_H
