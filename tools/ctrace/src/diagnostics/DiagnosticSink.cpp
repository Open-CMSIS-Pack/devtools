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

/** @brief Quotes verbose context values that could be mistaken for field separators. */
static std::string verboseContextValue(const std::string& value)
{
  bool quote = value.empty();
  for (const unsigned char byte : value) {
    quote = quote || byte <= 0x20U || byte == 0x7fU ||
            std::string_view(",:=\"\\[]").find(static_cast<char>(byte)) != std::string_view::npos;
  }
  if (!quote) {
    return value;
  }
  constexpr char hex[] = "0123456789abcdef";
  std::string result = "\"";
  for (const unsigned char byte : value) {
    if (byte == '"' || byte == '\\') {
      result += '\\';
      result += static_cast<char>(byte);
    } else if (byte < 0x20U || byte == 0x7fU) {
      result += "\\u00";
      result += hex[byte >> 4U];
      result += hex[byte & 0xfU];
    } else {
      result += static_cast<char>(byte);
    }
  }
  result += '"';
  return result;
}

/** @brief Renders a structured diagnostic in the stable command-line format. */
static std::string formatDiagnosticEvent(const DiagnosticSink::Event& event, bool verbose)
{
  std::ostringstream out;
  out << "[" << toString(event.severity) << "] "
      << (verbose && event.detailedMessage.has_value() ? *event.detailedMessage : event.message);
  bool firstContext = true;
  const auto appendContext = [&](const auto& context) {
    for (const auto& item : context) {
      out << (firstContext ? ": " : ", ") << item.first << "="
          << (verbose ? verboseContextValue(item.second) : item.second);
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
