/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "DecodeConsumers.h"

#include "DiagnosticSink.h"
#include "TraceEvent.h"
#include "TraceOutput.h"
#include "TraceOutputLifecycle.h"

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

/** @brief Formats an ITM enable mask as a fixed-width hexadecimal value. */
static std::string hexMask(std::uint32_t value)
{
  std::ostringstream out;
  out << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
  return out.str();
}

DecodeConsumers::DecodeConsumers(std::vector<std::unique_ptr<TraceOutput>> outputs, DiagnosticSink& diagnostics,
                                 std::optional<std::uint32_t> itmEnableMask,
                                 std::map<std::uint8_t, std::uint32_t> itmEnableMasksByTraceBusId)
  : m_diagnostics(diagnostics), m_itmEnableMask(itmEnableMask),
    m_itmEnableMasksByTraceBusId(std::move(itmEnableMasksByTraceBusId)), m_issueReporter(diagnostics),
    m_outputLifecycle(std::move(outputs), diagnostics)
{
}

void DecodeConsumers::append(const TraceEvent& event)
{
  ++m_eventCount;
  m_outputLifecycle.append(event);
  reportItmConfigurationMismatch(event);
  m_issueReporter.append(event);
}

void DecodeConsumers::reportItmConfigurationMismatch(const TraceEvent& event)
{
  const auto* software = traceEventPayload<SoftwareTraceEvent>(event);
  if (software == nullptr || software->channel == 0U || software->channel >= 32U) {
    return;
  }

  auto enableMask = m_itmEnableMask;
  const auto streamMask = m_itmEnableMasksByTraceBusId.find(event.traceBusId);
  if (streamMask != m_itmEnableMasksByTraceBusId.end()) {
    enableMask = streamMask->second;
  }
  if (!enableMask.has_value() || ((*enableMask & (1U << software->channel)) != 0U) ||
      !m_reportedDisabledItmChannels.emplace(event.traceBusId, software->channel).second) {
    return;
  }

  m_diagnostics.report({
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
  return m_eventCount;
}

void DecodeConsumers::finishIssues()
{
  m_issueReporter.finish();
}

void DecodeConsumers::finishOutputs() noexcept
{
  m_outputLifecycle.finish();
}

void DecodeConsumers::abortOutputs() noexcept
{
  m_outputLifecycle.abort();
}
