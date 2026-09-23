/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceCompassXmlOutput.h"

#include "CtfMetadataModel.h"
#include "DiagnosticSink.h"
#include "OutputPath.h"

#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

/** @brief Validates an XML target without following or modifying an existing symlink. */
static void validateXmlTarget(const std::filesystem::path& path)
{
  OutputPath::validateParent(path, "Trace Compass XML output");
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (error && error != std::errc::no_such_file_or_directory) {
    throw std::runtime_error("Failed to inspect existing Trace Compass XML " + path.string() + ": " + error.message());
  }
  if (!error && std::filesystem::is_directory(status)) {
    throw std::runtime_error("Refusing to replace Trace Compass XML because the target is a directory: " +
                             path.string());
  }
}

/** @brief Removes an old XML file or symlink without modifying its destination. */
static void removeXmlFile(const std::filesystem::path& path)
{
  std::error_code error;
  std::filesystem::remove(path, error);
  if (error) {
    throw std::runtime_error("Failed to remove existing Trace Compass XML " + path.string() + ": " + error.message());
  }
}

/** @brief Selects only graphical topics backed by emitted records. */
static TraceCompassXmlWriter::ViewMask observedViews(const CtfMetadataModel& metadata,
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

/** @brief Builds visible, clock-qualified routes without changing the collected output. */
static std::vector<TraceCompassXmlWriter::ViewRoute> viewRoutes(std::string_view channel,
                                                               const CtfMetadataModel& metadata,
                                                               const CtfClockDomainDescriptor& clock)
{
  std::vector<TraceCompassXmlWriter::ViewRoute> routes;
  for (const auto& stream : metadata.topology().streams) {
    const auto views = observedViews(metadata, stream.streamClassId);
    if (views == 0U) {
      continue;
    }
    if (!clock.uuid.has_value()) {
      throw std::invalid_argument("Trace Compass XML requires an explicit clock UUID for input " + std::string(channel));
    }
    auto label = std::string(channel);
    if (stream.processorName.has_value() && !stream.processorName->empty()) {
      label += " - " + *stream.processorName;
    }
    routes.push_back({stream.route.traceBusId.value_or(0U), std::move(label), views, clock.uuid->toString()});
  }
  return routes;
}

TraceCompassXmlOutput::TraceCompassXmlOutput(std::filesystem::path path, DiagnosticSink& diagnostics)
  : m_path(std::move(path)), m_diagnostics(diagnostics)
{
  OutputPath::requireSpecificTarget(m_path, "Trace Compass XML output");
}

void TraceCompassXmlOutput::prepare()
{
  validateXmlTarget(m_path);
  removeXmlFile(m_path);
  m_routes.clear();
  m_clockUuids.clear();
}

void TraceCompassXmlOutput::add(std::string_view channel, const CtfMetadataModel& metadata)
{
  std::set<CtfClockDomainId> clocks;
  for (const auto& stream : metadata.topology().streams) {
    clocks.insert(stream.clockDomainId);
  }
  if (clocks.empty()) {
    return;
  }
  if (clocks.size() != 1U) {
    m_diagnostics.report({
        DiagnosticSink::Severity::Warning,
        "Trace Compass XML views were not generated for this input because emitted CTF streams use multiple clock domains",
        {{"backend", "ctf"}, {"path", m_path.string()}, {"inputChannel", std::string(channel)},
         {"clockDomains", std::to_string(clocks.size())}},
    });
    return;
  }
  // CtfMetadataModel validates that every stream references an existing clock.
  auto routes = viewRoutes(channel, metadata, *metadata.clockDomain(*clocks.begin()));
  if (routes.empty()) {
    return;
  }
  if (!m_clockUuids.insert(routes.front().clockUuid).second) {
    throw std::invalid_argument("Trace Compass XML requires independent clock UUIDs for separate inputs: " +
                                 std::string(channel));
  }
  m_routes.insert(m_routes.end(), std::make_move_iterator(routes.begin()), std::make_move_iterator(routes.end()));
}

void TraceCompassXmlOutput::finish()
{
  if (m_routes.empty()) {
    return;
  }
  validateXmlTarget(m_path);
  removeXmlFile(m_path);
  try {
    TraceCompassXmlWriter::writeFile(m_path, m_routes);
  } catch (...) {
    std::error_code ignored;
    std::filesystem::remove(m_path, ignored);
    throw;
  }
}
