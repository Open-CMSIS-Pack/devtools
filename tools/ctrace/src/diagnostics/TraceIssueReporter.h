/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DIAGNOSTICS_TRACEISSUEREPORTER_H
#define CTRACE_SRC_DIAGNOSTICS_TRACEISSUEREPORTER_H

#include "DiagnosticSink.h"
#include "RawDiagnosticLocation.h"
#include "TraceEvent.h"
#include "TraceRoute.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/** @brief Converts trace issue and overflow events into structured diagnostics. */
class TraceIssueReporter final : public TraceEventSink {
public:
  /** @brief Creates a reporter that writes to the supplied diagnostic sink. */
  explicit TraceIssueReporter(DiagnosticSink& diagnostics);

  /** @brief Observes one event and reports issue-bearing payloads. */
  void append(const TraceEvent& event) override;
  /** @brief Emits deferred summary diagnostics once decoding is complete. */
  void finish();

private:
  /** @brief Stores deferred overflow state for one normalized route. */
  struct OverflowState {
    TraceRouteIdentity route;
    std::optional<std::uint64_t> firstTimestamp;
    std::uint64_t packetCount = 0U;
    RawDiagnosticLocation firstLocation;
    std::uint64_t lastOffset = 0U;
    std::vector<std::uint64_t> sampleOffsets;
  };

  /** @brief Retains synchronization anchors independently for each normalized route. */
  void observeSync(const TraceEvent& event);
  /** @brief Accumulates overflow state for the final summary. */
  void reportOverflow(const TraceEvent& event);
  /** @brief Reports one semantic decoder issue. */
  void reportError(const TraceEvent& event, const TraceIssueEvent& issue);
  /** @brief Associates a decoder position with its known preceding route-local synchronization. */
  RawDiagnosticLocation rawLocationFor(const TraceRouteIdentity& route, std::uint64_t offset) const;
  /** @brief Submits one normalized trace diagnostic to the sink. */
  void report(DiagnosticSink::Severity severity, std::string message, std::string detailedMessage,
              std::vector<std::pair<std::string, std::string>> context = {},
              std::vector<std::pair<std::string, std::string>> detailedContext = {},
              std::optional<RawDiagnosticLocation> rawLocation = std::nullopt);

  DiagnosticSink& m_diagnostics;
  std::map<TraceRouteId, OverflowState> m_overflowByRoute;
  std::map<TraceRouteId, std::uint64_t> m_lastSyncByRoute;
  bool m_finished = false;
};

#endif  // CTRACE_SRC_DIAGNOSTICS_TRACEISSUEREPORTER_H
