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
static std::string formatDiagnosticEvent(const DiagnosticSink::Event& event)
{
  std::ostringstream out;
  out << "[" << toString(event.severity) << "] " << event.message;
  for (std::size_t index = 0; index < event.context.size(); ++index) {
    const auto& item = event.context[index];
    out << (index == 0U ? ": " : ", ") << item.first << "=" << item.second;
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

void StderrDiagnosticSink::write(const Event& event)
{
  std::cerr << formatDiagnosticEvent(event);
}
