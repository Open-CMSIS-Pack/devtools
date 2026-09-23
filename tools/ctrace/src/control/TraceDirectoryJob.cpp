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
#include "ctf/TraceCompassXmlOutput.h"

#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/** @brief Adds raw-input identity to every diagnostic without changing its severity or failure impact. */
class InputDiagnosticSink final : public DiagnosticSink {
public:
  /** @brief Binds diagnostics to one independently processed capture. */
  InputDiagnosticSink(DiagnosticSink& target, const TraceRunRawInput& input)
    : m_target(target),
      m_context{{"inputChannel", input.channel}, {"input", input.path.string()}}
  {
  }

protected:
  /** @brief Forwards the original diagnostic with the capture context prepended. */
  void write(const Event& event) override
  {
    auto contextual = event;
    contextual.context.insert(contextual.context.begin(), m_context.begin(), m_context.end());
    m_target.report(contextual);
  }

private:
  DiagnosticSink& m_target;
  std::vector<std::pair<std::string, std::string>> m_context;
};

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

/** @brief Reports a target-level XML failure without invalidating completed capture outputs. */
static void reportXmlFailure(DiagnosticSink& diagnostics, const std::filesystem::path& path,
                              const std::exception& error)
{
  diagnostics.report({DiagnosticSink::Severity::Error, error.what(),
                       {{"backend", "trace-compass"}, {"path", path.string()}}});
}

/** @brief Prepares the shared XML independently of all per-capture output backends. */
static std::unique_ptr<TraceCompassXmlOutput> prepareTraceCompassXml(OutputFormat format,
                                                                    const std::filesystem::path& path,
                                                                    DiagnosticSink& diagnostics)
{
  if (format != OutputFormat::Ctf && format != OutputFormat::All) {
    return nullptr;
  }
  try {
    auto xml = std::make_unique<TraceCompassXmlOutput>(path, diagnostics);
    xml->prepare();
    return xml;
  } catch (const std::exception& error) {
    reportXmlFailure(diagnostics, path, error);
    return nullptr;
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
    const auto inputs = TraceRunDiscovery::selectInputs(config, [&](const auto& rawInput) {
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
    processInputs(config, inputs, configFile.parent_path() / (solutionSet + ".traceanalysis.xml"));
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

void TraceDirectoryJob::processInputs(const TraceRunConfig& config, const std::vector<TraceRunRawInput>& inputs,
                                       const std::filesystem::path& xmlPath)
{
  auto xml = prepareTraceCompassXml(m_options.outputFormat, xmlPath, m_diagnostics);
  std::vector<std::pair<std::string, CtfMetadataModel>> completed;
  for (const auto& rawInput : inputs) {
    auto metadata = processInput(config, rawInput);
    if (xml != nullptr && metadata.has_value()) {
      completed.emplace_back(rawInput.channel, std::move(*metadata));
    }
  }
  if (xml != nullptr) {
    try {
      for (const auto& [channel, metadata] : completed) {
        xml->add(channel, metadata);
      }
      xml->finish();
    } catch (const std::exception& error) {
      reportXmlFailure(m_diagnostics, xmlPath, error);
    }
  }
}

std::optional<CtfMetadataModel> TraceDirectoryJob::processInput(const TraceRunConfig& config,
                                                               const TraceRunRawInput& rawInput)
{
  InputDiagnosticSink diagnostics(m_diagnostics, rawInput);
  try {
    auto input = TraceRunDiscovery::resolveInput(config, rawInput);
    reportTraceRunWarnings(input.metadata(), diagnostics);
    FileDecodeJob fileJob(m_options, std::move(input), diagnostics);
    return fileJob.run();
  } catch (const std::exception& error) {
    diagnostics.report({DiagnosticSink::Severity::Error, error.what(), {{"config", config.path}}});
  }
  return std::nullopt;
}
