/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "OutputPath.h"

#include <stdexcept>
#include <string>
#include <system_error>

void OutputPath::requireSpecificTarget(const std::filesystem::path& path, const char* description)
{
  const auto normalized = path.lexically_normal();
  if (path.empty() || normalized == normalized.root_path() || normalized.filename().empty() ||
      normalized.filename() == "." || normalized.filename() == "..") {
    throw std::invalid_argument(std::string(description) + " must identify a specific output path");
  }
}

void OutputPath::validateParent(const std::filesystem::path& path, const char* description)
{
  std::error_code error;
  const auto absolute = std::filesystem::absolute(path, error);
  auto parent = (error ? path : absolute).lexically_normal().parent_path();
  while (!parent.empty()) {
    const auto status = std::filesystem::status(parent);
    if (std::filesystem::exists(status)) {
      if (!std::filesystem::is_directory(status)) {
        throw std::runtime_error(std::string(description) + " parent is not a directory: " + parent.string());
      }
      return;
    }
    parent = parent.parent_path();
  }
}
