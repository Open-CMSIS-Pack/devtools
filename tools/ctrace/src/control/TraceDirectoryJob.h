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
#include "ctf/CtfMetadataModel.h"

#include <filesystem>
#include <optional>
#include <vector>

struct TraceRunConfig;
struct TraceRunRawInput;

/** @brief Discovers and decodes the selected trace-run configurations in a directory. */
class TraceDirectoryJob {
public:
  /**
   * @brief Creates a directory job with parsed options and a trace-run reader.
   * @param options Validated command-line options selecting the trace directory.
   * @param diagnostics Sink receiving discovery and decode diagnostics.
   * @param configReader Reader used for every selected trace-run file.
   */
  TraceDirectoryJob(CliOptions options, DiagnosticSink& diagnostics, const TraceRunConfigReader& configReader);

  /**
   * @brief Runs discovery and all selected file decode jobs.
   *
   * Independent input failures are reported and do not prevent later selected
   * inputs from being processed.
   */
  void run();

private:
  /** @brief Reads one configuration and processes all its supported raw inputs. */
  void processConfigFile(const std::filesystem::path& configFile);
  /** @brief Collects completed per-input CTF metadata into one target-level XML. */
  void processInputs(const TraceRunConfig& config, const std::vector<TraceRunRawInput>& inputs,
                      const std::filesystem::path& xmlPath);
  /** @brief Isolates one input's preflight, decoding, outputs, and contextual diagnostics. */
  std::optional<CtfMetadataModel> processInput(const TraceRunConfig& config, const TraceRunRawInput& rawInput);

  CliOptions m_options;
  DiagnosticSink& m_diagnostics;
  const TraceRunConfigReader& m_configReader;
};

#endif  // CTRACE_SRC_CONTROL_TRACEDIRECTORYJOB_H
