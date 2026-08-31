/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceIssueReporter.h"

#include "DiagnosticSink.h"
#include "TraceEvent.h"

#include <string>
#include <utility>

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
  case TraceIssueCode::OpenCsdNoProgress:
    return atRawOffset("OpenCSD made no decode progress", event);
  case TraceIssueCode::OpenCsdWaitTimeout:
    return "OpenCSD remained blocked while flushing pending data";
  case TraceIssueCode::OpenCsdInitializationError:
    return "OpenCSD initialization failed";
  case TraceIssueCode::DecodeError:
  case TraceIssueCode::InvalidExceptionAction:
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
  report(DiagnosticSink::Severity::Warning, summary);
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
  report(issue.severity == TraceIssueSeverity::Warning ? DiagnosticSink::Severity::Warning
                                                       : DiagnosticSink::Severity::Error,
         displayErrorMessage(event, issue));
}

void TraceIssueReporter::report(DiagnosticSink::Severity severity, std::string message)
{
  m_diagnostics.report({
      severity,
      std::move(message),
  });
}
