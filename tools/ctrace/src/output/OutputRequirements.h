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

/** @brief Stores independently validated CSV and CTF output configurations. */
struct TraceOutputPlan {
  bool csvRequested = false;
  bool ctfRequested = false;
  std::optional<CsvOutputConfig> csv;
  std::optional<CtfOutputConfig> ctf;

  /** @brief Reports whether any output format was requested. */
  bool hasRequestedOutputs() const;
  /** @brief Reports whether at least one requested output remains enabled. */
  bool hasEnabledOutputs() const;
};

/**
 * @brief Validates output requirements and returns the formats that can be generated.
 * @param request User-requested output formats, targets, and filters.
 * @param rawInputPath Raw input used to derive default target names.
 * @param ctraceRunMeta Metadata required by output formats.
 * @param diagnostics Sink receiving independently reported format failures.
 * @return Plan retaining every requested format whose requirements are satisfied.
 */
TraceOutputPlan planTraceOutputs(const TraceOutputRequest& request, const std::filesystem::path& rawInputPath,
                                 const CtraceRunMeta& ctraceRunMeta, DiagnosticSink& diagnostics);

#endif  // CTRACE_SRC_OUTPUT_OUTPUTREQUIREMENTS_H
