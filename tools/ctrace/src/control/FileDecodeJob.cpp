/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "FileDecodeJob.h"

#include "CliOptions.h"
#include "csv/CsvFileOutput.h"
#include "ctf/CtfBundleOutput.h"
#include "CortexMStreamDecoder.h"
#include "DecodeConsumers.h"
#include "DecodePipeline.h"
#include "DiagnosticSink.h"
#include "OpenCsdItmDecoder.h"
#include "OutputRequirements.h"
#include "TraceOutput.h"
#include "TraceOutputConfig.h"
#include "TraceRunConfig.h"
#include "CtraceRunMeta.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <ios>
#include <memory>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/** @brief Reads a raw trace file in reusable fixed-size chunks. */
class RawFileReader final {
public:
  /** @brief Stores one byte chunk and its end-of-file state. */
  struct ReadResult {
    RawByteView bytes;
    bool eof = false;
  };

  /** @brief Reads one already opened and preflighted raw trace input. */
  RawFileReader(std::filesystem::path path, std::istream& stream)
    : m_path(std::move(path)),
      m_stream(stream),
      m_buffer(64U * 1024U)
  {
  }

  /** @brief Returns the next raw byte chunk. */
  ReadResult read()
  {
    if (m_eof) {
      return {{}, true};
    }

    m_stream.read(reinterpret_cast<char*>(m_buffer.data()), static_cast<std::streamsize>(m_buffer.size()));
    const auto readBytes = m_stream.gcount();
    if (readBytes > 0) {
      if (m_stream.eof()) {
        m_eof = true;
      }
      return {{m_buffer.data(), static_cast<std::size_t>(readBytes)}, false};
    }
    if (m_stream.bad()) {
      throw std::runtime_error("failed to read input file: " + m_path.string());
    }
    m_eof = true;
    return {{}, true};
  }

private:
  std::filesystem::path m_path;
  std::istream& m_stream;
  std::vector<std::uint8_t> m_buffer;
  bool m_eof = false;
};

/** @brief Formats event count, input size, elapsed time, and throughput. */
static std::string decodeSummary(const DecodeResult& decode, std::chrono::steady_clock::duration elapsed)
{
  const auto seconds = std::chrono::duration<double>(elapsed).count();
  const auto mebibytes = static_cast<double>(decode.bytesIn) / (1024.0 * 1024.0);
  const auto mebibytesPerSecond = seconds > 0.0 ? mebibytes / seconds : 0.0;

  std::ostringstream out;
  out << "decoded " << decode.eventsOut << " events from " << decode.bytesIn << " bytes in " << std::fixed
      << std::setprecision(3) << seconds << " s (" << std::setprecision(2) << mebibytesPerSecond << " MiB/s)";
  return out.str();
}

/** @brief Converts normalized trace-run routes into semantic decoder routes. */
static std::vector<CortexMDecodeRoute> decodeRoutes(const CtraceRunMeta& ctraceRunMeta)
{
  std::vector<CortexMDecodeRoute> result;
  result.reserve(ctraceRunMeta.routes().size());
  for (const auto& route : ctraceRunMeta.routes()) {
    result.push_back({route.identity, route.timestampPrescaler});
  }
  return result;
}

/** @brief Maps the preflighted raw-input contract to the decode frontend. */
static OpenCsdItmInputMode decodeInputMode(const TraceRunInputDescriptor& input)
{
  return input.format() == TraceRunFormat::Formatted ? OpenCsdItmInputMode::CoreSightFormatted
                                                     : OpenCsdItmInputMode::Single;
}

/** @brief Indexes route-local ITM enable masks without using a transport sentinel. */
static std::map<TraceRouteId, std::uint32_t> itmEnableMasks(const CtraceRunMeta& ctraceRunMeta)
{
  std::map<TraceRouteId, std::uint32_t> result;
  for (const auto& route : ctraceRunMeta.routes()) {
    if (route.itmEnableMask.has_value()) {
      result.emplace(route.identity.id, *route.itmEnableMask);
    }
  }
  return result;
}

/** @brief Counts source metadata directly from the canonical route catalogue. */
static std::size_t sourceCount(const CtraceRunMeta& ctraceRunMeta)
{
  std::size_t result = 0U;
  for (const auto& route : ctraceRunMeta.routes()) {
    result += route.sources.size();
  }
  return result;
}

/** @brief Converts command-line output selection into an output request. */
static TraceOutputRequest outputRequest(const CliOptions& options)
{
  return {
      options.outputFormat == OutputFormat::Csv || options.outputFormat == OutputFormat::All,
      options.outputFormat == OutputFormat::Ctf || options.outputFormat == OutputFormat::All,
      options.selection,
  };
}

