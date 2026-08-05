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
#include <fstream>
#include <iomanip>
#include <ios>
#include <memory>
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

  /** @brief Opens a raw trace input for binary reading. */
  explicit RawFileReader(std::filesystem::path path)
    : m_path(std::move(path)),
      m_stream(m_path, std::ios::binary),
      m_buffer(64U * 1024U)
  {
    if (!m_stream) {
      throw std::runtime_error("failed to open input file: " + m_path.string());
    }
  }

  /** @brief Returns the next raw byte chunk. */
  ReadResult read()
  {
    if (m_eof || !m_stream.is_open()) {
      return {{}, true};
    }

    m_stream.read(reinterpret_cast<char*>(m_buffer.data()), static_cast<std::streamsize>(m_buffer.size()));
    const auto readBytes = m_stream.gcount();
    if (readBytes > 0) {
      if (m_stream.eof()) {
        m_eof = true;
        m_stream.close();
      }
      return {{m_buffer.data(), static_cast<std::size_t>(readBytes)}, false};
    }
    if (m_stream.bad()) {
      throw std::runtime_error("failed to read input file: " + m_path.string());
    }

    m_eof = true;
    m_stream.close();
    return {{}, true};
  }

private:
  std::filesystem::path m_path;
  std::ifstream m_stream;
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

/** @brief Extracts fallback and per-stream timestamp prescalers from metadata. */
static ItmTimestampPrescalers timestampPrescalers(const CtraceRunMeta& ctraceRunMeta)
{
  auto fallback = ctraceRunMeta.timestampPrescaler();
  if (!fallback.has_value() && !ctraceRunMeta.hasDistinctProcessorPrescalers()) {
    fallback = TraceRunSchema::kDefaultTimestampPrescaler;
  }
  return {fallback, ctraceRunMeta.timestampPrescalersByTraceBusId()};
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

FileDecodeJob::FileDecodeJob(CliOptions options, std::filesystem::path rawInputPath, DiagnosticSink& diagnostics,
                             CtraceRunMeta ctraceRunMeta)
  : m_options(std::move(options)),
    m_rawInputPath(std::move(rawInputPath)),
    m_diagnostics(diagnostics),
    m_ctraceRunMeta(std::move(ctraceRunMeta))
{
}

FileDecodeJob::FileDecodeJob(CliOptions options, std::filesystem::path rawInputPath, DiagnosticSink& diagnostics,
                             CtraceRunMeta ctraceRunMeta, OpenCsdItmSessionFactory sessionFactory)
  : m_options(std::move(options)),
    m_rawInputPath(std::move(rawInputPath)),
    m_diagnostics(diagnostics),
    m_ctraceRunMeta(std::move(ctraceRunMeta)),
    m_sessionFactory(std::move(sessionFactory))
{
}

void FileDecodeJob::run()
{
  const auto prescalers = timestampPrescalers(m_ctraceRunMeta);
  auto outputPlan = planTraceOutputs(outputRequest(m_options), m_rawInputPath, m_ctraceRunMeta, m_diagnostics);
  if (outputPlan.hasRequestedOutputs() && !outputPlan.hasEnabledOutputs()) {
    return;
  }
  m_diagnostics.report({
      DiagnosticSink::Severity::Info,
      "applied ctrace-run meta",
      {
          {"path", m_ctraceRunMeta.configPath()},
          {"processors", std::to_string(m_ctraceRunMeta.processorCount())},
          {"sources", std::to_string(m_ctraceRunMeta.sources().size())},
      },
  });
  auto outputs = createConfiguredOutputs(outputPlan, m_diagnostics);
  DecodeConsumers consumers(std::move(outputs), m_diagnostics, m_ctraceRunMeta.itmEnableMask(),
                            m_ctraceRunMeta.itmEnableMasksByTraceBusId());

  if (prescalers.fallback.has_value()) {
    m_diagnostics.report({
        DiagnosticSink::Severity::Info,
        "using timestamp prescaler",
        {{"value", std::to_string(*prescalers.fallback)}},
    });
  } else {
    m_diagnostics.report({
        DiagnosticSink::Severity::Info,
        "using Trace-Bus-ID-specific timestamp prescalers",
        {{"traceBusIds", std::to_string(prescalers.byTraceBusId.size())}},
    });
  }
  const auto decodeStart = std::chrono::steady_clock::now();
  DecodeResult decode;
  bool decoderFatal = false;
  try {
    RawFileReader input(m_rawInputPath);
    std::unique_ptr<DecodePipeline> pipeline;
    if (m_sessionFactory) {
      pipeline = std::make_unique<DecodePipeline>(prescalers, consumers, m_sessionFactory);
    } else {
      pipeline = std::make_unique<DecodePipeline>(prescalers, consumers);
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
