/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "YmlTraceRunConfigReader.h"

#include "TraceRunConfig.h"
#include "yaml-cpp/exceptions.h"
#include "yaml-cpp/node/node.h"
#include "yaml-cpp/node/parse.h"
#include "yaml-cpp/node/type.h"
#include "yaml-cpp/yaml.h" // IWYU pragma: keep

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

using Node = YAML::Node;

/** @brief Converts a YAML mark to a one-based line number. */
static std::size_t lineNumber(const Node& node)
{
  return node.Mark().line >= 0 ? static_cast<std::size_t>(node.Mark().line + 1) : 0U;
}

/** @brief Formats a YAML validation error with file and line. */
static std::string errorMessage(const std::string& path, const Node& node, const std::string& message)
{
  std::ostringstream out;
  out << path;
  if (lineNumber(node) > 0U) {
    out << '(' << lineNumber(node) << ')';
  }
  out << ": " << message;
  return out.str();
}

[[noreturn]] static void fail(const std::string& path, const Node& node, const std::string& message)
{
  throw std::runtime_error(errorMessage(path, node, message));
}

/** @brief Returns the child node for one scalar mapping key, if present. */
static Node childNode(const Node& element, const std::string_view& tag)
{
  for (const auto& entry : element) {
    if (!entry.first.IsScalar() || entry.first.Scalar() != tag) {
      continue;
    }
    return entry.second;
  }
  return Node(YAML::NodeType::Undefined);
}

/** @brief Returns a child mapping or sequence container, if present. */
static Node childContainer(const Node& element, const std::string_view& tag)
{
  const auto node = childNode(element, tag);
  return node && (node.IsMap() || node.IsSequence()) ? node : Node(YAML::NodeType::Undefined);
}

/** @brief Reads one optional scalar mapping value as text. */
static std::optional<std::string> optionalAttribute(const Node& element, const std::string_view& name)
{
  const auto node = childNode(element, name);
  if (!node) {
    return std::nullopt;
  }
  if (node.IsNull()) {
    return std::string{};
  }
  if (!node.IsScalar()) {
    return std::nullopt;
  }
  return node.Scalar();
}

/** @brief Reads and validates an optional processor name. */
static std::optional<std::string> processorNameAttribute(const std::string& path, const Node& element)
{
  const auto node = childNode(element, "pname");
  if (!node || node.IsNull()) {
    return std::nullopt;
  }
  if (!node.IsScalar()) {
    fail(path, node, "'pname' must be a scalar string");
  }
  return TraceRunSchema::normalizedProcessorName(std::optional<std::string>(node.Scalar()));
}

/** @brief Reads a processor name without making unrelated YAML fatal. */
static std::optional<std::string> bestEffortProcessorName(const Node& element)
{
  const auto node = childNode(element, "pname");
  if (!node || node.IsNull() || !node.IsScalar()) {
    return std::nullopt;
  }
  return TraceRunSchema::normalizedProcessorName(std::optional<std::string>(node.Scalar()));
}

/** @brief Parses and range-checks one required unsigned mapping value. */
static std::uint64_t unsignedValue(const std::string& path, const Node& element, const std::string_view& name,
                                   const std::string& value, std::uint64_t maximum)
{
  std::string_view digits(value);
  int base = 10;
  if (digits.size() >= 2U && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
    base = 16;
    digits.remove_prefix(2);
  }
  if (digits.empty()) {
    fail(path, element, "'" + std::string(name) + "' must be an unsigned integer");
  }

  std::uint64_t parsed = 0;
  const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), parsed, base);
  if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size() || parsed > maximum) {
    fail(path, element, "'" + std::string(name) + "' must be an unsigned integer in range");
  }
  return parsed;
}

/** @brief Parses and range-checks one optional unsigned mapping value. */
static std::optional<std::uint64_t> optionalUnsignedAttribute(const std::string& path, const Node& element,
                                                              const std::string_view& name, std::uint64_t maximum)
{
  const auto value = optionalAttribute(element, name);
  if (!value.has_value()) {
    return std::nullopt;
  }
  return unsignedValue(path, element, name, *value, maximum);
}

