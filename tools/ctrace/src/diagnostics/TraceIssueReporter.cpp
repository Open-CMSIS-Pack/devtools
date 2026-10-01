/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceIssueReporter.h"

#include "DiagnosticSink.h"
#include "TraceEvent.h"
#include "TraceMessages.h"
#include "TraceRoute.h"

#include <string>
#include <utility>
#include <vector>

/** @brief Builds public stream context when a route has an architectural ID. */
static std::vector<std::pair<std::string, std::string>> routeContext(const TraceRouteIdentity& route)
{
  if (!route.traceBusId.has_value()) {
    return {};
  }
  return {{"stream", std::to_string(*route.traceBusId)}};
}

TraceIssueReporter::TraceIssueReporter(DiagnosticSink& diagnostics)
  : m_diagnostics(diagnostics)
{
}

void TraceIssueReporter::append(const TraceEvent& event)
{
  if (isTraceEvent<OverflowTraceEvent>(event)) {
    reportOverflow(event);
    return;
  }
  if (const auto* issue = traceEventPayload<TraceIssueEvent>(event)) {
    reportError(event, *issue);
  }
}

void TraceIssueReporter::finish()
{
  if (m_finished) {
    return;
  }
  m_finished = true;
  for (const auto& [routeId, state] : m_overflowByRoute) {
    (void)routeId;
    const TraceOverflowSummary summary{state.firstTimestamp, state.packetCount};
    report(DiagnosticSink::Severity::Warning,
           formatOverflowSummary(summary, TraceMessageStyle::Compact),
           formatOverflowSummary(summary, TraceMessageStyle::Detailed), routeContext(state.route));
  }
}

void TraceIssueReporter::reportOverflow(const TraceEvent& event)
{
  auto& state = m_overflowByRoute[event.route.id];
  if (state.packetCount == 0U) {
    state.route = event.route;
    state.firstTimestamp = event.tcyc;
  }
  ++state.packetCount;
}

void TraceIssueReporter::reportError(const TraceEvent& event, const TraceIssueEvent& issue)
{
  report(issue.severity == TraceIssueSeverity::Warning ? DiagnosticSink::Severity::Warning
                                                       : DiagnosticSink::Severity::Error,
         formatTraceIssue(issue, event.index, TraceMessageStyle::Compact),
         formatTraceIssue(issue, event.index, TraceMessageStyle::Detailed), routeContext(event.route),
         {{"raw_offset", std::to_string(event.index)}});
}

void TraceIssueReporter::report(DiagnosticSink::Severity severity, std::string message, std::string detailedMessage,
                                std::vector<std::pair<std::string, std::string>> context,
                                std::vector<std::pair<std::string, std::string>> detailedContext)
{
  DiagnosticSink::Event event{severity, std::move(message), std::move(context)};
  event.detailedMessage = std::move(detailedMessage);
  event.detailedContext = std::move(detailedContext);
  m_diagnostics.report(event);
}
