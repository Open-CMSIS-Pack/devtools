/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "TraceSelection.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct TraceOutputRequest {
  bool csv = false;
  bool ctf = false;
  TraceSelection selection;
};

struct ResolvedTraceSource {
  std::string type;
  std::uint32_t source = 0;
  std::uint8_t traceBusId = 0U;
  std::optional<std::string> label;
  std::optional<std::uint64_t> symbolAddress;
  std::string valueType = "unsigned int";
  std::uint8_t valueSize = 4U;
};

struct CsvOutputConfig {
  std::filesystem::path outputPath;
  TraceSelection selection;
};

struct CtfOutputConfig {
  CtfOutputConfig(std::filesystem::path outputDirectory, std::filesystem::path traceCompassXmlPath,
                  std::uint64_t clockHz, TraceSelection selection, std::vector<ResolvedTraceSource> sources)
    : outputDirectory(std::move(outputDirectory)), traceCompassXmlPath(std::move(traceCompassXmlPath)),
      coreClockHz(clockHz), selection(std::move(selection)), sources(std::move(sources))
  {
  }

  std::filesystem::path outputDirectory;
  std::filesystem::path traceCompassXmlPath;
  std::uint64_t coreClockHz = 0;
  TraceSelection selection;
  std::vector<ResolvedTraceSource> sources;
};
