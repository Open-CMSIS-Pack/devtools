/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class DiagnosticSink {
public:
  enum class Severity {
    Info,
    Warning,
    Error,
  };

  enum class Category {
    Cli,
    Input,
    Decode,
    Output,
  };

  enum class Impact {
    NonFatal,
    Fatal,
  };

  struct Event {
    Event() = default;

    Event(Severity eventSeverity, Category eventCategory, std::string eventCode, std::string eventMessage,
          std::vector<std::pair<std::string, std::string>> eventContext = {},
          std::optional<std::string> eventCompactMessage = std::nullopt,
          std::optional<Impact> eventImpact = std::nullopt)
      : severity(eventSeverity), category(eventCategory), code(std::move(eventCode)), message(std::move(eventMessage)),
        context(std::move(eventContext)), compactMessage(std::move(eventCompactMessage)),
        impact(eventImpact.value_or(eventSeverity == Severity::Error ? Impact::Fatal : Impact::NonFatal))
    {
    }

    Severity severity = Severity::Info;
    Category category = Category::Cli;
    std::string code;
    std::string message;
    std::vector<std::pair<std::string, std::string>> context;
    std::optional<std::string> compactMessage;
    Impact impact = Impact::NonFatal;
  };

  virtual ~DiagnosticSink() = default;

  void report(const Event& event);
  std::uint64_t fatalCount() const noexcept;
  bool hasFatalSince(std::uint64_t checkpoint) const noexcept;

protected:
  virtual void write(const Event& event) = 0;

private:
  std::uint64_t fatalCount_ = 0;
};

class StderrDiagnosticSink final : public DiagnosticSink {
protected:
  void write(const Event& event) override;
};

std::string_view toString(DiagnosticSink::Severity severity);
std::string_view toString(DiagnosticSink::Category category);
std::string_view toString(DiagnosticSink::Impact impact);
