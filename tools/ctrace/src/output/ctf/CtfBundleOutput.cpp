/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfBundleOutput.h"

#include "CtfEncoder.h"
#include "CtfUuid.h"
#include "DiagnosticSink.h"
#include "TraceCompassXmlWriter.h"
#include "TraceEvent.h"
#include "TraceOutputConfig.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

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

/** @brief Rejects an output whose nearest existing parent is not a directory. */
static void validateOutputParent(const std::filesystem::path& path, const char* description)
{
  auto parent = normalizedAbsolutePath(path).parent_path();
  while (!parent.empty()) {
    const auto status = std::filesystem::status(parent);
    if (std::filesystem::exists(status)) {
      if (!std::filesystem::is_directory(status)) {
        throw std::runtime_error(std::string(description) + " parent is not a directory: " + parent.string());
      }
      return;
    }
    parent = parent.parent_path();
  }
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
  validateOutputParent(ctfDirectory, "CTF output");
  validateOutputParent(traceCompassXml, "Trace Compass XML output");

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

/** @brief Selects only graphical views backed by emitted records in one completed stream. */
static TraceCompassXmlWriter::ViewMask traceCompassViews(const CtfMetadataModel& metadata,
                                                         CtfStreamClassId streamClassId)
{
  auto views = TraceCompassXmlWriter::ViewMask{0U};
  for (const auto topic : kCtfGraphicalTopics) {
    if (metadata.observedGraphicalTopic(streamClassId, topic)) {
      views |= TraceCompassXmlWriter::viewMask(topic);
    }
  }
  return views;
}

/** @brief Counts the clock domains referenced by completed CTF streams. */
static std::size_t traceCompassClockDomainCount(const std::vector<CtfStreamDescriptor>& streams)
{
  std::set<CtfClockDomainId> clocks;
  for (const auto& stream : streams) {
    clocks.insert(stream.clockDomainId);
  }
  return clocks.size();
}

/** @brief Builds the Trace Compass view routes for completed CTF streams. */
static std::vector<TraceCompassXmlWriter::ViewRoute> traceCompassViewRoutes(const CtfMetadataModel& metadata)
{
  std::vector<TraceCompassXmlWriter::ViewRoute> viewRoutes;
  for (const auto& stream : metadata.topology().streams) {
    viewRoutes.push_back({
        stream.route.traceBusId.value_or(0U),
        stream.processorName.value_or(std::string{}),
        traceCompassViews(metadata, stream.streamClassId),
    });
  }
  return viewRoutes;
}

CtfBundleOutput::CtfBundleOutput(CtfOutputConfig config, DiagnosticSink* diagnostics)
  : m_ctfOutputDirectory(std::move(config.outputDirectory)),
    m_traceCompassXmlPath(std::move(config.traceCompassXmlPath)),
    m_encoder(CtfEncoderConfig{
        std::move(config.metadata),
        std::move(config.selection),
        diagnostics,
        std::move(config.routes),
    }),
    m_diagnostics(diagnostics)
{
  validateOutputTargets(m_ctfOutputDirectory, m_traceCompassXmlPath);
}

CtfBundleOutput::~CtfBundleOutput()
{
  abortNoexcept();
}

std::string_view CtfBundleOutput::backendName() const noexcept
{
  return "ctf";
}

std::string CtfBundleOutput::targetPath() const
{
  return m_ctfOutputDirectory.string();
}

void CtfBundleOutput::prepareOutput()
{
  validateExistingOutputTypes(m_ctfOutputDirectory, m_traceCompassXmlPath);
  removeOutputDirectory(m_ctfOutputDirectory);
  removeOutputFile(m_traceCompassXmlPath);
  createOutputDirectory(m_ctfOutputDirectory);
}

void CtfBundleOutput::startOutput()
{
  const auto traceUuid = CtfUuid::randomV4();
  m_encoder.start(m_ctfOutputDirectory, traceUuid);
}

void CtfBundleOutput::stopOutput()
{
  m_encoder.stop();
  const auto* metadata = m_encoder.completedMetadata();
  // A successful encoder stop always publishes its completed metadata model.
  assert(metadata != nullptr);
  finalizeTraceCompassXml(*metadata);
}

void CtfBundleOutput::finalizeTraceCompassXml(const CtfMetadataModel& metadata)
{
  const auto& streams = metadata.topology().streams;
  if (streams.empty()) {
    removeOutputFile(m_traceCompassXmlPath);
    return;
  }

  const auto clockDomainCount = traceCompassClockDomainCount(streams);
  if (clockDomainCount != 1U) {
    omitTraceCompassXml(clockDomainCount);
    return;
  }

  const auto viewRoutes = traceCompassViewRoutes(metadata);
  const auto hasViews = std::any_of(viewRoutes.begin(), viewRoutes.end(), [](const auto& route) {
    return route.views != 0U;
  });
  if (!hasViews) {
    removeOutputFile(m_traceCompassXmlPath);
    return;
  }

  if (metadata.isLegacySingleStreamLayout()) {
    TraceCompassXmlWriter::writeLegacyFile(m_traceCompassXmlPath, viewRoutes.front().views);
    return;
  }
  TraceCompassXmlWriter::writeRoutedFile(m_traceCompassXmlPath, viewRoutes);
}

void CtfBundleOutput::omitTraceCompassXml(std::size_t clockDomainCount)
{
  removeOutputFile(m_traceCompassXmlPath);
  if (m_diagnostics == nullptr) {
    return;
  }
  m_diagnostics->report({
      DiagnosticSink::Severity::Warning,
      "Trace Compass XML was not generated because emitted CTF streams use multiple clock domains",
      {
          {"backend", "ctf"},
          {"path", m_traceCompassXmlPath.string()},
          {"clockDomains", std::to_string(clockDomainCount)},
      },
  });
}

void CtfBundleOutput::abortOutput()
{
  m_encoder.abort();
  removeIncompleteOutputs(m_ctfOutputDirectory, m_traceCompassXmlPath);
}

void CtfBundleOutput::writeOutput(const TraceEvent& event)
{
  m_encoder.writeEvent(event);
}
