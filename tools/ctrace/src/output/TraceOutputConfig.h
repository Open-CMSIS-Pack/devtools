/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_TRACEOUTPUTCONFIG_H
#define CTRACE_SRC_OUTPUT_TRACEOUTPUTCONFIG_H

#include "ctf/CtfMetadataModel.h"
#include "TraceRoute.h"
#include "TraceSelection.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/** @brief Stores the output formats and filters requested by the user. */
struct TraceOutputRequest {
  bool csv = false;
  bool ctf = false;
  TraceSelection selection;
};

/** @brief Configures one CSV output artifact. */
struct CsvOutputConfig {
  std::filesystem::path outputPath;
  TraceSelection selection;
};

/** @brief Configures one independently decoded CTF bundle. */
struct CtfOutputConfig {
  /** @brief Creates a complete CTF output configuration. */
  CtfOutputConfig(std::filesystem::path outputDirectory, TraceSelection selection, CtfMetadataTopology metadata,
                  std::vector<TraceRouteIdentity> routes = {})
    : outputDirectory(std::move(outputDirectory)),
      selection(std::move(selection)),
      metadata(std::move(metadata)),
      routes(std::move(routes))
  {
  }

  std::filesystem::path outputDirectory;
  TraceSelection selection;
  CtfMetadataTopology metadata;
  std::vector<TraceRouteIdentity> routes;
};

#endif  // CTRACE_SRC_OUTPUT_TRACEOUTPUTCONFIG_H