/** @brief Parses an optional unsigned value while deferring validation errors. */
static std::optional<std::uint64_t> deferredUnsignedAttribute(const std::string& path, const Node& element,
                                                              const std::string_view& name, std::uint64_t maximum,
                                                              std::optional<std::string>& error)
{
  try {
    return optionalUnsignedAttribute(path, element, name, maximum);
  } catch (const std::runtime_error& ex) {
    error = ex.what();
    return std::nullopt;
  }
}

/** @brief Parses an optional unsigned value only when its setup is consumed. */
static std::optional<std::uint64_t> bestEffortUnsignedAttribute(const std::string& path, const Node& element,
                                                                const std::string_view& name, std::uint64_t maximum)
{
  try {
    return optionalUnsignedAttribute(path, element, name, maximum);
  } catch (const std::runtime_error&) {
    return std::nullopt;
  }
}

// ctrace-ref identifies the originating ctrace.yml node. The data index links
// a generated DWT route to its copied ctrace-setup.data metadata.
/** @brief Extracts a DWT data setup index from a ctrace reference path. */
static std::optional<std::size_t> dwtDataIndex(const std::string_view& ctraceRef)
{
  constexpr std::string_view marker = "data#";
  const auto markerPosition = ctraceRef.rfind(marker);
  if (markerPosition == std::string_view::npos || (markerPosition != 0U && ctraceRef[markerPosition - 1U] != '/')) {
    return std::nullopt;
  }
  const auto value = ctraceRef.substr(markerPosition + marker.size());
  if (value.empty()) {
    return std::nullopt;
  }
  std::size_t index = 0U;
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), index);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
    return std::nullopt;
  }
  return index;
}

/** @brief Collects data setup indices consumed by matching references. */
static std::set<std::size_t> referencedDataSetupIndices(const std::vector<TraceRunReference>& references,
                                                        const std::optional<std::string>& setupProcessorName)
{
  std::set<std::size_t> indices;
  for (const auto& reference : references) {
    if (reference.type != "dwt" || !TraceRunSchema::isUsableReference(reference)) {
      continue;
    }
    if (!TraceRunSchema::processorNamesMayBind(setupProcessorName, reference.processorName)) {
      continue;
    }
    const auto index = reference.dataSetupIndex;
    if (index.has_value()) {
      indices.insert(*index);
    }
  }
  return indices;
}

/** @brief Requires one YAML node to be a sequence. */
static void requireSequence(const std::string& path, const Node& element, const std::string_view& name)
{
  if (!element.IsSequence()) {
    fail(path, element, "'" + std::string(name) + "' must be an array");
  }
}

/** @brief Locates and validates the root trace-run mapping. */
static Node traceRunRoot(const std::string& path, const Node& document)
{
  if (!document.IsMap()) {
    fail(path, document, "expected a YAML map containing 'ctrace-run'");
  }

  const auto root = childNode(document, "ctrace-run");
  if (!root) {
    fail(path, document, "missing top-level 'ctrace-run' node");
  }
  if (!root.IsMap()) {
    fail(path, root, "top-level 'ctrace-run' node must be a map");
  }
  return root;
}

/** @brief Parses scalar or sequence source identifiers from one reference. */
static std::vector<std::uint32_t> parseSources(const std::string& path, const Node& reference)
{
  const auto sourceNode = childNode(reference, "source");
  if (!sourceNode) {
    return {};
  }
  if (sourceNode.IsScalar() || sourceNode.IsNull()) {
    const auto scalarSource = sourceNode.IsScalar() ? sourceNode.Scalar() : std::string{};
    return {static_cast<std::uint32_t>(
        unsignedValue(path, sourceNode, "source", scalarSource, std::numeric_limits<std::uint32_t>::max()))};
  }

  requireSequence(path, sourceNode, "source");
  std::vector<std::uint32_t> sources;
  for (const auto& item : sourceNode) {
    if (!item.IsScalar() || item.Scalar().empty()) {
      fail(path, item, "each 'source' entry must be an unsigned integer");
    }
    const auto source = static_cast<std::uint32_t>(
        unsignedValue(path, item, "source", item.Scalar(), std::numeric_limits<std::uint32_t>::max()));
    sources.push_back(source);
  }
  return sources;
}

