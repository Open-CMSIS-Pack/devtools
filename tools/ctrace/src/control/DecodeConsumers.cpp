/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "DecodeConsumers.hpp"

#include "DiagnosticSink.hpp"
#include "TraceEvent.hpp"
#include "TraceOutput.hpp"
#include "TraceOutputLifecycle.hpp"

#include <cstdint>
#include <iomanip>
#include <ios>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string hexMask(std::uint32_t value)
{
  std::ostringstream out;
  out << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
  return out.str();
}

} // namespace

DecodeConsumers::DecodeConsumers(std::vector<std::unique_ptr<TraceOutput>> outputs, DiagnosticSink& diagnostics,
                                 std::optional<std::uint32_t> itmEnableMask,
                                 std::map<std::uint8_t, std::uint32_t> itmEnableMasksByTraceBusId)
  : diagnostics_(diagnostics), itmEnableMask_(itmEnableMask),
    itmEnableMasksByTraceBusId_(std::move(itmEnableMasksByTraceBusId)), issueReporter_(diagnostics),
    outputLifecycle_(std::move(outputs), diagnostics)
{
}

void DecodeConsumers::append(const TraceEvent& event)
{
  ++eventCount_;
  outputLifecycle_.append(event);
  reportItmConfigurationMismatch(event);
  issueReporter_.append(event);
}

void DecodeConsumers::reportItmConfigurationMismatch(const TraceEvent& event)
{
  const auto* software = traceEventPayload<SoftwareTraceEvent>(event);
  if (software == nullptr || software->channel == 0U || software->channel >= 32U) {
    return;
  }

  auto enableMask = itmEnableMask_;
  const auto streamMask = itmEnableMasksByTraceBusId_.find(event.traceBusId);
  if (streamMask != itmEnableMasksByTraceBusId_.end()) {
    enableMask = streamMask->second;
  }
  if (!enableMask.has_value() || ((*enableMask & (1U << software->channel)) != 0U) ||
      !reportedDisabledItmChannels_.emplace(event.traceBusId, software->channel).second) {
    return;
  }

  diagnostics_.report({
      DiagnosticSink::Severity::Warning,
      DiagnosticSink::Category::Input,
      "itm-channel-not-enabled",
      "ITM data was received on a channel not enabled by ctrace-setup.itm.enable",
      {
          {"stream", std::to_string(event.traceBusId)},
          {"channel", std::to_string(software->channel)},
          {"enable", hexMask(*enableMask)},
      },
  });
}

std::uint64_t DecodeConsumers::eventCount() const
{
  return eventCount_;
}

void DecodeConsumers::finishIssues()
{
  issueReporter_.finish();
}

void DecodeConsumers::finishOutputs() noexcept
{
  outputLifecycle_.finish();
}

void DecodeConsumers::abortOutputs() noexcept
{
  outputLifecycle_.abort();
}
