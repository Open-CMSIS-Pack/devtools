/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "CliOptions.hpp"
#include "DiagnosticSink.hpp"
#include "OpenCsdItmDecoder.hpp"
#include "CtraceRunMeta.hpp"

#include <filesystem>

class FileDecodeJob {
public:
  FileDecodeJob(CliOptions options, std::filesystem::path rawInputPath, DiagnosticSink& diagnostics,
                CtraceRunMeta ctraceRunMeta);
  FileDecodeJob(CliOptions options, std::filesystem::path rawInputPath, DiagnosticSink& diagnostics,
                CtraceRunMeta ctraceRunMeta, OpenCsdItmSessionFactory sessionFactory);

  void run();

private:
  CliOptions options_;
  std::filesystem::path rawInputPath_;
  DiagnosticSink& diagnostics_;
  CtraceRunMeta ctraceRunMeta_;
  OpenCsdItmSessionFactory sessionFactory_;
};
