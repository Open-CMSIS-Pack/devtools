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
#include "TraceRoute.h"
#include "TraceStreamId.h"

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
                                 std::map<TraceRouteId, std::uint32_t> itmEnableMasksByRoute)
  : m_diagnostics(diagnostics),
    m_itmEnableMasksByRoute(std::move(itmEnableMasksByRoute)),
    m_issueReporter(diagnostics),
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

void DecodeConsumers::appendByteSkip(const TraceByteSkip& skipped)
{
  ++m_eventCount;
  m_outputLifecycle.appendByteSkip(skipped);
  std::vector<std::pair<std::string, std::string>> context;
  if (skipped.traceId.has_value()) {
    context.emplace_back("stream", std::to_string(*skipped.traceId));
  }
  m_diagnostics.report({DiagnosticSink::Severity::Info, traceByteSkipMessage(skipped), std::move(context)});
}

void DecodeConsumers::reportItmConfigurationMismatch(const TraceEvent& event)
{
  const auto* software = traceEventPayload<SoftwareTraceEvent>(event);
  if (software == nullptr || software->channel == CoreSight::kExcludedItmStimulusPort ||
      !CoreSight::isItmStimulusPort(software->channel)) {
    return;
  }

  const auto streamMask = m_itmEnableMasksByRoute.find(event.route.id);
  if (streamMask == m_itmEnableMasksByRoute.end() ||
      ((streamMask->second & (1U << software->channel)) != 0U) ||
      !m_reportedDisabledItmChannels.emplace(event.route.id, software->channel).second) {
    return;
  }

  std::vector<std::pair<std::string, std::string>> context;
  if (event.route.traceBusId.has_value()) {
    context.emplace_back("stream", std::to_string(*event.route.traceBusId));
  }
  context.emplace_back("channel", std::to_string(software->channel));
  context.emplace_back("enable", hexMask(streamMask->second));
  m_diagnostics.report({
      DiagnosticSink::Severity::Warning,
      "ITM data was received on a channel not enabled by ctrace-setup.itm.enable",
      std::move(context),
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

void DecodeConsumers::finishOutputs(const TraceDecodeAbort* decodeAbort) noexcept
{
  m_outputLifecycle.finish(decodeAbort);
}

void DecodeConsumers::abortOutputs() noexcept
{
  m_outputLifecycle.abort();
}
