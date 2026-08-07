/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceRunDiscovery.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

constexpr std::string_view ConfigSuffix = ".ctrace-run.yml";

/** @brief Tests whether a string ends in the supplied suffix. */
static bool endsWith(const std::string_view& value, const std::string_view& suffix)
{
  return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

/** @brief Tests whether a file extension names a supported trace channel. */
static bool isTraceChannel(const std::string_view& value)
{
  return value == "SWO" || value == "TB" || value == "ER";
}

/** @brief Tests whether a solution-set name is reserved by Windows. */
static bool isWindowsReservedFilename(const std::string_view& value)
{
  const auto extension = value.find('.');
  const auto basename = value.substr(0U, extension);
  std::string upper;
  upper.reserve(basename.size());
  std::transform(basename.begin(), basename.end(), std::back_inserter(upper), [](const char character) {
    return static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
  });

  if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" || upper == "CONIN$" || upper == "CONOUT$") {
    return true;
  }
  return upper.size() == 4U && (upper.compare(0U, 3U, "COM") == 0 || upper.compare(0U, 3U, "LPT") == 0) &&
         upper[3] >= '1' && upper[3] <= '9';
}

/** @brief Tests whether a solution-set name is portable and path-safe. */
static bool isSafeSolutionSetName(const std::string_view& value)
{
  if (value.empty() || value == "." || value == ".." || value.back() == '.' || value.back() == ' ' ||
      isWindowsReservedFilename(value)) {
    return false;
  }

  constexpr std::string_view invalidFilenameCharacters = "<>:\"/\\|?*";
  return std::none_of(value.begin(), value.end(), [invalidFilenameCharacters](const char character) {
    const auto byte = static_cast<unsigned char>(character);
    return byte < 0x20U || invalidFilenameCharacters.find(character) != std::string_view::npos;
  });
}

/** @brief Rejects absent, non-regular, or unreadable trace-run files. */
static void requireReadableConfigFile(const std::filesystem::path& path)
{
  if (!std::filesystem::is_regular_file(path)) {
    throw std::runtime_error("trace-run configuration not found: " + path.string());
  }
}

std::vector<std::filesystem::path> TraceRunDiscovery::selectConfigFiles(const std::filesystem::path& traceDir,
                                                                        const std::optional<std::string>& target)
{
  if (!std::filesystem::is_directory(traceDir)) {
    throw std::runtime_error("trace directory not found: " + traceDir.string());
  }

  if (target.has_value()) {
    if (!isSafeSolutionSetName(*target)) {
      throw std::runtime_error("--target must be a solution-set name, got " + *target);
    }
    auto path = traceDir / (*target + std::string(ConfigSuffix));
    requireReadableConfigFile(path);
    return {std::move(path)};
  }

  std::vector<std::filesystem::path> configs;
  for (const auto& entry : std::filesystem::directory_iterator(traceDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto filename = entry.path().filename().string();
    if (endsWith(filename, ConfigSuffix) && filename.size() > std::string_view(ConfigSuffix).size()) {
      configs.push_back(entry.path());
    }
  }
  std::sort(configs.begin(), configs.end());
  if (configs.empty()) {
    throw std::runtime_error("no *" + std::string(ConfigSuffix) +
                             " files found in trace directory: " + traceDir.string());
  }
  return configs;
}

std::string TraceRunDiscovery::solutionSetName(const std::filesystem::path& configFile)
{
  const auto filename = configFile.filename().string();
  const auto suffix = std::string_view(ConfigSuffix);
  if (!endsWith(filename, suffix) || filename.size() == suffix.size()) {
    throw std::runtime_error("expected <solution-set>" + std::string(ConfigSuffix) + ", got " + configFile.string());
  }
  return filename.substr(0, filename.size() - suffix.size());
}

std::vector<TraceRunRawInput> TraceRunDiscovery::rawInputs(const std::filesystem::path& configFile)
{
  const auto solutionSet = solutionSetName(configFile);
  const auto prefix = solutionSet + ".";
  constexpr std::string_view suffix = ".raw";
  const auto directory = configFile.has_parent_path() ? configFile.parent_path() : std::filesystem::path(".");

  std::vector<TraceRunRawInput> inputs;
  for (const auto& entry : std::filesystem::directory_iterator(directory)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto filename = entry.path().filename().string();
    if (filename.rfind(prefix, 0) != 0 || !endsWith(filename, suffix)) {
      continue;
    }
    const auto channelSize = filename.size() - prefix.size() - suffix.size();
    if (channelSize == 0U) {
      continue;
    }
    const auto channel = filename.substr(prefix.size(), channelSize);
    if (!isTraceChannel(channel)) {
      continue;
    }
    inputs.push_back({entry.path(), channel});
  }
  std::sort(inputs.begin(), inputs.end(),
            [](const TraceRunRawInput& left, const TraceRunRawInput& right) { return left.path < right.path; });
  return inputs;
}
