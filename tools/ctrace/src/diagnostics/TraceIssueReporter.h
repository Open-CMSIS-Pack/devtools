/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DIAGNOSTICS_TRACEISSUEREPORTER_H
#define CTRACE_SRC_DIAGNOSTICS_TRACEISSUEREPORTER_H

#include "DiagnosticSink.h"
#include "TraceEvent.h"

#include <cstdint>
#include <optional>
#include <string>

class TraceIssueReporter final : public TraceEventSink {
public:
  explicit TraceIssueReporter(DiagnosticSink& diagnostics);

  void append(const TraceEvent& event) override;
  void finish();

private:
  void reportOverflow(const TraceEvent& event);
  void reportError(const TraceEvent& event, const TraceIssueEvent& issue);
  void report(DiagnosticSink::Severity severity, std::string code, std::string message,
              std::optional<std::string> compactMessage = std::nullopt);

  DiagnosticSink& diagnostics_;
  std::optional<std::uint64_t> firstOverflowTimestamp_;
  std::uint64_t overflowPackets_ = 0;
  bool finished_ = false;
};

#endif  // CTRACE_SRC_DIAGNOSTICS_TRACEISSUEREPORTER_H
