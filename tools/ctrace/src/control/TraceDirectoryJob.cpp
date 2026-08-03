/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceDirectoryJob.h"

#include "CliOptions.h"
#include "DiagnosticSink.h"
#include "FileDecodeJob.h"
#include "TraceRunConfig.h"
#include "TraceRunConfigReader.h"
#include "TraceRunDiscovery.h"
#include "CtraceRunMeta.h"

#include <exception>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

static std::vector<std::pair<std::string, std::string>> referenceContext(const TraceRunConfig& config,
                                                                         const TraceRunReference& reference)
{
  std::vector<std::pair<std::string, std::string>> context{
      {"config", config.path},
      {"ctraceRef", reference.ctraceRef},
      {"type", reference.type},
  };
  if (reference.processorName.has_value()) {
    context.emplace_back("pname", *reference.processorName);
  }
  if (reference.stream.has_value()) {
    context.emplace_back("stream", std::to_string(*reference.stream));
  }
  return context;
}

static void reportTraceRunDiagnostics(const TraceRunConfig& config, DiagnosticSink& diagnostics)
{
  for (const auto& reference : config.references) {
    if (TraceRunSchema::isItmChannelZero(reference)) {
      continue;
    }
    const auto report = [&](DiagnosticSink::Severity severity, const std::string& code,
                            const std::optional<std::string>& message) {
      if (!message.has_value() || message->empty()) {
        return;
      }
      diagnostics.report({
          severity,
          DiagnosticSink::Category::Input,
          code,
          *message,
          referenceContext(config, reference),
      });
    };
    report(DiagnosticSink::Severity::Info, "trace-run-generation-info", reference.info);
    report(DiagnosticSink::Severity::Warning, "trace-run-generation-warning", reference.warning);
    if (reference.error.has_value()) {
      auto context = referenceContext(config, reference);
      diagnostics.report({
          DiagnosticSink::Severity::Error,
          DiagnosticSink::Category::Input,
          "trace-run-generation-error",
          reference.error->empty() ? "trace generation setup failed without a diagnostic message" : *reference.error,
          std::move(context),
          std::nullopt,
          DiagnosticSink::Impact::NonFailing,
      });
    }
  }
}

TraceDirectoryJob::TraceDirectoryJob(CliOptions options, DiagnosticSink& diagnostics,
                                     const TraceRunConfigReader& configReader)
  : options_(std::move(options)), diagnostics_(diagnostics), configReader_(configReader)
{
}

void TraceDirectoryJob::run()
{
  std::vector<std::filesystem::path> configFiles;
  if (options_.traceDir.has_value()) {
    configFiles = TraceRunDiscovery::selectConfigFiles(*options_.traceDir, options_.targetName);
  } else {
    throw std::runtime_error("trace directory job requires <trace-dir>");
  }

  for (const auto& configFile : configFiles) {
    const auto solutionSet = TraceRunDiscovery::solutionSetName(configFile);
    try {
      const auto config = configReader_.read(configFile.string());
      diagnostics_.report({
          DiagnosticSink::Severity::Info,
          DiagnosticSink::Category::Input,
          "trace-run-config",
          "selected trace-run configuration",
          {
              {"solutionSet", solutionSet},
              {"path", config.path},
              {"references", std::to_string(config.references.size())},
              {"setups", std::to_string(config.setups.size())},
          },
      });
      reportTraceRunDiagnostics(config, diagnostics_);
      const auto ctraceRunMeta = CtraceRunMeta::fromConfig(config);
      const auto rawInputs = TraceRunDiscovery::rawInputs(configFile);
      bool processedSolutionSet = false;
      for (const auto& rawInput : rawInputs) {
        if (rawInput.channel != "SWO") {
          diagnostics_.report({
              DiagnosticSink::Severity::Warning,
              DiagnosticSink::Category::Input,
              "unsupported-trace-channel",
              "skipping raw trace channel that is not implemented yet",
              {
                  {"solutionSet", solutionSet},
                  {"channel", rawInput.channel},
                  {"path", rawInput.path.string()},
              },
          });
          continue;
        }

        FileDecodeJob fileJob(options_, rawInput.path, diagnostics_, ctraceRunMeta);
        fileJob.run();
        processedSolutionSet = true;
      }
      if (!processedSolutionSet) {
        diagnostics_.report({
            DiagnosticSink::Severity::Error,
            DiagnosticSink::Category::Input,
            "missing-swo-raw-input",
            "no supported <solution-set>.SWO.raw input found",
            {
                {"solutionSet", solutionSet},
                {"traceDir", configFile.parent_path().string()},
            },
        });
      }
    } catch (const std::exception& error) {
      diagnostics_.report({
          DiagnosticSink::Severity::Error,
          DiagnosticSink::Category::Input,
          "solution-set-failed",
          error.what(),
          {
              {"solutionSet", solutionSet},
              {"config", configFile.string()},
          },
      });
    }
  }
}
