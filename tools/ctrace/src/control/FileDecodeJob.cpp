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

class RawFileReader final {
public:
  struct ReadResult {
    RawByteView bytes;
    bool eof = false;
  };

  explicit RawFileReader(std::filesystem::path path)
    : path_(std::move(path)), stream_(path_, std::ios::binary), buffer_(64U * 1024U)
  {
    if (!stream_) {
      throw std::runtime_error("failed to open input file: " + path_.string());
    }
  }

  ReadResult read()
  {
    if (eof_ || !stream_.is_open()) {
      return {{}, true};
    }

    stream_.read(reinterpret_cast<char*>(buffer_.data()), static_cast<std::streamsize>(buffer_.size()));
    const auto readBytes = stream_.gcount();
    if (readBytes > 0) {
      if (stream_.eof()) {
        eof_ = true;
        stream_.close();
      }
      return {{buffer_.data(), static_cast<std::size_t>(readBytes)}, false};
    }
    if (stream_.bad()) {
      throw std::runtime_error("failed to read input file: " + path_.string());
    }

    eof_ = true;
    stream_.close();
    return {{}, true};
  }

private:
  std::filesystem::path path_;
  std::ifstream stream_;
  std::vector<std::uint8_t> buffer_;
  bool eof_ = false;
};

static std::string decodeSummary(const DecodeResult& decode, std::chrono::steady_clock::duration elapsed)
{
  const auto seconds = std::chrono::duration<double>(elapsed).count();
  const auto mebibytes = static_cast<double>(decode.bytesIn) / (1024.0 * 1024.0);
  const auto mebibytesPerSecond = seconds > 0.0 ? mebibytes / seconds : 0.0;

  std::ostringstream out;
  out << "decoded " << decode.packetsOut << " packets from " << decode.bytesIn << " bytes in " << std::fixed
      << std::setprecision(3) << seconds << " s (" << std::setprecision(2) << mebibytesPerSecond << " MiB/s)";
  return out.str();
}

static ItmTimestampPrescalers timestampPrescalers(const CtraceRunMeta& ctraceRunMeta)
{
  auto fallback = ctraceRunMeta.timestampPrescaler();
  if (!fallback.has_value() && !ctraceRunMeta.hasDistinctProcessorPrescalers()) {
    fallback = TraceRunSchema::kDefaultTimestampPrescaler;
  }
  return {fallback, ctraceRunMeta.timestampPrescalersByTraceBusId()};
}

static TraceOutputRequest outputRequest(const CliOptions& options)
{
  return {
      options.outputFormat == OutputFormat::Csv || options.outputFormat == OutputFormat::All,
      options.outputFormat == OutputFormat::Ctf || options.outputFormat == OutputFormat::All,
      options.selection,
  };
}

static std::vector<std::unique_ptr<TraceOutput>> createConfiguredOutputs(const TraceOutputPlan& outputPlan,
                                                                         DiagnosticSink& diagnostics)
{
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  if (outputPlan.ctf.has_value()) {
    outputs.push_back(std::make_unique<CtfBundleOutput>(*outputPlan.ctf, &diagnostics));
    diagnostics.report({
        DiagnosticSink::Severity::Info,
        DiagnosticSink::Category::Output,
        "trace-compass-xml",
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
  : options_(std::move(options)), rawInputPath_(std::move(rawInputPath)), diagnostics_(diagnostics),
    ctraceRunMeta_(std::move(ctraceRunMeta))
{
}

FileDecodeJob::FileDecodeJob(CliOptions options, std::filesystem::path rawInputPath, DiagnosticSink& diagnostics,
                             CtraceRunMeta ctraceRunMeta, OpenCsdItmSessionFactory sessionFactory)
  : options_(std::move(options)), rawInputPath_(std::move(rawInputPath)), diagnostics_(diagnostics),
    ctraceRunMeta_(std::move(ctraceRunMeta)), sessionFactory_(std::move(sessionFactory))
{
}

void FileDecodeJob::run()
{
  const auto prescalers = timestampPrescalers(ctraceRunMeta_);
  auto outputPlan = planTraceOutputs(outputRequest(options_), rawInputPath_, ctraceRunMeta_, diagnostics_);
  if (outputPlan.hasRequestedOutputs() && !outputPlan.hasEnabledOutputs()) {
    return;
  }
  diagnostics_.report({
      DiagnosticSink::Severity::Info,
      DiagnosticSink::Category::Input,
      "ctrace-run-meta",
      "applied ctrace-run meta",
      {
          {"path", ctraceRunMeta_.configPath()},
          {"processors", std::to_string(ctraceRunMeta_.processorCount())},
          {"sources", std::to_string(ctraceRunMeta_.sources().size())},
      },
  });
  auto outputs = createConfiguredOutputs(outputPlan, diagnostics_);
  DecodeConsumers consumers(std::move(outputs), diagnostics_, ctraceRunMeta_.itmEnableMask(),
                            ctraceRunMeta_.itmEnableMasksByTraceBusId());

  if (prescalers.fallback.has_value()) {
    diagnostics_.report({
        DiagnosticSink::Severity::Info,
        DiagnosticSink::Category::Input,
        "timestamp-prescaler",
        "using timestamp prescaler",
        {{"value", std::to_string(*prescalers.fallback)}},
    });
  } else {
    diagnostics_.report({
        DiagnosticSink::Severity::Info,
        DiagnosticSink::Category::Input,
        "timestamp-prescaler",
        "using Trace-Bus-ID-specific timestamp prescalers",
        {{"traceBusIds", std::to_string(prescalers.byTraceBusId.size())}},
    });
  }
  const auto decodeStart = std::chrono::steady_clock::now();
  DecodeResult decode;
  bool decoderFatal = false;
  try {
    RawFileReader input(rawInputPath_);
    std::unique_ptr<DecodePipeline> pipeline;
    if (sessionFactory_) {
      pipeline = std::make_unique<DecodePipeline>(prescalers, consumers, sessionFactory_);
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
    decode.packetsOut = consumers.eventCount();
  }
  consumers.finishIssues();
  const auto decodeEnd = std::chrono::steady_clock::now();
  diagnostics_.report({
      DiagnosticSink::Severity::Info,
      DiagnosticSink::Category::Decode,
      "summary",
      decodeSummary(decode, decodeEnd - decodeStart),
  });
  if (decoderFatal) {
    consumers.abortOutputs();
  } else {
    consumers.finishOutputs();
  }
}
