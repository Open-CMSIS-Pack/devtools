/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "FileDecodeJob.h"

#include "DiagnosticMessages.h"
#include "CliOptions.h"
#include "csv/CsvFileOutput.h"
#include "ctf/CtfBundleOutput.h"
#include "CortexMStreamDecoder.h"
#include "CoreSightFormatter.h"
#include "DecodeConsumers.h"
#include "DecodePipeline.h"
#include "DiagnosticSink.h"
#include "OpenCsdItmDecoder.h"
#include "OutputRequirements.h"
#include "ProductInfo.h"
#include "RawDiagnosticSink.h"
#include "TraceMessages.h"
#include "TraceOutput.h"
#include "TraceOutputConfig.h"
#include "TraceRunConfig.h"
#include "CtraceRunMeta.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <ios>
#include <memory>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
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
  RawFileReader(std::filesystem::path path, std::istream& stream, std::optional<std::uint64_t> byteLimit)
    : m_path(std::move(path)),
      m_stream(stream),
      m_buffer(64U * 1024U),
      m_remaining(byteLimit)
  {
  }

  /** @brief Returns the next raw byte chunk. */
  ReadResult read()
  {
    if (m_eof || m_remaining == 0U) {
      return {{}, true};
    }

    const auto requested = m_remaining.has_value() ? std::min<std::uint64_t>(m_buffer.size(), *m_remaining)
                                                   : m_buffer.size();
    m_stream.read(reinterpret_cast<char*>(m_buffer.data()), static_cast<std::streamsize>(requested));
    const auto readBytes = m_stream.gcount();
    if (readBytes > 0) {
      if (m_remaining.has_value()) {
        *m_remaining -= static_cast<std::uint64_t>(readBytes);
      }
      if (m_stream.eof()) {
        m_eof = true;
      }
      return {{m_buffer.data(), static_cast<std::size_t>(readBytes)}, false};
    }
    if (m_stream.bad()) {
      throw std::runtime_error(pathDiagnosticMessage(PathDiagnosticCode::RawInputReadFailed, m_path.string()));
    }
    m_eof = true;
    return {{}, true};
  }

private:
  std::filesystem::path m_path;
  std::istream& m_stream;
  std::vector<std::uint8_t> m_buffer;
  std::optional<std::uint64_t> m_remaining;
  bool m_eof = false;
};

/** @brief Retains the incomplete final formatter frame even if decoding fails before reaching it. */
static TraceRawTail readRawTail(const std::filesystem::path& path, std::istream& stream, std::uint64_t size)
{
  const auto tailSize = size % CoreSightFormatter::kMemoryAlignedFrameSize;
  TraceRawTail tail{size - tailSize, std::vector<std::uint8_t>(tailSize)};
  if (tail.bytes.empty()) {
    return tail;
  }

  stream.seekg(static_cast<std::streamoff>(tail.rawOffset), std::ios::beg);
  stream.read(reinterpret_cast<char*>(tail.bytes.data()), static_cast<std::streamsize>(tail.bytes.size()));
  stream.seekg(0, std::ios::beg);
  if (!stream) {
    throw std::runtime_error(pathDiagnosticMessage(PathDiagnosticCode::RawInputReadFailed, path.string()));
  }
  return tail;
}

