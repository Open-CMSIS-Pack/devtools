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

class TraceDirectoryJob {
public:
  TraceDirectoryJob(CliOptions options, DiagnosticSink& diagnostics, const TraceRunConfigReader& configReader);

  void run();

private:
  CliOptions options_;
  DiagnosticSink& diagnostics_;
  const TraceRunConfigReader& configReader_;
};

#endif  // CTRACE_SRC_CONTROL_TRACEDIRECTORYJOB_H
