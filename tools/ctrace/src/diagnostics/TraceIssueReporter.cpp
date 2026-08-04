/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceIssueReporter.h"

#include "DiagnosticSink.h"
#include "TraceEvent.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

/** @brief Appends a raw input offset to a diagnostic when available. */
static std::string atRawOffset(const std::string& message, const TraceEvent& event)
{
  return message + " at raw offset " + std::to_string(event.index);
}

/** @brief Creates the concise CSV representation of a trace issue. */
static std::string compactErrorMessage(const TraceEvent& event, const TraceIssueEvent& issue, const std::string& code)
{
  if (code == "data-loss") {
    if (issue.rawBytesConsumed.has_value()) {
      return std::to_string(*issue.rawBytesConsumed) + " raw bytes from raw offset " + std::to_string(event.index) +
             " could not be decoded before the next hardware ITM sync";
    }
    return "trace data at raw offset " + std::to_string(event.index) +
           " could not be decoded before the next hardware ITM sync";
  }
  if (code == "opencsd-bad-packet-sequence") {
    return atRawOffset("invalid ITM packet sequence", event);
  }
  if (code == "opencsd-invalid-packet-header") {
    return atRawOffset("invalid ITM packet header", event);
  }
  if (code == "opencsd-incomplete-tail") {
    return "incomplete ITM packet starting at raw offset " + std::to_string(event.index) + " at end of input";
  }
  if (code == "opencsd-no-progress") {
    return atRawOffset("OpenCSD made no decode progress", event);
  }
  if (code == "opencsd-wait-timeout") {
    return "OpenCSD remained blocked while flushing pending data";
  }
  if (code == "opencsd-initialization-error") {
    return "OpenCSD initialization failed";
  }
  return atRawOffset("trace decode error", event);
}

TraceIssueReporter::TraceIssueReporter(DiagnosticSink& diagnostics) : m_diagnostics(diagnostics) {}

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
  if (m_overflowPackets == 0U) {
    return;
  }

  const auto additionalOverflows = m_overflowPackets - 1U;
  const auto firstOverflow = m_firstOverflowTimestamp.has_value()
                                 ? "cycle timestamp " + std::to_string(*m_firstOverflowTimestamp)
                                 : std::string("an unknown cycle timestamp");
  auto summary = "first overflow occurred at " + firstOverflow;
  if (additionalOverflows > 0U) {
    summary += "; " + std::to_string(additionalOverflows) + " more occurred";
  }
  report(DiagnosticSink::Severity::Warning, "overflow",
         "SWO " + summary +
             ". Payload trace was lost at the overflow boundaries, and timestamp continuity after "
             "the first overflow may be incomplete or incorrect.",
         summary);
}

void TraceIssueReporter::reportOverflow(const TraceEvent& event)
{
  if (m_overflowPackets == 0U) {
    m_firstOverflowTimestamp = event.tcyc;
  }
  ++m_overflowPackets;
}

void TraceIssueReporter::reportError(const TraceEvent& event, const TraceIssueEvent& issue)
{
  const auto code = issue.code.empty() ? std::string("decode-error") : issue.code;
  if (code == "data-loss") {
    report(issue.severity == TraceIssueSeverity::Warning ? DiagnosticSink::Severity::Warning
                                                         : DiagnosticSink::Severity::Error,
           code,
           issue.message.empty() ? "Trace data loss detected while the decoder was not synchronized. Raw bytes were "
                                   "present, but OpenCSD could not turn them into reliable trace packets until a later "
                                   "sync/recovery point."
                                 : issue.message,
           compactErrorMessage(event, issue, code));
    return;
  }

  report(issue.severity == TraceIssueSeverity::Warning ? DiagnosticSink::Severity::Warning
                                                       : DiagnosticSink::Severity::Error,
         code, issue.message.empty() ? "decode error detected" : issue.message,
         compactErrorMessage(event, issue, code));
}

void TraceIssueReporter::report(DiagnosticSink::Severity severity, std::string code, std::string message,
                                std::optional<std::string> compactMessage)
{
  m_diagnostics.report({
      severity,
      DiagnosticSink::Category::Decode,
      std::move(code),
      std::move(message),
      {},
      std::move(compactMessage),
  });
}
