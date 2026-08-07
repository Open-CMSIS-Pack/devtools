/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_CTFBUNDLEOUTPUT_H
#define CTRACE_SRC_OUTPUT_CTF_CTFBUNDLEOUTPUT_H

#include "CtfEncoder.h"
#include "TraceEvent.h"
#include "TraceOutput.h"
#include "TraceOutputConfig.h"

#include <filesystem>

class DiagnosticSink;

/** @brief Owns a CTF directory and its companion Trace Compass XML file. */
class CtfBundleOutput final : public TraceOutput {
public:
  /**
   * @brief Creates a CTF bundle output from validated configuration.
   * @param config CTF paths, clock metadata, and event filters.
   * @param diagnostics Optional sink for non-fatal output diagnostics.
   */
  explicit CtfBundleOutput(CtfOutputConfig config, DiagnosticSink* diagnostics = nullptr);
  /** @brief Aborts an active bundle before destruction. */
  ~CtfBundleOutput() override;

  /** @brief Prepares empty CTF and XML targets. */
  void start() override;
  /** @brief Completes metadata, stream, and XML output. */
  void stop() override;
  /** @brief Removes incomplete CTF and XML targets. */
  void abort() override;
  /**
   * @brief Encodes one selected semantic event.
   * @param event Event evaluated and encoded by the CTF backend.
   */
  void writeEvent(const TraceEvent& event) override;
  /** @brief Returns the CTF backend name. */
  std::string_view backendName() const noexcept override;
  /** @brief Returns the CTF output directory path. */
  std::string targetPath() const override;

private:
  std::filesystem::path m_ctfOutputDirectory;
  std::filesystem::path m_traceCompassXmlPath;
  CtfEncoder m_encoder;
  bool m_active = false;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_CTFBUNDLEOUTPUT_H