/** @brief Formats input size, elapsed time, throughput, and the trace/diagnostic record count. */
static std::string decodeSummary(const DecodeResult& decode, std::chrono::steady_clock::duration elapsed)
{
  const auto seconds = std::chrono::duration<double>(elapsed).count();
  return decodeSummaryMessage(decode.bytesIn, seconds, decode.eventsOut);
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

/** @brief Owns configured backends and borrows access to CTF completion metadata. */
struct ConfiguredOutputs {
  std::vector<std::unique_ptr<TraceOutput>> backends;
  CtfBundleOutput* ctf = nullptr;
};

/** @brief Creates backends and retains non-owning access to completed CTF metadata. */
static ConfiguredOutputs createConfiguredOutputs(const TraceOutputPlan& outputPlan, DiagnosticSink& diagnostics)
{
  ConfiguredOutputs outputs;
  if (outputPlan.ctf.has_value()) {
    auto ctf = std::make_unique<CtfBundleOutput>(*outputPlan.ctf, &diagnostics);
    outputs.ctf = ctf.get();
    outputs.backends.push_back(std::move(ctf));
  }
  if (outputPlan.csv.has_value()) {
    outputs.backends.push_back(std::make_unique<CsvFileOutput>(outputPlan.csv->outputPath, outputPlan.csv->selection));
  }
  return outputs;
}

/** @brief Reports the normalized trace-run model selected for one decode job. */
static void reportTraceRunMeta(const CtraceRunMeta& meta, DiagnosticSink& diagnostics)
{
  DiagnosticSink::Event diagnostic{
      DiagnosticSink::Severity::Info,
      formatMessage(MessageId::AppliedTraceMetadata),
      {
          {"path", meta.configPath()},
          {"routes", std::to_string(meta.routes().size())},
          {"sources", std::to_string(sourceCount(meta))},
      },
  };
  diagnostic.visibility = DiagnosticSink::Visibility::Verbose;
  diagnostics.report(diagnostic);
}

/** @brief Reports the file identity and byte-coordinate contract once per input. */
static void reportRawInput(const TraceRunInputDescriptor& input, DiagnosticSink& diagnostics)
{
  std::error_code error;
  const auto absolutePath = std::filesystem::absolute(input.path(), error);
  const bool formatted = input.format() == TraceRunFormat::Formatted;
  DiagnosticSink::Event diagnostic{
      DiagnosticSink::Severity::Info,
      formatMessage(MessageId::RawInputInspection),
      {
          {"input_path", error ? input.path().string() : absolutePath.string()},
          {"input_size", std::to_string(input.size())},
          {"format", formatted ? "formatted" : "unformatted"},
          {"framing", formatted ? "memory-aligned-16" : "none"},
          {"config", input.metadata().configPath()},
          {"ctrace_version", CtraceVersion},
          {"offset_unit", "byte"},
          {"offset_base", "0"},
      },
  };
  diagnostic.visibility = DiagnosticSink::Visibility::Verbose;
  diagnostics.report(diagnostic);
}

/** @brief Reports the timestamp prescaler applied to every normalized route. */
static void reportTimestampPrescalers(const CtraceRunMeta& meta, DiagnosticSink& diagnostics)
{
  for (const auto& route : meta.routes()) {
    std::vector<std::pair<std::string, std::string>> context;
    context.emplace_back("value", std::to_string(route.timestampPrescaler));
    if (route.identity.traceBusId.has_value()) {
      context.emplace_back("stream", std::to_string(*route.identity.traceBusId));
    }
    if (route.processorName.has_value()) {
      context.emplace_back("pname", *route.processorName);
    }
    DiagnosticSink::Event diagnostic{DiagnosticSink::Severity::Info,
                                     formatMessage(MessageId::UsingTimestampPrescaler), std::move(context)};
    diagnostic.visibility = DiagnosticSink::Visibility::Verbose;
    diagnostics.report(diagnostic);
  }
}

/** @brief Creates the production or injected OpenCSD decode pipeline. */
static std::unique_ptr<DecodePipeline>
createDecodePipeline(const std::vector<CortexMDecodeRoute>& routes, OpenCsdItmInputMode inputMode,
                     DecodeConsumers& consumers, const OpenCsdItmSessionFactory& sessionFactory,
                     DiagnosticSink& diagnostics)
{
  if (sessionFactory) {
    return std::make_unique<DecodePipeline>(routes, inputMode, consumers, sessionFactory);
  }
  return std::make_unique<DecodePipeline>(
      routes, inputMode, consumers, [&diagnostics](std::uint8_t traceBusId, std::uint64_t sourceOffset) {
        DiagnosticSink::Event diagnostic{
            DiagnosticSink::Severity::Warning,
            formatMessage(MessageId::SkippingUnsupportedTraceSource),
            {
                {"stream", std::to_string(traceBusId)},
            },
        };
        diagnostic.rawLocation = RawDiagnosticLocation{};
        diagnostic.rawLocation->offset = sourceOffset;
        diagnostic.rawLocation->kind = RawDiagnosticLocation::Kind::FormatterGroup;
        diagnostics.report(diagnostic);
      });
}

/** @brief Streams one preflighted raw input through its configured decode pipeline. */
static DecodeResult decodeRawInput(const std::filesystem::path& path, std::istream& stream, DecodePipeline& pipeline,
                                  std::optional<std::uint64_t> byteLimit)
{
  RawFileReader input(path, stream, byteLimit);
  while (true) {
    const auto read = input.read();
    if (read.eof) {
      break;
    }
    pipeline.push(read.bytes);
  }
  return pipeline.finish();
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

std::optional<CtfMetadataModel> FileDecodeJob::run()
{
  RawDiagnosticSink diagnostics(m_diagnostics, m_input.format() == TraceRunFormat::Formatted, m_input.size());
  const auto& ctraceRunMeta = m_input.metadata();
  const auto routes = decodeRoutes(ctraceRunMeta);
  const auto inputMode = decodeInputMode(m_input);
  auto outputPlan = planTraceOutputs(outputRequest(m_options), m_input.path(), ctraceRunMeta, diagnostics);
  if (outputPlan.hasRequestedOutputs() && !outputPlan.hasEnabledOutputs()) {
    return std::nullopt;
  }
  reportRawInput(m_input, diagnostics);
  reportTraceRunMeta(ctraceRunMeta, diagnostics);
  auto outputs = createConfiguredOutputs(outputPlan, diagnostics);
  DecodeConsumers consumers(std::move(outputs.backends), diagnostics, itmEnableMasks(ctraceRunMeta));
  reportTimestampPrescalers(ctraceRunMeta, diagnostics);

  const auto decodeStart = std::chrono::steady_clock::now();
  const bool formatted = m_input.format() == TraceRunFormat::Formatted;
  // The capture starts at frame offset zero. Never search for or invent another frame boundary.
  const auto tail = formatted ? readRawTail(m_input.path(), m_input.stream(), m_input.size()) : TraceRawTail{};
  const auto byteLimit = formatted ? std::optional<std::uint64_t>{tail.rawOffset} : std::nullopt;
  DecodeResult decode;
  std::optional<TraceDecodeAbort> decodeAbort;
  try {
    auto pipeline = createDecodePipeline(routes, inputMode, consumers, m_sessionFactory, diagnostics);
    decode = decodeRawInput(m_input.path(), m_input.stream(), *pipeline, byteLimit);
  } catch (const OpenCsdFatalError& error) {
    decodeAbort = TraceDecodeAbort{error.bytesProcessed(), error.message()};
    decode.bytesIn = error.bytesProcessed();
  }
  if (!tail.bytes.empty()) {
    consumers.appendRawTailDiagnostic(tail);
  }
  decode.eventsOut = consumers.eventCount() + (decodeAbort.has_value() ? 1U : 0U);
  consumers.finishIssues();
  if (decodeAbort.has_value()) {
    DiagnosticSink::Event diagnostic{DiagnosticSink::Severity::Error,
                                     formatTraceMessage(*decodeAbort, TraceMessageStyle::Compact)};
    diagnostic.detailedMessage = formatTraceMessage(*decodeAbort, TraceMessageStyle::Detailed);
    diagnostic.detailedContext.emplace_back("bytesProcessed", std::to_string(decodeAbort->bytesProcessed));
    diagnostic.rawLocation = RawDiagnosticLocation{};
    diagnostic.rawLocation->offset = decodeAbort->bytesProcessed;
    diagnostic.rawLocation->kind = RawDiagnosticLocation::Kind::InputProgress;
    if (decodeAbort->reason.packet.has_value()) {
      diagnostic.rawLocation->packetSize = decodeAbort->reason.packet->size;
    }
    diagnostics.report(diagnostic);
  }
  const auto decodeEnd = std::chrono::steady_clock::now();
  DiagnosticSink::Event summary{
      DiagnosticSink::Severity::Info,
      decodeSummary(decode, decodeEnd - decodeStart),
  };
  summary.visibility = DiagnosticSink::Visibility::Verbose;
  diagnostics.report(summary);
  consumers.finishOutputs(decodeAbort.has_value() ? &*decodeAbort : nullptr);
  const auto* metadata = outputs.ctf == nullptr ? nullptr : outputs.ctf->completedMetadata();
  return metadata == nullptr ? std::nullopt : std::optional<CtfMetadataModel>{*metadata};
}
