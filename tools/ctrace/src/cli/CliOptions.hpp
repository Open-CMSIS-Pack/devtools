/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "TraceSelection.hpp"

#include <optional>
#include <string>

enum class OutputFormat {
  None,
  Ctf,
  Csv,
  All,
};

struct CliOptions {
  std::optional<std::string> traceDir;
  std::optional<std::string> targetName;
  OutputFormat outputFormat = OutputFormat::None;
  TraceSelection selection;
  bool help = false;
  bool version = false;
};