/** @brief Creates the output backends enabled by a validated plan. */
static std::vector<std::unique_ptr<TraceOutput>> createConfiguredOutputs(const TraceOutputPlan& outputPlan,
                                                                         DiagnosticSink& diagnostics)
{
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  if (outputPlan.ctf.has_value()) {
    outputs.push_back(std::make_unique<CtfBundleOutput>(*outputPlan.ctf, &diagnostics));
    diagnostics.report({
        DiagnosticSink::Severity::Info,
        "configured Trace Compass XML",
        {{"path", outputPlan.ctf->traceCompassXmlPath.string()}},
    });
  }
  if (outputPlan.csv.has_value()) {
    outputs.push_back(std::make_unique<CsvFileOutput>(outputPlan.csv->outputPath, outputPlan.csv->selection));
  }
  return outputs;
}

FileDecodeJob::FileDecodeJob(CliOptions options, TraceRunInputDescriptor input, DiagnosticSink& diagnostics)
  : m_options(std::move(options)),
    m_input(std::move(input)),
    m_diagnostics(diagnostics)
{
}

FileDecodeJob::FileDecodeJob(CliOptions options, TraceRunInputDescriptor input, DiagnosticSink& diagnostics,
                             OpenCsdItmSessionFactory sessionFactory)
  : m_options(std::move(options)),
    m_input(std::move(input)),
    m_diagnostics(diagnostics),
    m_sessionFactory(std::move(sessionFactory))
{
}

void FileDecodeJob::run()
{
  const auto& ctraceRunMeta = m_input.metadata();
  const auto routes = decodeRoutes(ctraceRunMeta);
  const auto inputMode = decodeInputMode(m_input);
  auto outputPlan = planTraceOutputs(outputRequest(m_options), m_input.path(), ctraceRunMeta, m_diagnostics);
  if (outputPlan.hasRequestedOutputs() && !outputPlan.hasEnabledOutputs()) {
    return;
  }
  m_diagnostics.report({
      DiagnosticSink::Severity::Info,
      "applied ctrace-run meta",
      {
          {"path", ctraceRunMeta.configPath()},
          {"routes", std::to_string(ctraceRunMeta.routes().size())},
          {"sources", std::to_string(sourceCount(ctraceRunMeta))},
      },
  });
  auto outputs = createConfiguredOutputs(outputPlan, m_diagnostics);
  DecodeConsumers consumers(std::move(outputs), m_diagnostics, itmEnableMasks(ctraceRunMeta));

  for (const auto& route : ctraceRunMeta.routes()) {
    std::vector<std::pair<std::string, std::string>> context;
    context.emplace_back("value", std::to_string(route.timestampPrescaler));
    if (route.identity.traceBusId.has_value()) {
      context.emplace_back("stream", std::to_string(*route.identity.traceBusId));
    }
    if (route.processorName.has_value()) {
      context.emplace_back("pname", *route.processorName);
    }
    m_diagnostics.report({
        DiagnosticSink::Severity::Info,
        "using timestamp prescaler",
        std::move(context),
    });
  }
  const auto decodeStart = std::chrono::steady_clock::now();
  DecodeResult decode;
  bool decoderFatal = false;
  try {
    RawFileReader input(m_input.path(), m_input.stream());
    std::unique_ptr<DecodePipeline> pipeline;
    if (m_sessionFactory) {
      pipeline = std::make_unique<DecodePipeline>(routes, inputMode, consumers, m_sessionFactory);
    } else {
      pipeline = std::make_unique<DecodePipeline>(routes, inputMode, consumers,
                                                  [&](std::uint8_t traceBusId, std::uint64_t sourceOffset) {
                                                    m_diagnostics.report({
                                                        DiagnosticSink::Severity::Warning,
                                                        "skipping unsupported formatted CoreSight trace source",
                                                        {
                                                            {"stream", std::to_string(traceBusId)},
                                                            {"rawOffset", std::to_string(sourceOffset)},
                                                        },
                                                    });
                                                  });
    }
    while (true) {
      const auto read = input.read();
      if (read.eof) {
        break;
      }
      pipeline->push(read.bytes);
    }
    decode = pipeline->finish();
  } catch (const OpenCsdFatalError& error) {
    decoderFatal = true;
    decode.bytesIn = error.bytesProcessed();
    decode.eventsOut = consumers.eventCount();
  }
  consumers.finishIssues();
  const auto decodeEnd = std::chrono::steady_clock::now();
  m_diagnostics.report({
      DiagnosticSink::Severity::Info,
      decodeSummary(decode, decodeEnd - decodeStart),
  });
  if (decoderFatal) {
    consumers.abortOutputs();
  } else {
    consumers.finishOutputs();
  }
}
