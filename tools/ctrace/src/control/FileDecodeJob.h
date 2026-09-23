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
#include "TraceRunDiscovery.h"
#include "ctf/CtfMetadataModel.h"

#include <optional>

/** @brief Decodes one raw trace file and owns its configured output lifecycle. */
class FileDecodeJob {
public:
  /**
   * @brief Creates a file decode job using the production OpenCSD session.
   * @param options Validated command-line options.
   * @param input Selected and preflighted raw input with normalized metadata.
   * @param diagnostics Sink receiving operational diagnostics.
   */
  FileDecodeJob(CliOptions options, TraceRunInputDescriptor input, DiagnosticSink& diagnostics);
  /**
   * @brief Creates a file decode job with an injected OpenCSD session factory.
   * @param options Validated command-line options.
   * @param input Selected and preflighted raw input with normalized metadata.
   * @param diagnostics Sink receiving operational diagnostics.
   * @param sessionFactory Factory used to create the decoder session.
   */
  FileDecodeJob(CliOptions options, TraceRunInputDescriptor input, DiagnosticSink& diagnostics,
                OpenCsdItmSessionFactory sessionFactory);

  /**
   * @brief Runs decoding, reporting, and output completion for the input file.
   *
   * Recoverable trace corruption is reported and decoding resumes at hardware
   * synchronization. Setup, input, and output failures are reported through the
   * diagnostic sink.
   * @return Completed CTF metadata, if that backend finished successfully.
   */
  std::optional<CtfMetadataModel> run();

private:
  CliOptions m_options;
  TraceRunInputDescriptor m_input;
  DiagnosticSink& m_diagnostics;
  OpenCsdItmSessionFactory m_sessionFactory;
};

#endif  // CTRACE_SRC_CONTROL_FILEDECODEJOB_H
