/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfBundleOutput.h"

#include "CtfEncoder.h"
#include "TraceCompassXmlWriter.h"
#include "TraceEvent.h"
#include "TraceOutputConfig.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

/** @brief Rejects empty and root-like output targets. */
static void requireOutputTarget(const std::filesystem::path& path, const char* description)
{
  const auto normalized = path.lexically_normal();
  if (path.empty() || normalized == normalized.root_path() || normalized.filename().empty() ||
      normalized.filename() == "." || normalized.filename() == "..") {
    throw std::invalid_argument(std::string(description) + " must identify a specific output path");
  }
}

/** @brief Returns a lexically normalized absolute path for safety comparisons. */
static std::filesystem::path normalizedAbsolutePath(const std::filesystem::path& path)
{
  std::error_code error;
  const auto absolute = std::filesystem::absolute(path, error);
  return (error ? path : absolute).lexically_normal();
}

/** @brief Folds one ASCII character for conservative portable path comparison. */
static char foldedPathCharacter(char character)
{
  return character >= 'A' && character <= 'Z' ? static_cast<char>(character + ('a' - 'A')) : character;
}

/** @brief Compares path components without relying on target-platform case rules. */
static bool pathComponentEquals(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
{
  const auto lhsText = lhs.generic_u8string();
  const auto rhsText = rhs.generic_u8string();
  return lhsText.size() == rhsText.size() &&
         std::equal(lhsText.begin(), lhsText.end(), rhsText.begin(), [](char lhsCharacter, char rhsCharacter) {
           return foldedPathCharacter(lhsCharacter) == foldedPathCharacter(rhsCharacter);
         });
}

/** @brief Tests whether a candidate path contains another target path. */
static bool isAncestorPath(const std::filesystem::path& candidate, const std::filesystem::path& path)
{
  auto candidateComponent = candidate.begin();
  auto pathComponent = path.begin();
  for (; candidateComponent != candidate.end(); ++candidateComponent, ++pathComponent) {
    if (pathComponent == path.end() || !pathComponentEquals(*candidateComponent, *pathComponent)) {
      return false;
    }
  }
  return true;
}

/** @brief Rejects CTF and Trace Compass targets that overlap unsafely. */
static void validateOutputTargets(const std::filesystem::path& ctfDirectory,
                                  const std::filesystem::path& traceCompassXml)
{
  requireOutputTarget(ctfDirectory, "CTF output directory");
  requireOutputTarget(traceCompassXml, "Trace Compass XML output");
  const auto normalizedCtf = normalizedAbsolutePath(ctfDirectory);
  const auto normalizedXml = normalizedAbsolutePath(traceCompassXml);
  if (isAncestorPath(normalizedCtf, normalizedXml) || isAncestorPath(normalizedXml, normalizedCtf)) {
    throw std::invalid_argument("CTF output directory and Trace Compass XML output must be separate, non-nested paths");
  }
}

/** @brief Removes an existing CTF directory without following unsafe targets. */
static void removeOutputDirectory(const std::filesystem::path& path)
{
  std::error_code error;
  std::filesystem::remove_all(path, error);
  if (error) {
    throw std::runtime_error("Failed to remove existing CTF output " + path.string() + ": " + error.message());
  }
}

/** @brief Verifies that existing output targets have replaceable filesystem types. */
static void validateExistingOutputTypes(const std::filesystem::path& ctfDirectory,
                                        const std::filesystem::path& traceCompassXml)
{
  std::error_code ctfError;
  const auto ctfStatus = std::filesystem::symlink_status(ctfDirectory, ctfError);
  if (ctfError && ctfError != std::errc::no_such_file_or_directory) {
    throw std::runtime_error("Failed to inspect existing CTF output " + ctfDirectory.string() + ": " +
                             ctfError.message());
  }
  if (!ctfError && std::filesystem::exists(ctfStatus) && !std::filesystem::is_directory(ctfStatus) &&
      !std::filesystem::is_symlink(ctfStatus)) {
    throw std::runtime_error("Refusing to replace CTF output because the target is not a directory: " +
                             ctfDirectory.string());
  }

  std::error_code xmlError;
  const auto xmlStatus = std::filesystem::symlink_status(traceCompassXml, xmlError);
  if (xmlError && xmlError != std::errc::no_such_file_or_directory) {
    throw std::runtime_error("Failed to inspect existing Trace Compass XML " + traceCompassXml.string() + ": " +
                             xmlError.message());
  }
  if (!xmlError && std::filesystem::is_directory(xmlStatus)) {
    throw std::runtime_error("Refusing to replace Trace Compass XML because the target is a directory: " +
                             traceCompassXml.string());
  }
}

/** @brief Removes an existing regular output file. */
static void removeOutputFile(const std::filesystem::path& path)
{
  std::error_code error;
  std::filesystem::remove(path, error);
  if (error) {
    throw std::runtime_error("Failed to remove existing Trace Compass XML " + path.string() + ": " + error.message());
  }
}

/** @brief Creates a prepared CTF output directory. */
static void createOutputDirectory(const std::filesystem::path& path)
{
  std::error_code error;
  std::filesystem::create_directories(path, error);
  if (error) {
    throw std::runtime_error("Failed to create CTF output directory " + path.string() + ": " + error.message());
  }
}

/** @brief Removes all partial artifacts after an aborted bundle. */
static void removeIncompleteOutputs(const std::filesystem::path& ctfDirectory,
                                    const std::filesystem::path& traceCompassXml)
{
  std::ostringstream errors;
  std::error_code ctfError;
  std::filesystem::remove_all(ctfDirectory, ctfError);
  if (ctfError) {
    errors << "CTF directory " << ctfDirectory.string() << ": " << ctfError.message();
  }

  std::error_code xmlError;
  std::filesystem::remove(traceCompassXml, xmlError);
  if (xmlError) {
    if (errors.tellp() > 0) {
      errors << "; ";
    }
    errors << "Trace Compass XML " << traceCompassXml.string() << ": " << xmlError.message();
  }
  if (errors.tellp() > 0) {
    throw std::runtime_error("Failed to remove incomplete CTF output: " + errors.str());
  }
}

CtfBundleOutput::CtfBundleOutput(CtfOutputConfig config, DiagnosticSink* diagnostics)
  : m_ctfOutputDirectory(std::move(config.outputDirectory)),
    m_traceCompassXmlPath(std::move(config.traceCompassXmlPath)),
    m_encoder(CtfEncoderConfig{
        config.coreClockHz,
        std::move(config.selection),
        std::move(config.sources),
        diagnostics,
    })
{
  validateOutputTargets(m_ctfOutputDirectory, m_traceCompassXmlPath);
}

CtfBundleOutput::~CtfBundleOutput()
{
  try {
    CtfBundleOutput::abort();
  } catch (...) {
    (void)0;
  }
}

std::string_view CtfBundleOutput::backendName() const noexcept
{
  return "ctf";
}

std::string CtfBundleOutput::targetPath() const
{
  return m_ctfOutputDirectory.string();
}

void CtfBundleOutput::start()
{
  abort();
  validateExistingOutputTypes(m_ctfOutputDirectory, m_traceCompassXmlPath);
  removeOutputDirectory(m_ctfOutputDirectory);
  removeOutputFile(m_traceCompassXmlPath);
  createOutputDirectory(m_ctfOutputDirectory);
  m_active = true;
  try {
    m_encoder.start(m_ctfOutputDirectory);
    TraceCompassXmlWriter::writeFile(m_traceCompassXmlPath);
  } catch (...) {
    abort();
    throw;
  }
}

void CtfBundleOutput::stop()
{
  if (!m_active) {
    return;
  }
  try {
    m_encoder.stop();
    m_active = false;
  } catch (...) {
    abort();
    throw;
  }
}

void CtfBundleOutput::abort()
{
  m_encoder.abort();
  if (m_active) {
    removeIncompleteOutputs(m_ctfOutputDirectory, m_traceCompassXmlPath);
    m_active = false;
  }
}

void CtfBundleOutput::writeEvent(const TraceEvent& event)
{
  m_encoder.writeEvent(event);
}
