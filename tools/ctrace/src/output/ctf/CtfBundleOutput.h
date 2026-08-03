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

class CtfBundleOutput final : public TraceOutput {
public:
  explicit CtfBundleOutput(CtfOutputConfig config, DiagnosticSink* diagnostics = nullptr);
  ~CtfBundleOutput() override;

  void start() override;
  void stop() override;
  void abort() override;
  void writeEvent(const TraceEvent& event) override;
  std::string_view backendName() const noexcept override;
  std::string targetPath() const override;

private:
  std::filesystem::path ctfOutputDirectory_;
  std::filesystem::path traceCompassXmlPath_;
  CtfEncoder encoder_;
  bool active_ = false;
};

#endif  // CTRACE_SRC_OUTPUT_CTF_CTFBUNDLEOUTPUT_H
