/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DIAGNOSTICS_DIAGNOSTICSINK_H
#define CTRACE_SRC_DIAGNOSTICS_DIAGNOSTICSINK_H

#include "RawDiagnosticLocation.h"

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

  /** @brief Defines whether a diagnostic affects process success. */
  enum class Impact {
    NonFailing,
    Failing,
  };

  /** @brief Selects whether a diagnostic is visible during normal or only verbose CLI output. */
  enum class Visibility {
    Always,
    Verbose,
  };

  /** @brief Stores one structured diagnostic event. */
  struct Event {
    /**
     * @brief Constructs a fully classified diagnostic event.
     * @param eventSeverity Display severity.
     * @param eventMessage Normal display text, also used when no detailed alternative exists.
     * @param eventContext Structured identity and actionable context shown in both modes.
     * @param eventImpact Optional explicit process-success impact.
     */
    Event(Severity eventSeverity, std::string eventMessage,
          std::vector<std::pair<std::string, std::string>> eventContext = {},
          std::optional<Impact> eventImpact = std::nullopt)
      : severity(eventSeverity),
        message(std::move(eventMessage)),
        context(std::move(eventContext)),
        impact(eventImpact.value_or(eventSeverity == Severity::Error ? Impact::Failing : Impact::NonFailing))
    {
    }

    Severity severity = Severity::Info;
    std::string message;
    std::vector<std::pair<std::string, std::string>> context;
    Impact impact = Impact::NonFailing;
    /** @brief Optional complete description selected by verbose CLI output. */
    std::optional<std::string> detailedMessage;
    /** @brief Technical context shown only with verbose CLI output. */
    std::vector<std::pair<std::string, std::string>> detailedContext;
    /** @brief Optional typed raw location, resolved by the input-scoped diagnostic sink. */
    std::optional<RawDiagnosticLocation> rawLocation;
    Visibility visibility = Visibility::Always;
  };

  /** @brief Destroys a diagnostic sink through its interface. */
  virtual ~DiagnosticSink() = default;

  /**
   * @brief Records a diagnostic and forwards it to the concrete writer.
   * @param event Fully classified diagnostic event.
   *
   * Failing impact is counted independently of display severity and visibility.
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
public:
  /** @brief Enables detailed messages, technical context and verbose-only diagnostics. */
  void setVerbose(bool verbose) noexcept;

protected:
  /** @brief Renders one diagnostic event to standard error. */
  void write(const Event& event) override;

private:
  bool m_verbose = false;
};

/** @brief Returns the display name of a diagnostic severity. */
std::string_view toString(DiagnosticSink::Severity severity);

#endif  // CTRACE_SRC_DIAGNOSTICS_DIAGNOSTICSINK_H
