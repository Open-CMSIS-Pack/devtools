/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_TEST_UNIT_SUPPORT_TESTPATH_H
#define CTRACE_TEST_UNIT_SUPPORT_TESTPATH_H

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

inline std::filesystem::path testTemporaryPath(const std::string& name)
{
#if defined(_WIN32)
  const auto processId = _getpid();
#else
  const auto processId = getpid();
#endif
  return std::filesystem::temp_directory_path() / (name + '-' + std::to_string(processId));
}

/** @brief Owns and removes one process-specific temporary test path. */
class TemporaryTestPath {
public:
  /** @brief Prepares an empty path with the supplied test name. */
  explicit TemporaryTestPath(const std::string& name) : path_(testTemporaryPath(name))
  {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
    if (error) {
      throw std::runtime_error("failed to prepare temporary test path: " + path_.string());
    }
  }

  /** @brief Removes the temporary path and its contents. */
  ~TemporaryTestPath()
  {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  /** @brief Disables copying to preserve unique path ownership. */
  TemporaryTestPath(const TemporaryTestPath&) = delete;
  /** @brief Disables copy assignment to preserve unique path ownership. */
  TemporaryTestPath& operator=(const TemporaryTestPath&) = delete;

  /** @brief Returns the owned temporary path. */
  const std::filesystem::path& path() const
  {
    return path_;
  }

  /** @brief Creates and returns the owned path as a directory. */
  const std::filesystem::path& createDirectory() const
  {
    std::filesystem::create_directories(path_);
    return path_;
  }

private:
  std::filesystem::path path_;
};

#endif  // CTRACE_TEST_UNIT_SUPPORT_TESTPATH_H
