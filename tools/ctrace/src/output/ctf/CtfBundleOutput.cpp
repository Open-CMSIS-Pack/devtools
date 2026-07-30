/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfBundleOutput.hpp"

#include "CtfEncoder.hpp"
#include "TraceCompassXmlWriter.hpp"
#include "TraceEvent.hpp"
#include "TraceOutputConfig.hpp"

#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <algorithm>
#include <cwctype>
#endif

namespace {

void requireOutputTarget(const std::filesystem::path& path, const char* description)
{
  const auto normalized = path.lexically_normal();
  if (path.empty() || normalized == normalized.root_path() || normalized.filename().empty() ||
      normalized.filename() == "." || normalized.filename() == "..") {
    throw std::invalid_argument(std::string(description) + " must identify a specific output path");
  }
}

std::filesystem::path normalizedAbsolutePath(const std::filesystem::path& path)
{
  std::error_code error;
  const auto absolute = std::filesystem::absolute(path, error);
  return (error ? path : absolute).lexically_normal();
}

bool pathComponentEquals(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
{
#ifdef _WIN32
  const auto& lhsNative = lhs.native();
  const auto& rhsNative = rhs.native();
  return lhsNative.size() == rhsNative.size() &&
         std::equal(lhsNative.begin(), lhsNative.end(), rhsNative.begin(),
                    [](wchar_t lhsCharacter, wchar_t rhsCharacter) {
                      return std::towlower(lhsCharacter) == std::towlower(rhsCharacter);
                    });
#else
  return lhs == rhs;
#endif
}

bool isAncestorPath(const std::filesystem::path& candidate, const std::filesystem::path& path)
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

void validateOutputTargets(const std::filesystem::path& ctfDirectory, const std::filesystem::path& traceCompassXml)
{
  requireOutputTarget(ctfDirectory, "CTF output directory");
  requireOutputTarget(traceCompassXml, "Trace Compass XML output");
  const auto normalizedCtf = normalizedAbsolutePath(ctfDirectory);
  const auto normalizedXml = normalizedAbsolutePath(traceCompassXml);
  if (isAncestorPath(normalizedCtf, normalizedXml) || isAncestorPath(normalizedXml, normalizedCtf)) {
    throw std::invalid_argument("CTF output directory and Trace Compass XML output must be separate, non-nested paths");
  }
}

void removeOutputDirectory(const std::filesystem::path& path)
{
  std::error_code error;
  std::filesystem::remove_all(path, error);
  if (error) {
    throw std::runtime_error("Failed to remove existing CTF output " + path.string() + ": " + error.message());
  }
}

void validateExistingOutputTypes(const std::filesystem::path& ctfDirectory,
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

void removeOutputFile(const std::filesystem::path& path)
{
  std::error_code error;
  std::filesystem::remove(path, error);
  if (error) {
    throw std::runtime_error("Failed to remove existing Trace Compass XML " + path.string() + ": " + error.message());
  }
}

void createOutputDirectory(const std::filesystem::path& path)
{
  std::error_code error;
  std::filesystem::create_directories(path, error);
  if (error) {
    throw std::runtime_error("Failed to create CTF output directory " + path.string() + ": " + error.message());
  }
}

void removeIncompleteOutputs(const std::filesystem::path& ctfDirectory, const std::filesystem::path& traceCompassXml)
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

} // namespace

CtfBundleOutput::CtfBundleOutput(CtfOutputConfig config, DiagnosticSink* diagnostics)
  : ctfOutputDirectory_(std::move(config.outputDirectory)), traceCompassXmlPath_(std::move(config.traceCompassXmlPath)),
    encoder_(CtfEncoderConfig{
        config.coreClockHz,
        std::move(config.selection),
        std::move(config.sources),
        diagnostics,
    })
{
  validateOutputTargets(ctfOutputDirectory_, traceCompassXmlPath_);
}

CtfBundleOutput::~CtfBundleOutput()
{
  try {
    abort();
  } catch (...) {
    // Destructors cannot report a best-effort cleanup failure.
    (void)0;
  }
}

std::string_view CtfBundleOutput::backendName() const noexcept
{
  return "ctf";
}

std::string CtfBundleOutput::targetPath() const
{
  return ctfOutputDirectory_.string();
}

void CtfBundleOutput::start()
{
  abort();
  validateExistingOutputTypes(ctfOutputDirectory_, traceCompassXmlPath_);
  removeOutputDirectory(ctfOutputDirectory_);
  removeOutputFile(traceCompassXmlPath_);
  createOutputDirectory(ctfOutputDirectory_);
  active_ = true;
  try {
    encoder_.start(ctfOutputDirectory_);
    TraceCompassXmlWriter::writeFile(traceCompassXmlPath_);
  } catch (...) {
    abort();
    throw;
  }
}

void CtfBundleOutput::stop()
{
  if (!active_) {
    return;
  }
  try {
    encoder_.stop();
    active_ = false;
  } catch (...) {
    abort();
    throw;
  }
}

void CtfBundleOutput::abort()
{
  encoder_.abort();
  if (active_) {
    removeIncompleteOutputs(ctfOutputDirectory_, traceCompassXmlPath_);
    active_ = false;
  }
}

void CtfBundleOutput::writeEvent(const TraceEvent& event)
{
  encoder_.writeEvent(event);
}