/** @brief Stores diagnostics copied from one parsed trace reference. */
struct ReferenceDiagnostics {
  std::optional<std::string> info;
  std::optional<std::string> warning;
  std::optional<std::string> error;
};

/** @brief Parses optional informational diagnostics attached to a reference. */
static ReferenceDiagnostics parseReferenceDiagnostics(const std::string& path, const Node& element)
{
  const auto message = [&](const std::string_view& name) {
    const auto value = optionalAttribute(element, name);
    if (childContainer(element, name)) {
      fail(path, element, "'" + std::string(name) + "' must be a scalar message");
    }
    return value;
  };
  return {
      message("info"),
      message("warning"),
      message("error"),
  };
}

/** @brief Parses one relevant trace-run reference. */
static std::optional<TraceRunReference> parseReference(const std::string& path, const Node& element)
{
  if (!element.IsMap()) {
    fail(path, element, "each 'ctrace-refs' entry must be a map");
  }

  const auto requiredScalar = [&](const std::string_view& name) {
    const auto node = childNode(element, name);
    if (!node || !node.IsScalar() || node.Scalar().empty()) {
      fail(path, node ? node : element, "missing required '" + std::string(name) + "' scalar in ctrace-ref entry");
    }
    return node.Scalar();
  };

  TraceRunReference reference;
  reference.type = requiredScalar("type");
  if (!TraceRunSchema::consumesReferenceMetadata(reference.type)) {
    return std::nullopt;
  }
  reference.ctraceRef = requiredScalar("ctrace-ref");
  reference.line = lineNumber(element);
  if (reference.type == "dwt") {
    reference.dataSetupIndex = dwtDataIndex(reference.ctraceRef);
  }

  const auto diagnostics = parseReferenceDiagnostics(path, element);
  reference.info = diagnostics.info;
  reference.warning = diagnostics.warning;
  reference.error = diagnostics.error;

  const auto parseStream = [&]() -> std::optional<std::uint32_t> {
    if (childContainer(element, "stream")) {
      fail(path, element, "'stream' must be a scalar unsigned integer");
    }
    const auto stream = optionalUnsignedAttribute(path, element, "stream", std::numeric_limits<std::uint32_t>::max());
    if (!stream.has_value()) {
      return std::nullopt;
    }
    return static_cast<std::uint32_t>(*stream);
  };

  const auto parseRoute = [&]() {
    reference.processorName = processorNameAttribute(path, element);
    reference.stream = parseStream();
    reference.sources = parseSources(path, element);
    if (reference.type == "dwt") {
      reference.symbolAddress =
          bestEffortUnsignedAttribute(path, element, "symbol-address", std::numeric_limits<std::uint64_t>::max());
    }
    reference.label = optionalAttribute(element, "label");
  };

  if (!TraceRunSchema::supportsSource(reference.type)) {
    // These source types currently contribute diagnostics only. Preserve
    // a valid processor name for log context, but do not validate fields
    // that no ctrace decoder or output consumes.
    reference.processorName = bestEffortProcessorName(element);
    return reference;
  }

  if (diagnostics.error.has_value()) {
    auto diagnosticReference = reference;
    diagnosticReference.processorName = bestEffortProcessorName(element);
    try {
      parseRoute();
      if (TraceRunSchema::isUsableReference(reference) || TraceRunSchema::isItmChannelZero(reference)) {
        return reference;
      }
      return diagnosticReference;
    } catch (const std::runtime_error&) {
      return diagnosticReference;
    }
  }

  parseRoute();
  return reference;
}

