/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_TEST_UNIT_SUPPORT_TESTPATH_H
#define CTRACE_TEST_UNIT_SUPPORT_TESTPATH_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

/** @brief Creates a process-specific path in the system temporary directory. */
inline std::filesystem::path testTemporaryPath(const std::string& name)
{
  static std::atomic<std::uint64_t> sequence{0U};
  const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto uniqueId = sequence.fetch_add(1U, std::memory_order_relaxed);
  return std::filesystem::temp_directory_path() /
         (name + '-' + std::to_string(timestamp) + '-' + std::to_string(uniqueId));
}

/** @brief Owns and removes one process-specific temporary test path. */
class TemporaryTestPath {
public:
  /** @brief Prepares an empty path with the supplied test name. */
  explicit TemporaryTestPath(const std::string& name)
    : m_path(testTemporaryPath(name))
  {
    std::error_code error;
    std::filesystem::remove_all(m_path, error);
    if (error) {
      throw std::runtime_error("failed to prepare temporary test path: " + m_path.string());
    }
  }

  /** @brief Removes the temporary path and its contents. */
  ~TemporaryTestPath()
  {
    std::error_code ignored;
    std::filesystem::remove_all(m_path, ignored);
  }

  /** @brief Disables copying to preserve unique path ownership. */
  TemporaryTestPath(const TemporaryTestPath&) = delete;
  /** @brief Disables copy assignment to preserve unique path ownership. */
  TemporaryTestPath& operator=(const TemporaryTestPath&) = delete;

  /** @brief Returns the owned temporary path. */
  const std::filesystem::path& path() const
  {
    return m_path;
  }

  /** @brief Creates and returns the owned path as a directory. */
  const std::filesystem::path& createDirectory() const
  {
    std::filesystem::create_directories(m_path);
    return m_path;
  }

private:
  std::filesystem::path m_path;
};

#endif // CTRACE_TEST_UNIT_SUPPORT_TESTPATH_H
