/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct TraceRunRawInput {
  std::filesystem::path path;
  std::string channel;
};

namespace TraceRunDiscovery {

std::vector<std::filesystem::path> selectConfigFiles(const std::filesystem::path& traceDir,
                                                     const std::optional<std::string>& target);
std::string solutionSetName(const std::filesystem::path& configFile);
std::vector<TraceRunRawInput> rawInputs(const std::filesystem::path& configFile);

} // namespace TraceRunDiscovery
