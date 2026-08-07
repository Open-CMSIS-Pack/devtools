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

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

/** @brief Fans decoded events out to outputs, diagnostics, and configuration checks. */
class DecodeConsumers final : public TraceEventSink {
public:
  /** @brief Creates the consumers for one raw trace input. */
  DecodeConsumers(std::vector<std::unique_ptr<TraceOutput>> outputs, DiagnosticSink& diagnostics,
                  std::optional<std::uint32_t> itmEnableMask = std::nullopt,
                  std::map<std::uint8_t, std::uint32_t> itmEnableMasksByTraceBusId = {});

  /** @brief Forwards one decoded event to all configured consumers. */
  void append(const TraceEvent& event) override;
  /** @brief Returns the number of events observed during decoding. */
  std::uint64_t eventCount() const;
  /** @brief Completes deferred issue reporting. */
  void finishIssues();
  /** @brief Completes all output artifacts without throwing. */
  void finishOutputs() noexcept;
  /** @brief Aborts and removes partial output artifacts without throwing. */
  void abortOutputs() noexcept;

private:
  /** @brief Reports an ITM event that contradicts the configured enable mask. */
  void reportItmConfigurationMismatch(const TraceEvent& event);

  DiagnosticSink& m_diagnostics;
  std::optional<std::uint32_t> m_itmEnableMask;
  std::map<std::uint8_t, std::uint32_t> m_itmEnableMasksByTraceBusId;
  std::set<std::pair<std::uint8_t, std::uint32_t>> m_reportedDisabledItmChannels;
  TraceIssueReporter m_issueReporter;
  TraceOutputLifecycle m_outputLifecycle;
  std::uint64_t m_eventCount = 0;
};

#endif  // CTRACE_SRC_CONTROL_DECODECONSUMERS_H
