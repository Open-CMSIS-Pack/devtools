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

/** @brief Decodes one raw trace file and owns its configured output lifecycle. */
class FileDecodeJob {
public:
  /** @brief Creates a file decode job using the production OpenCSD session. */
  FileDecodeJob(CliOptions options, std::filesystem::path rawInputPath, DiagnosticSink& diagnostics,
                CtraceRunMeta ctraceRunMeta);
  /** @brief Creates a file decode job with an injected OpenCSD session factory. */
  FileDecodeJob(CliOptions options, std::filesystem::path rawInputPath, DiagnosticSink& diagnostics,
                CtraceRunMeta ctraceRunMeta, OpenCsdItmSessionFactory sessionFactory);

  /** @brief Runs decoding, reporting, and output completion for the input file. */
  void run();

private:
  CliOptions options_;
  std::filesystem::path rawInputPath_;
  DiagnosticSink& diagnostics_;
  CtraceRunMeta ctraceRunMeta_;
  OpenCsdItmSessionFactory sessionFactory_;
};

#endif  // CTRACE_SRC_CONTROL_FILEDECODEJOB_H
