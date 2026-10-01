/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_DIAGNOSTICS_RAWDIAGNOSTICSINK_H
#define CTRACE_SRC_DIAGNOSTICS_RAWDIAGNOSTICSINK_H

#include "DiagnosticSink.h"

#include <cstdint>
#include <optional>

/** @brief Adds bounded raw-inspection context without reading or changing trace data. */
class RawDiagnosticSink final : public DiagnosticSink {
public:
  /** @brief Binds typed locations to the effective format and known input size. */
  RawDiagnosticSink(DiagnosticSink& target, bool formatted, std::optional<std::uint64_t> inputSize);

protected:
  /** @brief Preserves the diagnostic and enriches its verbose-only context. */
  void write(const Event& event) override;

private:
  DiagnosticSink& m_target;
  bool m_formatted;
  std::optional<std::uint64_t> m_inputSize;
};

#endif // CTRACE_SRC_DIAGNOSTICS_RAWDIAGNOSTICSINK_H
