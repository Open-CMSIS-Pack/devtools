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

#include <cstddef>
#include <filesystem>

class DiagnosticSink;

/** @brief Owns a CTF directory and its optional companion Trace Compass XML file. */
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

  /** @brief Returns the CTF backend name. */
  std::string_view backendName() const noexcept override;
  /** @brief Returns the CTF output directory path. */
  std::string targetPath() const override;

protected:
  /** @brief Prepares an empty CTF target and removes stale companion XML. */
  void prepareOutput() override;
  /** @brief Starts the CTF encoder for the prepared target. */
  void startOutput() override;
  /** @brief Completes the bundle and writes XML only for observed views sharing one clock. */
  void stopOutput() override;
  /** @brief Aborts the encoder and removes incomplete CTF and XML targets. */
  void abortOutput() override;
  /**
   * @brief Encodes one selected semantic event.
   * @param event Event evaluated and encoded by the CTF backend.
   */
  void writeOutput(const TraceEvent& event) override;

private:
  /** @brief Finalizes or omits the companion Trace Compass XML for completed CTF metadata. */
  void finalizeTraceCompassXml(const CtfMetadataModel& metadata);
  /** @brief Removes Trace Compass XML and reports incompatible emitted clock domains. */
  void omitTraceCompassXml(std::size_t clockDomainCount);

  std::filesystem::path m_ctfOutputDirectory;
  std::filesystem::path m_traceCompassXmlPath;
  CtfEncoder m_encoder;
  DiagnosticSink* m_diagnostics = nullptr;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_CTFBUNDLEOUTPUT_H
