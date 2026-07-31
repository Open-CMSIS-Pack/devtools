/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtraceMain.hpp"

#include "CliOptions.hpp"
#include "CliParser.hpp"
#include "DiagnosticSink.hpp"
#include "TraceDirectoryJob.hpp"
#include "YmlTraceRunConfigReader.hpp"

#include <exception>
#include <iostream>

namespace {

void reportFailure(DiagnosticSink& diagnostics, DiagnosticSink::Category category, const char* code,
                   const std::exception& error)
{
  diagnostics.report({
      DiagnosticSink::Severity::Error,
      category,
      code,
      error.what(),
  });
}

} // namespace

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
    CliParser::validate(options);
    if (options.version) {
      std::cout << CliParser::versionString() << "\n";
      return 0;
    }
  } catch (const std::exception& error) { // LCOV_EXCL_BR_LINE: exception dispatch is validated by CLI tests
    reportFailure(diagnostics, DiagnosticSink::Category::Cli, "invalid-arguments", error);
    return 1;
  }

  try {
    YmlTraceRunConfigReader configReader;
    TraceDirectoryJob job(options, diagnostics, configReader);
    job.run();
    return diagnostics.fatalCount() == 0U ? 0 : 1;
  } catch (const std::exception& error) { // LCOV_EXCL_BR_LINE: exception dispatch is validated by job tests
    reportFailure(diagnostics, DiagnosticSink::Category::Input, "trace-directory-failed", error);
    return 1;
  }
}
