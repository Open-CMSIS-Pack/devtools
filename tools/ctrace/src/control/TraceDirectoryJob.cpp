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

/** @brief Builds diagnostic context for one trace-run reference. */
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

/** @brief Reports annotations from every reference retained by the trace-run reader. */
static void reportConsumedReferenceDiagnostics(const TraceRunConfig& config, DiagnosticSink& diagnostics)
{
  for (const auto& reference : config.references) {
    const auto report = [&](DiagnosticSink::Severity severity, const std::vector<std::string>& messages) {
      for (const auto& message : messages) {
        if (message.empty()) {
          continue;
        }
        diagnostics.report({
            severity,
            message,
            referenceContext(config, reference),
        });
      }
    };
    report(DiagnosticSink::Severity::Info, reference.info);
    report(DiagnosticSink::Severity::Warning, reference.warning);
    for (const auto& error : reference.error) {
      diagnostics.report({
          DiagnosticSink::Severity::Error,
          error.empty() ? "trace generation setup failed without a diagnostic message" : error,
          referenceContext(config, reference),
          DiagnosticSink::Impact::NonFailing,
      });
    }
  }
}

/** @brief Reports non-fatal inconsistencies ignored while normalizing trace-run metadata. */
static void reportTraceRunWarnings(const CtraceRunMeta& meta, DiagnosticSink& diagnostics)
{
  for (const auto& warning : meta.warnings()) {
    auto context = warning.context;
    context.insert(context.begin(), {"config", meta.configPath()});
    diagnostics.report({
        DiagnosticSink::Severity::Warning,
        warning.message,
        std::move(context),
    });
  }
}

TraceDirectoryJob::TraceDirectoryJob(CliOptions options, DiagnosticSink& diagnostics,
                                     const TraceRunConfigReader& configReader)
  : m_options(std::move(options)),
    m_diagnostics(diagnostics),
    m_configReader(configReader)
{
}

void TraceDirectoryJob::run()
{
  std::vector<std::filesystem::path> configFiles;
  if (m_options.traceDir.has_value()) {
    configFiles = TraceRunDiscovery::selectConfigFiles(*m_options.traceDir, m_options.targetName);
  } else {
    throw std::runtime_error("trace directory job requires <trace-dir>");
  }

  for (const auto& configFile : configFiles) {
    processConfigFile(configFile);
  }
}

void TraceDirectoryJob::processConfigFile(const std::filesystem::path& configFile)
{
  const auto solutionSet = TraceRunDiscovery::solutionSetName(configFile);
  try {
    auto config = m_configReader.read(configFile.string());
    m_diagnostics.report({
        DiagnosticSink::Severity::Info,
        "selected trace-run configuration",
        {
            {"solutionSet", solutionSet},
            {"path", config.path},
            {"references", std::to_string(config.references.size())},
            {"setups", std::to_string(config.setups.size())},
        },
    });
    reportConsumedReferenceDiagnostics(config, m_diagnostics);
    auto input = TraceRunDiscovery::resolveInput(std::move(config), [&](const auto& rawInput) {
      m_diagnostics.report({
          DiagnosticSink::Severity::Warning,
          "skipping raw trace channel excluded from active input selection",
          {
              {"solutionSet", solutionSet},
              {"channel", rawInput.channel},
              {"path", rawInput.path.string()},
          },
      });
    });
    reportTraceRunWarnings(input.metadata(), m_diagnostics);
    FileDecodeJob fileJob(m_options, std::move(input), m_diagnostics);
    fileJob.run();
  } catch (const std::exception& error) {
    m_diagnostics.report({
        DiagnosticSink::Severity::Error,
        error.what(),
        {
            {"solutionSet", solutionSet},
            {"config", configFile.string()},
        },
    });
  }
}
