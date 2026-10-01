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

/** @brief Owns the lifecycle of one independently completed CTF directory. */
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
  /** @brief Returns emitted metadata after a successful stop, or nullptr otherwise. */
  const CtfMetadataModel* completedMetadata() const noexcept;

protected:
  /** @brief Prepares an empty CTF target. */
  void prepareOutput() override;
  /** @brief Starts the CTF encoder for the prepared target. */
  void startOutput() override;
  /** @brief Completes the CTF bundle and publishes its emitted metadata. */
  void stopOutput() override;
  /** @brief Aborts the encoder and removes the incomplete CTF target. */
  void abortOutput() override;
  /**
   * @brief Encodes one selected semantic event.
   * @param event Event evaluated and encoded by the CTF backend.
   */
  void writeOutput(const TraceEvent& event) override;

private:
  std::filesystem::path m_ctfOutputDirectory;
  CtfEncoder m_encoder;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_CTFBUNDLEOUTPUT_H
