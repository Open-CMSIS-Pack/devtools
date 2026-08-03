/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_CONTROL_FILEDECODEJOB_H
#define CTRACE_SRC_CONTROL_FILEDECODEJOB_H

#include "CliOptions.h"
#include "DiagnosticSink.h"
#include "OpenCsdItmDecoder.h"
#include "CtraceRunMeta.h"

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

#endif  // CTRACE_SRC_CONTROL_FILEDECODEJOB_H
