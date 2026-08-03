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

/** @brief Associates one discovered raw trace input with its channel name. */
struct TraceRunRawInput {
  std::filesystem::path path;
  std::string channel;
};

/** @brief Discovers trace-run configurations and their raw input files. */
class TraceRunDiscovery final {
public:
  /** @brief Selects one target configuration or every configuration in a trace directory. */
  static std::vector<std::filesystem::path> selectConfigFiles(const std::filesystem::path& traceDir,
                                                              const std::optional<std::string>& target);
  /** @brief Derives the solution-set name from a trace-run filename. */
  static std::string solutionSetName(const std::filesystem::path& configFile);
  /** @brief Discovers the supported raw inputs associated with a trace-run file. */
  static std::vector<TraceRunRawInput> rawInputs(const std::filesystem::path& configFile);

private:
  TraceRunDiscovery() = delete;
};

#endif  // CTRACE_SRC_TRACERUN_TRACERUNDISCOVERY_H
