/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfBundleOutput.h"

#include "DiagnosticMessages.h"
#include "CtfUuid.h"
#include "OutputPath.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

/** @brief Verifies that an existing CTF target can safely be replaced. */
static void validateExistingOutput(const std::filesystem::path& directory)
{
  OutputPath::validateParent(directory, "CTF output");
  std::error_code error;
  const auto status = std::filesystem::symlink_status(directory, error);
  if (error && error != std::errc::no_such_file_or_directory) {
    throw std::runtime_error(
        pathDiagnosticMessage(PathDiagnosticCode::CtfInspect, directory.string(), error.message()));
  }
  if (!error && std::filesystem::exists(status) && !std::filesystem::is_directory(status) &&
      !std::filesystem::is_symlink(status)) {
    throw std::runtime_error(pathDiagnosticMessage(PathDiagnosticCode::CtfRefuseFile, directory.string()));
  }
}

/** @brief Removes only the selected CTF directory, without following a target symlink. */
static void removeOutputDirectory(const std::filesystem::path& directory)
{
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  if (error) {
    throw std::runtime_error(pathDiagnosticMessage(PathDiagnosticCode::CtfRemove, directory.string(), error.message()));
  }
}

CtfBundleOutput::CtfBundleOutput(CtfOutputConfig config, DiagnosticSink* diagnostics)
  : m_ctfOutputDirectory(std::move(config.outputDirectory)),
    m_encoder(CtfEncoderConfig{
        std::move(config.metadata),
        std::move(config.selection),
        diagnostics,
        std::move(config.routes),
    })
{
  OutputPath::requireSpecificTarget(m_ctfOutputDirectory, "CTF output directory");
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

const CtfMetadataModel* CtfBundleOutput::completedMetadata() const noexcept
{
  return m_encoder.completedMetadata();
}

void CtfBundleOutput::prepareOutput()
{
  validateExistingOutput(m_ctfOutputDirectory);
  removeOutputDirectory(m_ctfOutputDirectory);
  std::error_code error;
  std::filesystem::create_directories(m_ctfOutputDirectory, error);
  if (error) {
    throw std::runtime_error(
        pathDiagnosticMessage(PathDiagnosticCode::CtfCreateDirectory, m_ctfOutputDirectory.string(), error.message()));
  }
}

void CtfBundleOutput::startOutput()
{
  m_encoder.start(m_ctfOutputDirectory, CtfUuid::randomV4());
}

void CtfBundleOutput::stopOutput()
{
  m_encoder.stop();
}

void CtfBundleOutput::abortOutput()
{
  m_encoder.abort();
  removeOutputDirectory(m_ctfOutputDirectory);
}

void CtfBundleOutput::writeOutput(const TraceEvent& event)
{
  m_encoder.writeEvent(event);
}
