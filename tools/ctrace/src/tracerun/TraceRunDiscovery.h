/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_TRACERUN_TRACERUNDISCOVERY_H
#define CTRACE_SRC_TRACERUN_TRACERUNDISCOVERY_H

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct TraceRunRawInput {
  std::filesystem::path path;
  std::string channel;
};

class TraceRunDiscovery final {
public:
  static std::vector<std::filesystem::path> selectConfigFiles(const std::filesystem::path& traceDir,
                                                              const std::optional<std::string>& target);
  static std::string solutionSetName(const std::filesystem::path& configFile);
  static std::vector<TraceRunRawInput> rawInputs(const std::filesystem::path& configFile);

private:
  TraceRunDiscovery() = delete;
};

#endif  // CTRACE_SRC_TRACERUN_TRACERUNDISCOVERY_H
