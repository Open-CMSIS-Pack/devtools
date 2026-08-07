/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_TRACEOUTPUTCONFIG_H
#define CTRACE_SRC_OUTPUT_TRACEOUTPUTCONFIG_H

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

/** @brief Stores normalized source metadata required by trace outputs. */
struct ResolvedTraceSource {
  std::string type;
  std::uint32_t source = 0;
  std::uint8_t traceBusId = 0U;
  std::optional<std::string> label;
  std::optional<std::uint64_t> symbolAddress;
  std::string valueType = "unsigned int";
  std::uint8_t valueSize = 4U;
};

/** @brief Configures one CSV output artifact. */
struct CsvOutputConfig {
  std::filesystem::path outputPath;
  TraceSelection selection;
};

/** @brief Configures one CTF bundle and its Trace Compass analysis file. */
struct CtfOutputConfig {
  /** @brief Creates a complete CTF output configuration. */
  CtfOutputConfig(std::filesystem::path outputDirectory, std::filesystem::path traceCompassXmlPath,
                  std::uint64_t clockHz, TraceSelection selection, std::vector<ResolvedTraceSource> sources)
    : outputDirectory(std::move(outputDirectory)),
      traceCompassXmlPath(std::move(traceCompassXmlPath)),
      coreClockHz(clockHz),
      selection(std::move(selection)),
      sources(std::move(sources))
  {
  }

  std::filesystem::path outputDirectory;
  std::filesystem::path traceCompassXmlPath;
  std::uint64_t coreClockHz = 0;
  TraceSelection selection;
  std::vector<ResolvedTraceSource> sources;
};

#endif  // CTRACE_SRC_OUTPUT_TRACEOUTPUTCONFIG_H
