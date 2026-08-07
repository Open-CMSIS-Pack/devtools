/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CliParser.h"

#include "CliOptions.h"
#include "ProductInfo.h"
#include "TraceSelection.h"
#include "TraceStreamId.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cxxopts.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

/** @brief Tests whether a command-line token begins with an option prefix. */
static bool startsWithDash(const std::string& value)
{
  return !value.empty() && value.front() == '-';
}

/** @brief Parses a complete unsigned command-line value or reports its option. */
static std::uint32_t parseUnsignedInteger(const std::string& value, const std::string& option)
{
  std::uint32_t parsed = 0;
  const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (value.empty() || result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
    throw std::runtime_error(option + " must be an unsigned integer, got " + value);
  }
  return parsed;
}

/** @brief Consumes one repeated or space-separated multi-value option. */
static std::optional<std::vector<std::string>> consumeMultiValueOption(const std::string& arg,
                                                                       const std::string& option, std::size_t& index,
                                                                       const std::vector<std::string>& arguments)
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
    while (index + 1U < arguments.size() && !startsWithDash(arguments[index + 1U])) {
      append(arguments[++index]);
    }
  } else {
    append(arg.substr(option.size() + 1U));
  }
  if (values.empty()) {
    throw std::runtime_error("Missing value for " + option);
  }
  return values;
}

/** @brief Expands multi-value options into the representation expected by cxxopts. */
static std::vector<std::string> normalizeArgsForCxxopts(const std::vector<std::string>& arguments)
{
  std::vector<std::string> args;
  args.reserve(arguments.size());
  if (!arguments.empty()) {
    args.push_back(arguments.front());
  }

  for (std::size_t index = 1U; index < arguments.size(); ++index) {
    const auto& arg = arguments[index];
    if (const auto types = consumeMultiValueOption(arg, "--type", index, arguments)) {
      for (const auto& type : *types) {
        args.emplace_back("--type");
        args.push_back(type);
      }
      continue;
    }
    if (const auto streams = consumeMultiValueOption(arg, "--stream", index, arguments)) {
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

/** @brief Creates a stable argv view over owned argument strings. */
static std::vector<const char*> asArgv(const std::vector<std::string>& args)
{
  std::vector<const char*> argv;
  argv.reserve(args.size());
  for (const auto& arg : args) {
    argv.push_back(arg.c_str());
  }
  return argv;
}

/** @brief Resolves output switches into one normalized format selection. */
static OutputFormat selectedOutputFormat(bool csvRequested, bool ctfRequested, bool allRequested)
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

/** @brief Registers the complete ctrace command-line grammar. */
static void configureCliParser(cxxopts::Options& parser)
{
  using SelectionValues = std::vector<std::string>;

  parser.custom_help("<trace-dir> [options]");
  parser.set_width(100U);
  parser.allow_unrecognised_options();
  parser.add_options()("csv", "Generate only CSV output")("ctf", "Generate only CTF / Trace Compass XML output")(
      "a,all", "Generate CSV and CTF / XML output")("type", "Filter output for specific packet types (default: all)",
                                                    cxxopts::value<SelectionValues>(), "sel [...]")(
      "stream", "Filter output for specific streams (default: all)", cxxopts::value<SelectionValues>(),
      "sel [...]")("t,target", "Specify a trace solution-set (default: all)",
                   cxxopts::value<std::string>())("V,version", "Print version");
  parser.add_options("Hidden")("h,help", "Print help");
}

/** @brief Parses normalized command-line arguments into ctrace options. */
static CliOptions parseCliArgs(const std::vector<std::string>& arguments)
{
  CliOptions options;
  auto normalizedArgs = normalizeArgsForCxxopts(arguments);
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

  options.help = parsed.count("help") != 0U;
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

/** @brief Validates semantic relationships between parsed options. */
static void validateCliOptions(const CliOptions& options)
{
  if (options.help) {
    return;
  }
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

CliOptions CliParser::parse(const std::vector<std::string>& arguments)
{
  return parseCliArgs(arguments);
}

void CliParser::validate(const CliOptions& options)
{
  validateCliOptions(options);
}

std::string CliParser::helpString()
{
  cxxopts::Options parser("ctrace", "CMSIS trace utility");
  configureCliParser(parser);
  return parser.help({""});
}

std::string CliParser::versionString()
{
  return std::string("ctrace ") + CtraceVersion;
}
