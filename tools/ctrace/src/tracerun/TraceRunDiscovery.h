/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_TRACERUN_TRACERUNDISCOVERY_H
#define CTRACE_SRC_TRACERUN_TRACERUNDISCOVERY_H

#include "CtraceRunMeta.h"
#include "TraceRunConfig.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <fstream>
#include <istream>
#include <optional>
#include <string>
#include <vector>

/** @brief Associates one discovered raw trace input with its channel name. */
struct TraceRunRawInput {
  std::filesystem::path path;
  std::string channel;
};

class FileDecodeJob;
class TraceRunInputDescriptorTestAccess;

/** @brief Owns one selected, preflighted raw input handle and its normalized metadata. */
class TraceRunInputDescriptor {
public:
  /** @brief Moves exclusive ownership of one preflighted input handle. */
  TraceRunInputDescriptor(TraceRunInputDescriptor&&) = default;
  /** @brief Moves exclusive ownership of one preflighted input handle. */
  TraceRunInputDescriptor& operator=(TraceRunInputDescriptor&&) = default;
  /** @brief Disables copying because the descriptor owns an open input handle. */
  TraceRunInputDescriptor(const TraceRunInputDescriptor&) = delete;
  /** @brief Disables copy assignment because the descriptor owns an open input handle. */
  TraceRunInputDescriptor& operator=(const TraceRunInputDescriptor&) = delete;

  /** @brief Returns the selected raw-input path used for diagnostics and output naming. */
  const std::filesystem::path& path() const noexcept;
  /** @brief Returns the effective byte format of this input. */
  TraceRunFormat format() const noexcept;
  /** @brief Returns the normalized trace-run metadata and routes. */
  const CtraceRunMeta& metadata() const noexcept;

private:
  friend class FileDecodeJob;
  friend class TraceRunDiscovery;
  friend class TraceRunInputDescriptorTestAccess;

  /** @brief Returns the preflighted stream positioned at the first input byte. */
  std::istream& stream() noexcept;

  /** @brief Creates one descriptor after successful selection and preflight. */
  TraceRunInputDescriptor(std::filesystem::path path, TraceRunFormat format, CtraceRunMeta metadata,
                          std::ifstream stream);

  std::filesystem::path m_path;
  TraceRunFormat m_format = TraceRunFormat::Unformatted;
  CtraceRunMeta m_metadata;
  std::ifstream m_stream;
};

/** @brief Receives recognized raw inputs excluded from the active selection contract. */
using SkippedTraceRunInputSink = std::function<void(const TraceRunRawInput&)>;

/** @brief Discovers trace-run configurations and their raw input files. */
class TraceRunDiscovery final {
public:
  /**
   * @brief Selects one target configuration or every configuration in a trace directory.
   * @param traceDir Directory containing trace-run files.
   * @param target Optional solution-set name to select exclusively.
   * @return Deterministically ordered configuration paths.
   * @throws std::runtime_error If the directory or requested target is invalid.
   */
  static std::vector<std::filesystem::path> selectConfigFiles(const std::filesystem::path& traceDir,
                                                              const std::optional<std::string>& target);
  /**
   * @brief Derives the solution-set name from a trace-run filename.
   * @param configFile Trace-run configuration path.
   * @return Filename without the ctrace-run suffix.
   */
  static std::string solutionSetName(const std::filesystem::path& configFile);
  /**
   * @brief Selects every supported raw input associated with one trace-run configuration.
   * @param config Parsed trace-run configuration with its source path and optional format override.
   * @param skippedInputSink Optional observer for recognized inputs excluded from selection.
   * @return Deterministically ordered inputs for independent preflight and decoding.
   * @throws std::runtime_error If the source path is missing or no eligible input exists.
   */
  static std::vector<TraceRunRawInput> selectInputs(const TraceRunConfig& config,
                                                   const SkippedTraceRunInputSink& skippedInputSink = {});
  /**
   * @brief Preflights one selected raw input, resolves its format, and normalizes its routes.
   * @param config Configuration copied so each input resolves its own effective format and routes.
   * @param selected Input returned by selectInputs().
   * @return Fully normalized input descriptor safe to pass to a decode job.
   * @throws std::runtime_error If file access, formatted alignment, or route metadata is invalid.
   */
  static TraceRunInputDescriptor resolveInput(TraceRunConfig config, const TraceRunRawInput& selected);

private:
  /** @brief Discovers recognized raw inputs associated with one trace-run file. */
  static std::vector<TraceRunRawInput> rawInputs(const std::filesystem::path& configFile);
  /** @brief Prevents construction of this stateless discovery utility. */
  TraceRunDiscovery() = delete;
};

#endif  // CTRACE_SRC_TRACERUN_TRACERUNDISCOVERY_H
