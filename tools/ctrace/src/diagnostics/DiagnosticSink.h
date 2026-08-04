/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DIAGNOSTICS_DIAGNOSTICSINK_H
#define CTRACE_SRC_DIAGNOSTICS_DIAGNOSTICSINK_H

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/** @brief Classifies, counts, and emits structured ctrace diagnostics. */
class DiagnosticSink {
public:
  /** @brief Defines the displayed diagnostic severity. */
  enum class Severity {
    Info,
    Warning,
    Error,
  };

  /** @brief Identifies the subsystem that produced a diagnostic. */
  enum class Category {
    Cli,
    Input,
    Decode,
    Output,
  };

  /** @brief Defines whether a diagnostic affects process success. */
  enum class Impact {
    NonFailing,
    Failing,
  };

  /** @brief Stores one structured diagnostic event. */
  struct Event {
    /** @brief Constructs an empty informational CLI diagnostic. */
    Event() = default;

    /**
     * @brief Constructs a fully classified diagnostic event.
     * @param eventSeverity Display severity.
     * @param eventCategory Originating subsystem.
     * @param eventCode Stable machine-readable code.
     * @param eventMessage Human-readable description.
     * @param eventContext Structured key-value context.
     * @param eventCompactMessage Optional compact-mode description.
     * @param eventImpact Optional explicit process-success impact.
     */
    Event(Severity eventSeverity, Category eventCategory, std::string eventCode, std::string eventMessage,
          std::vector<std::pair<std::string, std::string>> eventContext = {},
          std::optional<std::string> eventCompactMessage = std::nullopt,
          std::optional<Impact> eventImpact = std::nullopt)
      : severity(eventSeverity), category(eventCategory), code(std::move(eventCode)), message(std::move(eventMessage)),
        context(std::move(eventContext)), compactMessage(std::move(eventCompactMessage)),
        impact(eventImpact.value_or(eventSeverity == Severity::Error ? Impact::Failing : Impact::NonFailing))
    {
    }

    Severity severity = Severity::Info;
    Category category = Category::Cli;
    std::string code;
    std::string message;
    std::vector<std::pair<std::string, std::string>> context;
    std::optional<std::string> compactMessage;
    Impact impact = Impact::NonFailing;
  };

  /** @brief Destroys a diagnostic sink through its interface. */
  virtual ~DiagnosticSink() = default;

  /**
   * @brief Records a diagnostic and forwards it to the concrete writer.
   * @param event Fully classified diagnostic event.
   *
   * Failing impact is counted independently of display severity.
   */
  void report(const Event& event);
  /**
   * @brief Returns the number of diagnostics with failing impact.
   * @return Monotonic failure count accumulated by report().
   */
  std::uint64_t failureCount() const noexcept;

protected:
  /** @brief Writes one diagnostic in the representation selected by the sink. */
  virtual void write(const Event& event) = 0;

private:
  std::uint64_t m_failureCount = 0;
};

/** @brief Renders structured diagnostics to standard error. */
class StderrDiagnosticSink final : public DiagnosticSink {
protected:
  /** @brief Renders one diagnostic event to standard error. */
  void write(const Event& event) override;
};

/** @brief Returns the display name of a diagnostic severity. */
std::string_view toString(DiagnosticSink::Severity severity);
/** @brief Returns the display name of a diagnostic category. */
std::string_view toString(DiagnosticSink::Category category);
/** @brief Returns the display name of a diagnostic impact. */
std::string_view toString(DiagnosticSink::Impact impact);

#endif  // CTRACE_SRC_DIAGNOSTICS_DIAGNOSTICSINK_H
