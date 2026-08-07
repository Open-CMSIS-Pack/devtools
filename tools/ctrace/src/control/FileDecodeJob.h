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
  /**
   * @brief Creates a file decode job using the production OpenCSD session.
   * @param options Validated command-line options.
   * @param rawInputPath Raw SWO input to decode.
   * @param diagnostics Sink receiving operational diagnostics.
   * @param ctraceRunMeta Normalized metadata for decoding and output.
   */
  FileDecodeJob(CliOptions options, std::filesystem::path rawInputPath, DiagnosticSink& diagnostics,
                CtraceRunMeta ctraceRunMeta);
  /**
   * @brief Creates a file decode job with an injected OpenCSD session factory.
   * @param options Validated command-line options.
   * @param rawInputPath Raw SWO input to decode.
   * @param diagnostics Sink receiving operational diagnostics.
   * @param ctraceRunMeta Normalized metadata for decoding and output.
   * @param sessionFactory Factory used to create the decoder session.
   */
  FileDecodeJob(CliOptions options, std::filesystem::path rawInputPath, DiagnosticSink& diagnostics,
                CtraceRunMeta ctraceRunMeta, OpenCsdItmSessionFactory sessionFactory);

  /**
   * @brief Runs decoding, reporting, and output completion for the input file.
   *
   * Recoverable trace corruption is reported and decoding resumes at hardware
   * synchronization. Setup, input, and output failures are reported through the
   * diagnostic sink.
   */
  void run();

private:
  CliOptions m_options;
  std::filesystem::path m_rawInputPath;
  DiagnosticSink& m_diagnostics;
  CtraceRunMeta m_ctraceRunMeta;
  OpenCsdItmSessionFactory m_sessionFactory;
};

#endif  // CTRACE_SRC_CONTROL_FILEDECODEJOB_H
