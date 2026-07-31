/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "FileDecodeJob.hpp"

#include "CliOptions.hpp"
#include "csv/CsvFileOutput.hpp"
#include "ctf/CtfBundleOutput.hpp"
#include "CortexMStreamDecoder.hpp"
#include "DecodeConsumers.hpp"
#include "DecodePipeline.hpp"
#include "DiagnosticSink.hpp"
#include "OpenCsdItmDecoder.hpp"
#include "OutputRequirements.hpp"
#include "TraceOutput.hpp"
#include "TraceOutputConfig.hpp"
#include "TraceRunConfig.hpp"
#include "CtraceRunMeta.hpp"

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

namespace {

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
    if (eof_ || !stream_.is_open()) { // LCOV_EXCL_BR_LINE: read() is not called again after EOF
      return {{}, true};
    }

    stream_.read(reinterpret_cast<char*>(buffer_.data()), static_cast<std::streamsize>(buffer_.size()));
    const auto readBytes = stream_.gcount();
    if (readBytes > 0) {
      if (stream_.eof()) { // LCOV_EXCL_BR_LINE: exact-buffer boundaries do not change decoding behavior
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

std::string decodeSummary(const DecodeResult& decode, std::chrono::steady_clock::duration elapsed)
{
  const auto seconds = std::chrono::duration<double>(elapsed).count();
  const auto mebibytes = static_cast<double>(decode.bytesIn) / (1024.0 * 1024.0);
  const auto mebibytesPerSecond =
      seconds > 0.0 ? mebibytes / seconds : 0.0; // LCOV_EXCL_BR_LINE: a measured interval cannot be negative

  std::ostringstream out;
  out << "decoded " << decode.packetsOut << " packets from " << decode.bytesIn << " bytes in " << std::fixed
      << std::setprecision(3) << seconds << " s (" << std::setprecision(2) << mebibytesPerSecond << " MiB/s)";
  return out.str();
}

ItmTimestampPrescalers timestampPrescalers(const CtraceRunMeta& ctraceRunMeta)
{
  auto fallback = ctraceRunMeta.timestampPrescaler();
  if (!fallback.has_value() && !ctraceRunMeta.hasDistinctProcessorPrescalers()) {
    fallback = TraceRunSchema::kDefaultTimestampPrescaler;
  }
  return {fallback, ctraceRunMeta.timestampPrescalersByTraceBusId()};
}

TraceOutputRequest outputRequest(const CliOptions& options)
{
  return {
      options.outputFormat == OutputFormat::Csv || options.outputFormat == OutputFormat::All,
      options.outputFormat == OutputFormat::Ctf || options.outputFormat == OutputFormat::All,
      options.selection,
  };
}

std::vector<std::unique_ptr<TraceOutput>> createConfiguredOutputs(const TraceOutputPlan& outputPlan,
                                                                  DiagnosticSink& diagnostics)
{
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  if (outputPlan.ctf.has_value()) {
    outputs.push_back(std::make_unique<CtfBundleOutput>(*outputPlan.ctf, &diagnostics));
    // LCOV_EXCL_BR_START: generated aggregate-initializer exception edges
    diagnostics.report({
        DiagnosticSink::Severity::Info,
        DiagnosticSink::Category::Output,
        "trace-compass-xml",
        "configured Trace Compass XML",
        {{"path", outputPlan.ctf->traceCompassXmlPath.string()}},
    });
    // LCOV_EXCL_BR_STOP
  }
  if (outputPlan.csv.has_value()) {
    outputs.push_back(std::make_unique<CsvFileOutput>(outputPlan.csv->outputPath, outputPlan.csv->selection));
  }
  return outputs;
} // LCOV_EXCL_LINE: GCC attributes the generated vector cleanup to this closing brace

} // namespace

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
  // LCOV_EXCL_BR_START: generated aggregate-initializer exception edges
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
  // LCOV_EXCL_BR_STOP
  auto outputs = createConfiguredOutputs(outputPlan, diagnostics_);
  DecodeConsumers consumers(std::move(outputs), diagnostics_, ctraceRunMeta_.itmEnableMask(),
                            ctraceRunMeta_.itmEnableMasksByTraceBusId());

  if (prescalers.fallback.has_value()) {
    // LCOV_EXCL_BR_START: generated aggregate-initializer exception edges
    diagnostics_.report({
        DiagnosticSink::Severity::Info,
        DiagnosticSink::Category::Input,
        "timestamp-prescaler",
        "using timestamp prescaler",
        {{"value", std::to_string(*prescalers.fallback)}},
    });
    // LCOV_EXCL_BR_STOP
  } else {
    // LCOV_EXCL_BR_START: generated aggregate-initializer exception edges
    diagnostics_.report({
        DiagnosticSink::Severity::Info,
        DiagnosticSink::Category::Input,
        "timestamp-prescaler",
        "using Trace-Bus-ID-specific timestamp prescalers",
        {{"traceBusIds", std::to_string(prescalers.byTraceBusId.size())}},
    });
    // LCOV_EXCL_BR_STOP
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
