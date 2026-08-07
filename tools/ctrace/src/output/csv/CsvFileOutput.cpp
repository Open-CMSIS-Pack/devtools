/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CsvFileOutput.h"

#include "CsvRowMapper.h"
#include "TraceEvent.h"
#include "TraceSelection.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

/** @brief Adapts a binary output file to the CSV stream interface. */
class CsvFileStream final : public CsvFileOutput::Stream {
public:
  /** @brief Opens a new binary output file, replacing an existing file. */
  explicit CsvFileStream(const std::filesystem::path& path)
    : m_stream(path, std::ios::out | std::ios::trunc | std::ios::binary)
  {
  }

  /** @brief Returns the underlying output file stream. */
  std::ostream& output() override { return m_stream; }

  /** @brief Closes the underlying output file stream. */
  void close() override { m_stream.close(); }

private:
  std::ofstream m_stream;
};

/** @brief Safely removes an existing non-directory CSV target. */
static void removeExistingCsv(const std::filesystem::path& path)
{
  const auto normalized = path.lexically_normal();
  if (path.empty() || normalized == normalized.root_path() || normalized.filename().empty() ||
      normalized.filename() == "." || normalized.filename() == "..") {
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
  if (error) {
    throw std::runtime_error("Failed to remove existing CSV output " + path.string() + ": " + error.message());
  }
}

/** @brief Creates the parent directory of a CSV target when needed. */
static void createParentDirectory(const std::filesystem::path& path)
{
  const auto parent = path.parent_path();
  if (parent.empty()) {
    return;
  }
  std::error_code error;
  std::filesystem::create_directories(parent, error);
  if (error) {
    throw std::runtime_error("Failed to create CSV output directory " + parent.string() + ": " + error.message());
  }
}

CsvFileOutput::CsvFileOutput(std::filesystem::path outputFile, TraceSelection selection)
  : CsvFileOutput(std::move(outputFile), std::move(selection), [](const std::filesystem::path& path) {
      return std::make_unique<CsvFileStream>(path);
    })
{
}

CsvFileOutput::CsvFileOutput(std::filesystem::path outputFile, TraceSelection selection, StreamFactory streamFactory)
  : m_outputFile(std::move(outputFile)),
    m_selection(std::move(selection)),
    m_streamFactory(std::move(streamFactory))
{
  if (!m_streamFactory) {
    throw std::invalid_argument("CSV stream factory must be configured");
  }
}

CsvFileOutput::~CsvFileOutput()
{
  try {
    CsvFileOutput::abort();
  } catch (...) {
    (void)0;
  }
}

std::string_view CsvFileOutput::backendName() const noexcept
{
  return "csv";
}

std::string CsvFileOutput::targetPath() const
{
  return m_outputFile.string();
}

void CsvFileOutput::start()
{
  abort();
  const auto& outputPath = m_outputFile;
  removeExistingCsv(outputPath);
  createParentDirectory(outputPath);
  m_active = true;
  m_stream = m_streamFactory(outputPath);
  if (m_stream == nullptr || !m_stream->output()) {
    abort();
    throw std::runtime_error("Failed to open CSV output " + outputPath.string());
  }

  m_stream->output() << CsvRowMapper::header() << "\n";
  if (!m_stream->output()) {
    abort();
    throw std::runtime_error("Failed to write CSV output " + m_outputFile.string());
  }
}

void CsvFileOutput::stop()
{
  if (m_stream != nullptr) {
    m_stream->close();
  }
  const auto failed = m_stream != nullptr && !m_stream->output();
  m_stream.reset();
  if (failed) {
    abort();
    throw std::runtime_error("Failed to write CSV output " + m_outputFile.string());
  }
  m_active = false;
}

void CsvFileOutput::abort()
{
  m_stream.reset();
  if (m_active) {
    removeExistingCsv(m_outputFile);
    m_active = false;
  }
}

void CsvFileOutput::writeEvent(const TraceEvent& event)
{
  if (m_stream == nullptr) {
    return;
  }
  if (!traceEventSelectedForOutput(event, m_selection)) {
    return;
  }

  m_stream->output() << CsvRowMapper::row(event) << "\n";
}
