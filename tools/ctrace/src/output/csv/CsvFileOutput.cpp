/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CsvFileOutput.hpp"

#include "CsvRowMapper.hpp"
#include "TraceEvent.hpp"
#include "TraceSelection.hpp"

#include <filesystem>
#include <ios>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

void removeExistingCsv(const std::filesystem::path& path)
{
  const auto normalized = path.lexically_normal();
  if (path.empty() || normalized == normalized.root_path() || normalized.filename().empty() || // LCOV_EXCL_BR_LINE
      normalized.filename() == "." || normalized.filename() == "..") {                         // LCOV_EXCL_BR_LINE
    throw std::invalid_argument("CSV output path must identify a file");
  }
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (error && error != std::errc::no_such_file_or_directory) {
    throw std::runtime_error("Failed to inspect existing CSV output " + path.string() + ": " + error.message());
  }
  if (!error && std::filesystem::is_directory(status)) {
    throw std::runtime_error("Refusing to replace CSV output because the target is a directory: " + path.string());
  }
  error.clear();
  std::filesystem::remove(path, error);
  if (error) { // LCOV_EXCL_BR_LINE: requires an external filesystem failure
    // LCOV_EXCL_START: requires an external filesystem failure after successful inspection
    throw std::runtime_error("Failed to remove existing CSV output " + path.string() + ": " + error.message());
    // LCOV_EXCL_STOP
  }
}

void createParentDirectory(const std::filesystem::path& path)
{
  const auto parent = path.parent_path();
  if (parent.empty()) {
    return;
  }
  std::error_code error;
  std::filesystem::create_directories(parent, error);
  if (error) { // LCOV_EXCL_BR_LINE: requires an external filesystem failure
    // LCOV_EXCL_START: requires an external filesystem failure after successful parent inspection
    throw std::runtime_error("Failed to create CSV output directory " + parent.string() + ": " + error.message());
    // LCOV_EXCL_STOP
  }
}

} // namespace

CsvFileOutput::CsvFileOutput(std::filesystem::path outputFile, TraceSelection selection)
  : outputFile_(std::move(outputFile)), selection_(std::move(selection))
{
}

CsvFileOutput::~CsvFileOutput()
{
  try {
    CsvFileOutput::abort();
    // LCOV_EXCL_START: destructors cannot expose a best-effort filesystem cleanup failure
  } catch (...) {
    (void)0;
  }
  // LCOV_EXCL_STOP
}

std::string_view CsvFileOutput::backendName() const noexcept
{
  return "csv";
}

std::string CsvFileOutput::targetPath() const
{
  return outputFile_.string();
}

void CsvFileOutput::start()
{
  abort();
  const auto& outputPath = outputFile_;
  removeExistingCsv(outputPath);
  createParentDirectory(outputPath);
  active_ = true;
  stream_.clear();
  stream_.open(outputPath, std::ios::out | std::ios::trunc | std::ios::binary);
  if (!stream_.is_open()) {
    abort();
    throw std::runtime_error("Failed to open CSV output " + outputPath.string());
  }

  stream_ << CsvRowMapper::header() << "\n";
  if (!stream_) { // LCOV_EXCL_BR_LINE: requires an external device write failure
    abort();      // LCOV_EXCL_LINE
    throw std::runtime_error("Failed to write CSV output " + outputFile_.string()); // LCOV_EXCL_LINE
  }
}

void CsvFileOutput::stop()
{
  if (stream_.is_open()) {
    stream_.close();
  }
  if (!stream_) { // LCOV_EXCL_BR_LINE: requires an external device close failure
    abort();      // LCOV_EXCL_LINE
    throw std::runtime_error("Failed to write CSV output " + outputFile_.string()); // LCOV_EXCL_LINE
  }
  active_ = false;
}

void CsvFileOutput::abort()
{
  if (stream_.is_open()) {
    stream_.close();
  }
  stream_.clear();
  if (active_) {
    removeExistingCsv(outputFile_);
    active_ = false;
  }
}

void CsvFileOutput::writeEvent(const TraceEvent& event)
{
  if (!stream_.is_open()) {
    return;
  }
  if (!traceEventSelectedForOutput(event, selection_)) {
    return;
  }

  stream_ << CsvRowMapper::row(event) << "\n";
}