/** @brief Parses every consumed reference from the trace-run root. */
static std::vector<TraceRunReference> parseReferences(const std::string& path, const Node& root)
{
  const auto referencesNode = childNode(root, "ctrace-refs");
  if (!referencesNode) {
    fail(path, root, "missing required 'ctrace-refs' array");
  }
  requireSequence(path, referencesNode, "ctrace-refs");

  std::vector<TraceRunReference> references;
  for (const auto& item : referencesNode) {
    auto reference = parseReference(path, item);
    if (reference.has_value()) {
      references.push_back(std::move(*reference));
    }
  }
  return references;
}

/** @brief Parses timestamp metadata from one consumed setup. */
static std::optional<TraceRunTimestampSetup> parseTimestampSetup(const std::string& path, const Node& element)
{
  const auto timestampsNode = childNode(element, "timestamps");
  if (!timestampsNode) {
    return std::nullopt;
  }
  if (timestampsNode.IsScalar() || timestampsNode.IsNull()) {
    TraceRunTimestampSetup timestamps;
    timestamps.line = lineNumber(timestampsNode);
    if (timestampsNode.IsScalar() && !timestampsNode.Scalar().empty()) {
      timestamps.clockError = "'timestamps' must be empty or a map";
    }
    return timestamps;
  }
  if (!timestampsNode.IsMap()) {
    TraceRunTimestampSetup timestamps;
    timestamps.line = lineNumber(timestampsNode);
    timestamps.clockError = "'timestamps' must be empty or a map";
    return timestamps;
  }

  TraceRunTimestampSetup timestamps;
  timestamps.line = lineNumber(timestampsNode);
  const auto clock = childNode(timestampsNode, "clock");
  if (clock && !clock.IsScalar() && !clock.IsNull()) {
    timestamps.clockError = "'timestamps.clock' must be a scalar unsigned integer";
  } else {
    timestamps.clockHz = deferredUnsignedAttribute(path, timestampsNode, "clock",
                                                   std::numeric_limits<std::uint64_t>::max(), timestamps.clockError);
  }
  if (childContainer(timestampsNode, "itm-prescaler")) {
    fail(path, timestampsNode, "'timestamps.itm-prescaler' must be a scalar unsigned integer");
  }
  const auto prescaler =
      optionalUnsignedAttribute(path, timestampsNode, "itm-prescaler", std::numeric_limits<std::uint32_t>::max());
  if (prescaler.has_value()) {
    timestamps.timestampPrescaler = static_cast<std::uint32_t>(*prescaler);
  }
  return timestamps;
}

/** @brief Parses ITM enable-mask metadata from one consumed setup. */
static std::optional<TraceRunItmSetup> parseItmSetup(const std::string& path, const Node& element)
{
  const auto itmNode = childNode(element, "itm");
  if (!itmNode) {
    return std::nullopt;
  }
  if (!itmNode.IsMap()) {
    fail(path, itmNode, "'itm' must be a map containing 'enable'");
  }
  const auto enableNode = childNode(itmNode, "enable");
  if (!enableNode || !enableNode.IsScalar() || enableNode.Scalar().empty()) {
    fail(path, enableNode ? enableNode : itmNode, "'itm.enable' is required and must be a scalar unsigned integer");
  }
  return TraceRunItmSetup{static_cast<std::uint32_t>(
      unsignedValue(path, enableNode, "itm.enable", enableNode.Scalar(), std::numeric_limits<std::uint32_t>::max()))};
}

