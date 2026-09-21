/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_CONTROL_DECODECONSUMERS_H
#define CTRACE_SRC_CONTROL_DECODECONSUMERS_H

#include "DiagnosticSink.h"
#include "TraceEvent.h"
#include "TraceIssueReporter.h"
#include "TraceOutput.h"
#include "TraceOutputLifecycle.h"
#include "TraceRoute.h"

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

/** @brief Fans decoded events out to outputs, diagnostics, and configuration checks. */
class DecodeConsumers final : public TraceEventSink {
public:
  /** @brief Creates the consumers for one raw trace input. */
  DecodeConsumers(std::vector<std::unique_ptr<TraceOutput>> outputs, DiagnosticSink& diagnostics,
                  std::map<TraceRouteId, std::uint32_t> itmEnableMasksByRoute = {});

  /** @brief Forwards one decoded event to all configured consumers. */
  void append(const TraceEvent& event) override;
  /** @brief Reports skipped input payload without assigning a synthetic trace route. */
  void appendByteSkip(const TraceByteSkip& skipped) override;
  /** @brief Returns the number of trace/diagnostic records observed before output filtering. */
  std::uint64_t eventCount() const;
  /** @brief Completes deferred issue reporting. */
  void finishIssues();
  /** @brief Completes output artifacts using each backend's optional fatal-input policy. */
  void finishOutputs(const TraceDecodeAbort* decodeAbort = nullptr) noexcept;
  /** @brief Aborts and removes partial output artifacts without throwing. */
  void abortOutputs() noexcept;

private:
  /** @brief Reports an ITM event that contradicts the configured enable mask. */
  void reportItmConfigurationMismatch(const TraceEvent& event);

  DiagnosticSink& m_diagnostics;
  std::map<TraceRouteId, std::uint32_t> m_itmEnableMasksByRoute;
  std::set<std::pair<TraceRouteId, std::uint32_t>> m_reportedDisabledItmChannels;
  TraceIssueReporter m_issueReporter;
  TraceOutputLifecycle m_outputLifecycle;
  std::uint64_t m_eventCount = 0;
};

#endif  // CTRACE_SRC_CONTROL_DECODECONSUMERS_H
