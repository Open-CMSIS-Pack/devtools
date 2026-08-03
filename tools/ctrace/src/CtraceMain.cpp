/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtraceMain.h"

#include "CliOptions.h"
#include "CliParser.h"
#include "DiagnosticSink.h"
#include "TraceDirectoryJob.h"
#include "YmlTraceRunConfigReader.h"

#include <exception>
#include <iostream>

static void reportFailure(DiagnosticSink& diagnostics, DiagnosticSink::Category category, const char* code,
                          const std::exception& error)
{
  diagnostics.report({
      DiagnosticSink::Severity::Error,
      category,
      code,
      error.what(),
  });
}

int CtraceMain(int argc, const char* const argv[])
{
  StderrDiagnosticSink diagnostics;
  CliOptions options;
  try {
    if (argc <= 1) {
      std::cout << CliParser::helpString();
      return 0;
    }
    options = CliParser::parse(argc, argv);
    if (options.help) {
      std::cout << CliParser::helpString();
      return 0;
    }
    CliParser::validate(options);
    if (options.version) {
      std::cout << CliParser::versionString() << "\n";
      return 0;
    }
  } catch (const std::exception& error) {
    reportFailure(diagnostics, DiagnosticSink::Category::Cli, "invalid-arguments", error);
    return 1;
  }

  try {
    YmlTraceRunConfigReader configReader;
    TraceDirectoryJob job(options, diagnostics, configReader);
    job.run();
    return diagnostics.failureCount() == 0U ? 0 : 1;
  } catch (const std::exception& error) {
    reportFailure(diagnostics, DiagnosticSink::Category::Input, "trace-directory-failed", error);
    return 1;
  }
}
