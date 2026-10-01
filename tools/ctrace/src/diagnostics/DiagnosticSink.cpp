/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "DiagnosticSink.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

/** @brief Renders a structured diagnostic in the stable command-line format. */
static std::string formatDiagnosticEvent(const DiagnosticSink::Event& event, bool verbose)
{
  std::ostringstream out;
  out << "[" << toString(event.severity) << "] "
      << (verbose && event.detailedMessage.has_value() ? *event.detailedMessage : event.message);
  bool firstContext = true;
  const auto appendContext = [&](const auto& context) {
    for (const auto& item : context) {
      out << (firstContext ? ": " : ", ") << item.first << "=" << item.second;
      firstContext = false;
    }
  };
  appendContext(event.context);
  if (verbose) {
    appendContext(event.detailedContext);
  }
  out << "\n";
  return out.str();
}

void DiagnosticSink::report(const Event& event)
{
  if (event.impact == Impact::Failing) {
    ++m_failureCount;
  }
  write(event);
}

std::uint64_t DiagnosticSink::failureCount() const noexcept
{
  return m_failureCount;
}

std::string_view toString(DiagnosticSink::Severity severity)
{
  switch (severity) {
  case DiagnosticSink::Severity::Info:
    return "info";
  case DiagnosticSink::Severity::Warning:
    return "warning";
  case DiagnosticSink::Severity::Error:
    return "error";
  }
  return "unknown";
}

void StderrDiagnosticSink::setVerbose(bool verbose) noexcept
{
  m_verbose = verbose;
}

void StderrDiagnosticSink::write(const Event& event)
{
  if (!m_verbose && event.visibility == Visibility::Verbose) {
    return;
  }
  std::cerr << formatDiagnosticEvent(event, m_verbose);
}
