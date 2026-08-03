/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_OUTPUTREQUIREMENTS_H
#define CTRACE_SRC_OUTPUT_OUTPUTREQUIREMENTS_H

#include "TraceOutputConfig.h"

#include <filesystem>
#include <optional>

class CtraceRunMeta;
class DiagnosticSink;

struct TraceOutputPlan {
  bool csvRequested = false;
  bool ctfRequested = false;
  std::optional<CsvOutputConfig> csv;
  std::optional<CtfOutputConfig> ctf;

  bool hasRequestedOutputs() const;
  bool hasEnabledOutputs() const;
};

TraceOutputPlan planTraceOutputs(const TraceOutputRequest& request, const std::filesystem::path& rawInputPath,
                                 const CtraceRunMeta& ctraceRunMeta, DiagnosticSink& diagnostics);

#endif  // CTRACE_SRC_OUTPUT_OUTPUTREQUIREMENTS_H
