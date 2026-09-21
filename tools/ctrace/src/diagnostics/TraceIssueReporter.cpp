/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceIssueReporter.h"

#include "DiagnosticSink.h"
#include "TraceEvent.h"
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

/** @brief Appends a raw input offset to a diagnostic when available. */
static std::string atRawOffset(const std::string& message, const TraceEvent& event)
{
  return message + " at raw offset " + std::to_string(event.index);
}

/** @brief Creates the concise user-facing representation of a trace issue. */
static std::string displayErrorMessage(const TraceEvent& event, const TraceIssueEvent& issue)
{
  switch (issue.code) {
  case TraceIssueCode::DataLoss:
    if (issue.rawBytesConsumed.has_value()) {
      return std::to_string(*issue.rawBytesConsumed) + " raw bytes from raw offset " + std::to_string(event.index) +
             " could not be decoded before the next hardware ITM sync";
    }
    return "trace data at raw offset " + std::to_string(event.index) +
           " could not be decoded before the next hardware ITM sync";
  case TraceIssueCode::OpenCsdBadPacketSequence:
    return atRawOffset("invalid ITM packet sequence", event);
  case TraceIssueCode::OpenCsdInvalidPacketHeader:
    return atRawOffset("invalid ITM packet header", event);
  case TraceIssueCode::OpenCsdIncompleteTail:
    return "incomplete ITM packet starting at raw offset " + std::to_string(event.index) + " at end of input";
  case TraceIssueCode::OpenCsdMissingSync:
    return issue.message.empty() ? "no hardware ITM SYNC before end of input" : issue.message;
  case TraceIssueCode::OpenCsdNoProgress:
    return atRawOffset("OpenCSD made no decode progress", event);
  case TraceIssueCode::OpenCsdWaitTimeout:
    return "OpenCSD remained blocked while flushing pending data";
  case TraceIssueCode::OpenCsdInitializationError:
    return "OpenCSD initialization failed";
  case TraceIssueCode::DecodeError:
  case TraceIssueCode::InvalidExceptionAction:
  case TraceIssueCode::UnsupportedDwtEventCounterPayload:
  case TraceIssueCode::UnsupportedPmuEventCounterPayload:
  case TraceIssueCode::UnsupportedDwtAddressPayload:
  case TraceIssueCode::UnsupportedDwtPcSamplePayload:
  case TraceIssueCode::OpenCsdDecodeError:
    return atRawOffset("trace decode error", event);
  }
  return atRawOffset("trace decode error", event);
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
    const auto additionalOverflows = state.packetCount - 1U;
    const auto firstOverflow = state.firstTimestamp.has_value()
                                   ? "cycle timestamp " + std::to_string(*state.firstTimestamp)
                                   : std::string("an unknown cycle timestamp");
    auto summary = "first overflow occurred at " + firstOverflow;
    if (additionalOverflows > 0U) {
      summary += "; " + std::to_string(additionalOverflows) + " more occurred";
    }
    report(DiagnosticSink::Severity::Warning, std::move(summary), routeContext(state.route));
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
         displayErrorMessage(event, issue), routeContext(event.route));
}

void TraceIssueReporter::report(DiagnosticSink::Severity severity, std::string message,
                                std::vector<std::pair<std::string, std::string>> context)
{
  m_diagnostics.report({
      severity,
      std::move(message),
      std::move(context),
  });
}
