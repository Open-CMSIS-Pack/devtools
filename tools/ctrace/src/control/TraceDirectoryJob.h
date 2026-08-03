/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_CONTROL_TRACEDIRECTORYJOB_H
#define CTRACE_SRC_CONTROL_TRACEDIRECTORYJOB_H

#include "CliOptions.h"
#include "DiagnosticSink.h"
#include "TraceRunConfigReader.h"

/** @brief Discovers and decodes the selected trace-run configurations in a directory. */
class TraceDirectoryJob {
public:
  /** @brief Creates a directory job with parsed options and a trace-run reader. */
  TraceDirectoryJob(CliOptions options, DiagnosticSink& diagnostics, const TraceRunConfigReader& configReader);

  /** @brief Runs discovery and all selected file decode jobs. */
  void run();

private:
  CliOptions options_;
  DiagnosticSink& diagnostics_;
  const TraceRunConfigReader& configReader_;
};

#endif  // CTRACE_SRC_CONTROL_TRACEDIRECTORYJOB_H
