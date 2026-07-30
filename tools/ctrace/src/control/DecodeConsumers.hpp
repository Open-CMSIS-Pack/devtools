/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "DiagnosticSink.hpp"
#include "TraceEvent.hpp"
#include "TraceIssueReporter.hpp"
#include "TraceOutput.hpp"
#include "TraceOutputLifecycle.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

class DecodeConsumers final : public TraceEventSink {
public:
  DecodeConsumers(std::vector<std::unique_ptr<TraceOutput>> outputs, DiagnosticSink& diagnostics,
                  std::optional<std::uint32_t> itmEnableMask = std::nullopt,
                  std::map<std::uint8_t, std::uint32_t> itmEnableMasksByTraceBusId = {});

  void append(const TraceEvent& event) override;
  std::uint64_t eventCount() const;
  void finishIssues();
  void finishOutputs() noexcept;
  void abortOutputs() noexcept;

private:
  void reportItmConfigurationMismatch(const TraceEvent& event);

  DiagnosticSink& diagnostics_;
  std::optional<std::uint32_t> itmEnableMask_;
  std::map<std::uint8_t, std::uint32_t> itmEnableMasksByTraceBusId_;
  std::set<std::pair<std::uint8_t, std::uint32_t>> reportedDisabledItmChannels_;
  TraceIssueReporter issueReporter_;
  TraceOutputLifecycle outputLifecycle_;
  std::uint64_t eventCount_ = 0;
};
