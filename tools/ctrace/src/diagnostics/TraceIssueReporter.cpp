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

#include <cstddef>
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

/** @brief Formats the bounded set of representative overflow offsets. */
static std::string formatSampleOffsets(const std::vector<std::uint64_t>& offsets)
{
  std::string result = "[";
  for (const auto offset : offsets) {
    if (result.size() > 1U) {
      result += ',';
    }
    result += std::to_string(offset);
  }
  result += ']';
  return result;
}

TraceIssueReporter::TraceIssueReporter(DiagnosticSink& diagnostics)
  : m_diagnostics(diagnostics)
{
}

void TraceIssueReporter::append(const TraceEvent& event)
{
  if (isTraceEvent<SyncTraceEvent>(event)) {
    observeSync(event);
    return;
  }
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
    auto sampleOffsets = formatSampleOffsets(state.sampleOffsets);
    report(DiagnosticSink::Severity::Warning,
           formatOverflowSummary(summary, TraceMessageStyle::Compact),
           formatOverflowSummary(summary, TraceMessageStyle::Detailed), routeContext(state.route),
           {{"overflow_count", std::to_string(state.packetCount)},
            {"first_raw_offset", std::to_string(state.firstLocation.offset)},
            {"last_raw_offset", std::to_string(state.lastOffset)},
            {"sample_raw_offsets", std::move(sampleOffsets)},
            {"omitted_offsets", std::to_string(state.packetCount - state.sampleOffsets.size())}},
           state.firstLocation);
  }
}

void TraceIssueReporter::observeSync(const TraceEvent& event)
{
  m_lastSyncByRoute[event.route.id] = event.index;
  const auto overflow = m_overflowByRoute.find(event.route.id);
  if (overflow != m_overflowByRoute.end()) {
    auto& location = overflow->second.firstLocation;
    if (!location.nextSyncOffset.has_value() && event.index >= location.offset) {
      location.nextSyncOffset = event.index;
    }
  }
}

void TraceIssueReporter::reportOverflow(const TraceEvent& event)
{
  auto& state = m_overflowByRoute[event.route.id];
  if (state.packetCount == 0U) {
    state.route = event.route;
    state.firstTimestamp = event.tcyc;
    state.firstLocation = rawLocationFor(event.route, event.index);
  }
  ++state.packetCount;
  state.lastOffset = event.index;
  constexpr std::size_t maxSampleOffsets = 3U;
  if (state.sampleOffsets.size() < maxSampleOffsets) {
    state.sampleOffsets.push_back(event.index);
  }
}

void TraceIssueReporter::reportError(const TraceEvent& event, const TraceIssueEvent& issue)
{
  auto location = rawLocationFor(event.route, event.index);
  if (issue.code == TraceIssueCode::OpenCsdInitializationError) {
    location.kind = RawDiagnosticLocation::Kind::InputProgress;
  }
  if (const auto* recovery = std::get_if<TraceRecovery>(&issue.message.data)) {
    if (recovery->kind != TraceRecoveryKind::Generic) {
      location = rawLocationFor(event.route, recovery->startOffset);
      location.endOffset = recovery->endOffset;
      if (recovery->kind == TraceRecoveryKind::Resumed) {
        location.nextSyncOffset = recovery->endOffset;
      }
    }
  }
  if (issue.message.packet.has_value()) {
    location.packetSize = issue.message.packet->size;
  }
  report(issue.severity == TraceIssueSeverity::Warning ? DiagnosticSink::Severity::Warning
                                                       : DiagnosticSink::Severity::Error,
         formatTraceIssue(issue, event.index, TraceMessageStyle::Compact),
         formatTraceIssue(issue, event.index, TraceMessageStyle::Detailed), routeContext(event.route), {}, location);
}

RawDiagnosticLocation TraceIssueReporter::rawLocationFor(const TraceRouteIdentity& route, std::uint64_t offset) const
{
  RawDiagnosticLocation location{offset};
  const auto sync = m_lastSyncByRoute.find(route.id);
  if (sync != m_lastSyncByRoute.end() && sync->second <= offset) {
    location.previousSyncOffset = sync->second;
  }
  return location;
}

void TraceIssueReporter::report(DiagnosticSink::Severity severity, std::string message, std::string detailedMessage,
                                std::vector<std::pair<std::string, std::string>> context,
                                std::vector<std::pair<std::string, std::string>> detailedContext,
                                const RawDiagnosticLocation& rawLocation)
{
  DiagnosticSink::Event event{severity, std::move(message), std::move(context)};
  event.detailedMessage = std::move(detailedMessage);
  event.detailedContext = std::move(detailedContext);
  event.rawLocation = rawLocation;
  m_diagnostics.report(event);
}