/** @brief Parses only DWT data setups referenced by consumed routes. */
static std::vector<TraceRunDataSetup> parseReferencedDataSetups(const std::string& path, const Node& element,
                                                                const std::set<std::size_t>& referencedIndices)
{
  const auto dataNode =
      referencedIndices.empty() ? Node(YAML::NodeType::Undefined) : childNode(element, "data");
  if (!dataNode || !dataNode.IsSequence()) {
    return {};
  }

  std::vector<TraceRunDataSetup> dataSetups;
  std::size_t index = 0U;
  bool foundReferencedEntry = false;
  for (const auto& item : dataNode) {
    if (referencedIndices.find(index++) == referencedIndices.end()) {
      dataSetups.emplace_back();
      continue;
    }
    foundReferencedEntry = true;
    TraceRunDataSetup data;
    if (!item.IsMap()) {
      dataSetups.push_back(std::move(data));
      continue;
    }
    const auto type = childNode(item, "symbol-type");
    if (type && !type.IsScalar() && !type.IsNull()) {
      data.symbolTypeError = "'data.symbol-type' must be a scalar string";
    } else if (type && !type.IsNull()) {
      data.symbolType = optionalAttribute(item, "symbol-type");
    }
    const auto size = childNode(item, "symbol-size");
    if (size && !size.IsScalar() && !size.IsNull()) {
      data.symbolSizeError = "'data.symbol-size' must be a scalar unsigned integer";
    } else if (size && !size.IsNull()) {
      data.symbolSize = deferredUnsignedAttribute(path, item, "symbol-size", std::numeric_limits<std::uint64_t>::max(),
                                                  data.symbolSizeError);
    }
    dataSetups.push_back(std::move(data));
  }
  return foundReferencedEntry ? dataSetups : std::vector<TraceRunDataSetup>{};
}

/** @brief Parses one trace setup and its consumed metadata groups. */
static TraceRunSetup parseSetup(const std::string& path, const Node& element,
                                const std::vector<TraceRunReference>& references)
{
  TraceRunSetup setup;
  setup.line = lineNumber(element);
  setup.processorName = processorNameAttribute(path, element);
  const auto referencedDataIndices = referencedDataSetupIndices(references, setup.processorName);
  setup.timestamps = parseTimestampSetup(path, element);
  setup.itm = parseItmSetup(path, element);
  setup.data = parseReferencedDataSetups(path, element, referencedDataIndices);
  return setup;
}

/** @brief Parses all trace setups referenced by relevant routes. */
static std::vector<TraceRunSetup> parseSetups(const std::string& path, const Node& root,
                                              const std::vector<TraceRunReference>& references)
{
  const auto setupNode = childNode(root, "ctrace-setup");
  if (!setupNode) {
    return {};
  }
  requireSequence(path, setupNode, "ctrace-setup");

  std::vector<TraceRunSetup> setups;
  for (const auto& item : setupNode) {
    if (!item.IsMap()) {
      fail(path, item, "each 'ctrace-setup' entry must be a map");
    }
    // The copied ctrace.yml setup semantics define the presence of
    // 'disable' itself as sufficient to ignore the complete list entry.
    // Its value therefore has no schema that ctrace needs to validate.
    const auto disable = childNode(item, "disable");
    if (disable) {
      continue;
    }
    auto setup = parseSetup(path, item, references);
    if (!setup.timestamps.has_value() && !setup.itm.has_value() && setup.data.empty()) {
      continue;
    }
    setups.push_back(std::move(setup));
  }
  return setups;
}

TraceRunConfig YmlTraceRunConfigReader::read(const std::string& path) const
{
  if (path.empty()) {
    throw std::runtime_error("trace-run configuration path is empty");
  }

  std::vector<Node> documents;
  try {
    documents = YAML::LoadAllFromFile(path);
  } catch (const YAML::Exception& error) {
    std::ostringstream message;
    message << "failed to parse trace-run configuration: " << path;
    if (error.mark.line >= 0) {
      message << '(' << (error.mark.line + 1);
      if (error.mark.column >= 0) {
        message << ',' << (error.mark.column + 1);
      }
      message << ')';
    }
    message << ": " << error.msg;
    throw std::runtime_error(message.str());
  }
  if (documents.size() != 1U) {
    const auto location = documents.size() > 1U ? documents[1] : Node(YAML::NodeType::Undefined);
    fail(path, location, "expected exactly one YAML document");
  }

  const auto root = traceRunRoot(path, documents.front());

  TraceRunConfig config;
  config.path = path;
  config.references = parseReferences(path, root);
  config.setups = parseSetups(path, root, config.references);
  return config;
}
