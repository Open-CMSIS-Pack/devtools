/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CliParser.hpp"

#include "CliOptions.hpp"
#include "TraceSelection.hpp"
#include "TraceStreamId.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cxxopts.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

bool startsWithDash(const std::string& value)
{
  return !value.empty() && value.front() == '-';
}

std::uint32_t parseUnsignedInteger(const std::string& value, const std::string& option)
{
  std::uint32_t parsed = 0;
  const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (value.empty() || result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
    throw std::runtime_error(option + " must be an unsigned integer, got " + value);
  }
  return parsed;
}

std::optional<std::vector<std::string>> consumeMultiValueOption(const std::string& arg, const std::string& option,
                                                                int& index, int argc, const char* const argv[])
{
  if (arg != option && arg.rfind(option + "=", 0) != 0) {
    return std::nullopt;
  }

  std::vector<std::string> values;
  const auto append = [&](const std::string& value) {
    if (value.empty()) {
      throw std::runtime_error("Missing value for " + option);
    }
    if (value.find(',') != std::string::npos) {
      throw std::runtime_error(option + " values must be separated by spaces");
    }
    values.push_back(value);
  };
  if (arg == option) {
    while (index + 1 < argc && !startsWithDash(argv[index + 1])) {
      append(argv[++index]);
    }
  } else {
    append(arg.substr(option.size() + 1U));
  }
  if (values.empty()) {
    throw std::runtime_error("Missing value for " + option);
  }
  return values;
}

std::vector<std::string> normalizeArgsForCxxopts(int argc, const char* const argv[])
{
  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(argc));
  if (argc > 0) {
    args.emplace_back(argv[0]);
  }

  for (int index = 1; index < argc; ++index) {
    const std::string arg = argv[index];
    if (const auto types = consumeMultiValueOption(arg, "--type", index, argc, argv)) {
      for (const auto& type : *types) {
        args.emplace_back("--type");
        args.push_back(type);
      }
      continue;
    }
    if (const auto streams = consumeMultiValueOption(arg, "--stream", index, argc, argv)) {
      for (const auto& stream : *streams) {
        args.emplace_back("--stream");
        args.push_back(stream);
      }
      continue;
    }
    args.push_back(arg);
  }
  return args;
}

std::vector<const char*> asArgv(const std::vector<std::string>& args)
{
  std::vector<const char*> argv;
  argv.reserve(args.size());
  for (const auto& arg : args) {
    argv.push_back(arg.c_str());
  }
  return argv;
}

OutputFormat selectedOutputFormat(bool csvRequested, bool ctfRequested, bool allRequested)
{
  if (allRequested || (csvRequested && ctfRequested)) {
    return OutputFormat::All;
  }
  if (csvRequested) {
    return OutputFormat::Csv;
  }
  if (ctfRequested) {
    return OutputFormat::Ctf;
  }
  return OutputFormat::None;
}

void configureCliParser(cxxopts::Options& parser)
{
  parser.custom_help("<trace-dir> [options]");
  parser.allow_unrecognised_options();
  parser.add_options()("V,version", "Print version")(
      "t,target", "Specify solution-set (default: process all solution sets)", cxxopts::value<std::string>())(
      "csv", "Generate only CSV files")("ctf", "Generate only CTF files")("a,all", "Generate CSV and CTF files")(
      "type", "Filter packet types (default: all packet types)", cxxopts::value<std::vector<std::string>>(),
      "sel [...]")("stream", "Filter streams (default: all streams)", cxxopts::value<std::vector<std::string>>(),
                   "sel [...]");
}

CliOptions parseCliArgs(int argc, const char* const argv[])
{
  CliOptions options;
  auto normalizedArgs = normalizeArgsForCxxopts(argc, argv);
  auto cargv = asArgv(normalizedArgs);
  auto mutableArgc = static_cast<int>(cargv.size());

  cxxopts::Options parser("ctrace", "CMSIS trace utility");
  configureCliParser(parser);

  const auto parsed = parser.parse(mutableArgc, cargv.data());
  const auto& unknown = parsed.unmatched();
  for (const auto& argument : unknown) {
    if (startsWithDash(argument)) {
      throw std::runtime_error("Unknown argument: " + argument);
    }
    if (options.traceDir.has_value()) {
      throw std::runtime_error("Unexpected positional argument: " + argument);
    }
    options.traceDir = argument;
  }

  options.version = parsed.count("version") != 0U;
  if (parsed.count("target") != 0U) {
    options.targetName = parsed["target"].as<std::string>();
  }
  if (parsed.count("type") != 0U) {
    for (const auto& type : parsed["type"].as<std::vector<std::string>>()) {
      options.selection.types.push_back(type);
    }
  }
  if (parsed.count("stream") != 0U) {
    for (const auto& stream : parsed["stream"].as<std::vector<std::string>>()) {
      const auto streamId = parseUnsignedInteger(stream, "--stream");
      if (!CoreSight::isTraceBusId(streamId)) {
        throw std::runtime_error("--stream must be a CoreSight Trace Bus ID between 0 and 111, got " + stream);
      }
      options.selection.streams.push_back(static_cast<std::uint8_t>(streamId));
    }
  }

  options.outputFormat =
      selectedOutputFormat(parsed.count("csv") != 0U, parsed.count("ctf") != 0U, parsed.count("all") != 0U);
  return options;
}

void validateCliOptions(const CliOptions& options)
{
  for (const auto& type : options.selection.types) {
    if (!parseTraceEventType(type).has_value()) {
      throw std::runtime_error("Invalid --type value: " + type + " (accepted: " + traceEventTypeList(", ") + ")");
    }
  }
  if (options.version) {
    return;
  }
  if (!options.traceDir.has_value()) {
    throw std::runtime_error("Specify <trace-dir>");
  }
}

} // namespace

CliOptions CliParser::parse(int argc, const char* const argv[])
{
  return parseCliArgs(argc, argv);
}

void CliParser::validate(const CliOptions& options)
{
  validateCliOptions(options);
}

std::string CliParser::helpString()
{
  cxxopts::Options parser("ctrace", "CMSIS trace utility");
  configureCliParser(parser);
  return parser.help();
}

std::string CliParser::versionString()
{
  return std::string("ctrace ") + CTRACE_VERSION;
}
