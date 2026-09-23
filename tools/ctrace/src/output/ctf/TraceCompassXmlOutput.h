/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_TRACECOMPASSXMLOUTPUT_H
#define CTRACE_SRC_OUTPUT_CTF_TRACECOMPASSXMLOUTPUT_H

#include "TraceCompassXmlWriter.h"

#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class CtfMetadataModel;
class DiagnosticSink;

/** @brief Collects completed per-channel views into one solution-set XML file. */
class TraceCompassXmlOutput final {
public:
  /** @brief Selects the target XML and the sink for unsupported-clock warnings. */
  TraceCompassXmlOutput(std::filesystem::path path, DiagnosticSink& diagnostics);
  /** @brief Validates the target, removes stale XML, and starts a fresh collection. */
  void prepare();
  /** @brief Collects observed views from one completed, single-clock CTF bundle. */
  void add(std::string_view channel, const CtfMetadataModel& metadata);
  /** @brief Writes collected views, removing partial XML if writing fails. */
  void finish();

private:
  std::filesystem::path m_path;
  DiagnosticSink& m_diagnostics;
  std::vector<TraceCompassXmlWriter::ViewRoute> m_routes;
  std::set<std::string> m_clockUuids;
};

#endif // CTRACE_SRC_OUTPUT_CTF_TRACECOMPASSXMLOUTPUT_H
