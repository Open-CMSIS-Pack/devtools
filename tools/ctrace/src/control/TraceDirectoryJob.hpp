/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "CliOptions.hpp"
#include "DiagnosticSink.hpp"
#include "TraceRunConfigReader.hpp"

class TraceDirectoryJob {
public:
  TraceDirectoryJob(CliOptions options, DiagnosticSink& diagnostics, const TraceRunConfigReader& configReader);

  void run();

private:
  CliOptions options_;
  DiagnosticSink& diagnostics_;
  const TraceRunConfigReader& configReader_;
};
