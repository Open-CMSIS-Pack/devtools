/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.hpp"
#include <gtest/gtest.h>
#include "TraceIssueReporter.hpp"
#include <optional>
#include <string>

TEST(CtraceUnitTests, testDiagnosticCollection)
{
  CollectingDiagnosticSink sink;
  sink.report({
      DiagnosticSink::Severity::Warning,
      DiagnosticSink::Category::Decode,
      "timestamp",
      "timestamp discontinuity",
      {{"offset", "42"}},
  });

  require(sink.events().size() == 1, "diagnostic event count mismatch");
  require(sink.events()[0].severity == DiagnosticSink::Severity::Warning, "diagnostic severity mismatch");
  require(sink.events()[0].category == DiagnosticSink::Category::Decode, "diagnostic category mismatch");
  require(toString(sink.events()[0].severity) == "warning", "diagnostic severity text mismatch");
  require(toString(sink.events()[0].category) == "decode", "diagnostic category text mismatch");
  require(sink.fatalCount() == 0U, "warnings must not be fatal");

  sink.report({
      DiagnosticSink::Severity::Error,
      DiagnosticSink::Category::Input,
      "generator-error",
      "generator could not create an irrelevant register",
      {},
      std::nullopt,
      DiagnosticSink::Impact::NonFatal,
  });
  require(sink.events().back().impact == DiagnosticSink::Impact::NonFatal && sink.fatalCount() == 0U,
          "non-fatal errors must retain error severity without failing the job");

  sink.report({
      DiagnosticSink::Severity::Error,
      DiagnosticSink::Category::Input,
      "required-input-missing",
      "required input is missing",
  });
  require(sink.events().back().impact == DiagnosticSink::Impact::Fatal && sink.fatalCount() == 1U,
          "fatal errors must fail the job");

  sink.clear();
  require(sink.events().empty(), "diagnostic clear failed");
}

TEST(CtraceUnitTests, testTraceIssueReporterReportsEveryIssue)
{
  CollectingDiagnosticSink payloadIndependentDiagnostics;
  TraceIssueReporter payloadIndependentReporter(payloadIndependentDiagnostics);
  TraceEvent payloadIndependentError = issuePacket("opencsd-incomplete-tail");
  payloadIndependentReporter.append(payloadIndependentError);
  require(payloadIndependentDiagnostics.events().size() == 1U,
          "decoder errors must remain visible independently of payload filtering");
  require(payloadIndependentDiagnostics.fatalCount() == 1U,
          "decoder errors must fail validation independently of payload filtering");

  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);

  reporter.append(overflowPacket(10));
  reporter.append(overflowPacket(20));
  reporter.finish();
  reporter.finish();

  TraceEvent dataLoss = issuePacket("data-loss", "trace data was lost");
  dataLoss.index = 12U;
  std::get<TraceIssueEvent>(dataLoss.payload).rawBytesConsumed = 3U;
  reporter.append(dataLoss);
  reporter.append(dataLoss);

  TraceEvent warning = issuePacket("opencsd-warning", "decoder warning", TraceIssueSeverity::Warning);
  reporter.append(warning);

  TraceEvent initializationError = issuePacket("opencsd-initialization-error", "decoder setup failed");
  reporter.append(initializationError);

  require(diagnostics.events().size() == 5U, "TraceIssueReporter should report every error and warning occurrence");
  require(diagnostics.events()[0].code == "overflow", "TraceIssueReporter overflow code mismatch");
  require(diagnostics.events()[0].severity == DiagnosticSink::Severity::Warning,
          "TraceIssueReporter overflow severity mismatch");
  require(diagnostics.events()[1].code == "data-loss", "TraceIssueReporter data-loss code mismatch");
  require(diagnostics.events()[1].severity == DiagnosticSink::Severity::Error,
          "TraceIssueReporter data-loss severity mismatch");
  require(diagnostics.events()[1].context.empty(), "TraceIssueReporter context mismatch");
  require(diagnostics.events()[1].compactMessage.has_value() &&
              diagnostics.events()[1].compactMessage->find("3 raw bytes") != std::string::npos,
          "TraceIssueReporter should provide a compact stderr message");
  require(diagnostics.events()[2].code == "data-loss", "TraceIssueReporter repeated data-loss missing");
  require(diagnostics.events()[3].severity == DiagnosticSink::Severity::Warning,
          "TraceIssueReporter should preserve warning severity");
  require(diagnostics.events()[4].context.empty(), "TraceIssueReporter context mismatch");
  require(diagnostics.fatalCount() == 3U, "TraceIssueReporter should classify decoder errors as fatal");
}
