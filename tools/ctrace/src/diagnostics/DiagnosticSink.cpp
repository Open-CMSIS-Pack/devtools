/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "DiagnosticSink.h"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

static std::string formatDiagnosticEvent(const DiagnosticSink::Event& event)
{
  std::ostringstream out;
  out << "[" << toString(event.severity) << "] " << toString(event.category);
  if (!event.code.empty()) {
    out << "/" << event.code;
  }
  out << ": " << event.compactMessage.value_or(event.message);
  for (const auto& item : event.context) {
    out << " " << item.first << "=" << item.second;
  }
  out << "\n";
  return out.str();
}

void DiagnosticSink::report(const Event& event)
{
  if (event.impact == Impact::Failing) {
    ++failureCount_;
  }
  write(event);
}

std::uint64_t DiagnosticSink::failureCount() const noexcept
{
  return failureCount_;
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

std::string_view toString(DiagnosticSink::Category category)
{
  switch (category) {
  case DiagnosticSink::Category::Cli:
    return "cli";
  case DiagnosticSink::Category::Input:
    return "input";
  case DiagnosticSink::Category::Decode:
    return "decode";
  case DiagnosticSink::Category::Output:
    return "output";
  }
  return "unknown";
}

std::string_view toString(DiagnosticSink::Impact impact)
{
  switch (impact) {
  case DiagnosticSink::Impact::NonFailing:
    return "non-failing";
  case DiagnosticSink::Impact::Failing:
    return "failing";
  }
  return "unknown";
}

void StderrDiagnosticSink::write(const Event& event)
{
  std::cerr << formatDiagnosticEvent(event);
}
