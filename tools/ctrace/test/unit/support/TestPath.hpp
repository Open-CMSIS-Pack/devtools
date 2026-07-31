/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

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

class TemporaryTestPath {
public:
  explicit TemporaryTestPath(const std::string& name) : path_(testTemporaryPath(name))
  {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
    if (error) {
      throw std::runtime_error("failed to prepare temporary test path: " + path_.string());
    }
  }

  ~TemporaryTestPath()
  {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }

  TemporaryTestPath(const TemporaryTestPath&) = delete;
  TemporaryTestPath& operator=(const TemporaryTestPath&) = delete;

  const std::filesystem::path& path() const
  {
    return path_;
  }

  const std::filesystem::path& createDirectory() const
  {
    std::filesystem::create_directories(path_);
    return path_;
  }

private:
  std::filesystem::path path_;
};
