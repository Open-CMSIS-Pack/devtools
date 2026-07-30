/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "CtfEncoder.hpp"
#include "TraceEvent.hpp"
#include "TraceOutput.hpp"
#include "TraceOutputConfig.hpp"

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
