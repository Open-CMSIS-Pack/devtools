/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "YmlTraceRunConfigReader.hpp"

#include "TraceRunConfig.hpp"
#include "yaml-cpp/exceptions.h"
#include "yaml-cpp/node/node.h"
#include "yaml-cpp/node/type.h"
#include "yaml-cpp/node/impl.h"    // IWYU pragma: keep
#include "yaml-cpp/node/convert.h" // IWYU pragma: keep
#include "yaml-cpp/node/iterator.h"
#include "yaml-cpp/node/detail/impl.h" // IWYU pragma: keep
#include "yaml-cpp/node/parse.h"

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

namespace {

using Node = YAML::Node;

std::size_t lineNumber(const Node& node)
{
  return node.Mark().line >= 0 ? static_cast<std::size_t>(node.Mark().line + 1) : 0U;
}

std::string errorMessage(const std::string& path, const Node& node, const std::string& message)
{
  std::ostringstream out;
  out << path;
  if (lineNumber(node) > 0U) {
    out << '(' << lineNumber(node) << ')';
  }
  out << ": " << message;
  return out.str();
}

[[noreturn]] void fail(const std::string& path, const Node& node, const std::string& message)
{
  throw std::runtime_error(errorMessage(path, node, message));
}

struct NodeLookup {
  Node value{YAML::NodeType::Undefined};
  Node duplicateKey{YAML::NodeType::Undefined};
};

NodeLookup lookupNode(const Node& element, std::string_view tag)
{
  NodeLookup result;
  if (!element.IsMap()) {
    return result;
  }
  for (const auto& entry : element) {
    if (!entry.first.IsScalar() || entry.first.Scalar() != tag) {
      continue;
    }
    if (result.value) {
      if (!result.duplicateKey) {
        result.duplicateKey = entry.first;
      }
      continue;
    }
    result.value = entry.second;
  }
  return result;
}

Node uniqueNode(const std::string& path, const Node& element, std::string_view tag)
{
  auto result = lookupNode(element, tag);
  if (result.duplicateKey) {
    fail(path, result.duplicateKey, "duplicate '" + std::string(tag) + "' node");
  }
  return result.value;
}

Node uniqueChild(const std::string& path, const Node& element, std::string_view tag)
{
  const auto node = uniqueNode(path, element, tag);
  return node && (node.IsMap() || node.IsSequence()) ? node : Node(YAML::NodeType::Undefined);
}

std::optional<std::string> optionalAttribute(const std::string& path, const Node& element, std::string_view name)
{
  const auto node = uniqueNode(path, element, name);
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

std::optional<std::string> processorNameAttribute(const std::string& path, const Node& element)
{
  const auto node = uniqueNode(path, element, "pname");
  if (!node || node.IsNull()) {
    return std::nullopt;
  }
  if (!node.IsScalar()) {
    fail(path, node, "'pname' must be a scalar string");
  }
  return TraceRunSchema::normalizedProcessorName(std::optional<std::string>(node.Scalar()));
}

std::optional<std::string> bestEffortProcessorName(const Node& element)
{
  const auto lookup = lookupNode(element, "pname");
  if (lookup.duplicateKey || !lookup.value || lookup.value.IsNull() || !lookup.value.IsScalar()) {
    return std::nullopt;
  }
  return TraceRunSchema::normalizedProcessorName(std::optional<std::string>(lookup.value.Scalar()));
}

std::uint64_t unsignedValue(const std::string& path, const Node& element, std::string_view name,
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

std::optional<std::uint64_t> optionalUnsignedAttribute(const std::string& path, const Node& element,
                                                       std::string_view name, std::uint64_t maximum)
{
  const auto value = optionalAttribute(path, element, name);
  if (!value.has_value()) {
    return std::nullopt;
  }
  return unsignedValue(path, element, name, *value, maximum);
}

std::optional<std::uint64_t> deferredUnsignedAttribute(const std::string& path, const Node& element,
                                                       std::string_view name, std::uint64_t maximum,
                                                       std::optional<std::string>& error)
{
  try {
    return optionalUnsignedAttribute(path, element, name, maximum);
  } catch (const std::runtime_error& ex) {
    error = ex.what();
    return std::nullopt;
  }
}

std::optional<std::uint64_t> bestEffortUnsignedAttribute(const std::string& path, const Node& element,
                                                         std::string_view name, std::uint64_t maximum)
{
  try {
    return optionalUnsignedAttribute(path, element, name, maximum);
  } catch (const std::runtime_error&) {
    return std::nullopt;
  }
}

// ctrace-ref identifies the originating ctrace.yml node. The data index links
// a generated DWT route to its copied ctrace-setup.data metadata.
std::optional<std::size_t> dwtDataIndex(std::string_view ctraceRef)
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

std::set<std::size_t> referencedDataSetupIndices(const std::vector<TraceRunReference>& references,
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

bool hasReferencedDataForProcessor(const std::vector<TraceRunReference>& references,
                                   const std::optional<std::string>& setupProcessorName)
{
  for (const auto& reference : references) {
    if (reference.type == "dwt" && TraceRunSchema::isUsableReference(reference) &&
        TraceRunSchema::processorNamesMayBind(setupProcessorName, reference.processorName)) {
      return true;
    }
  }
  return false;
}

bool setupDataMayBeConsumed(const Node& setup, const std::vector<TraceRunReference>& references)
{
  bool hasProcessorName = false;
  for (const auto& entry : setup) {
    if (!entry.first.IsScalar() || entry.first.Scalar() != "pname") {
      continue;
    }
    hasProcessorName = true;
    if (!entry.second.IsScalar() && !entry.second.IsNull()) {
      // It could name a referenced processor. Let strict parsing report
      // it when this setup contains otherwise consumed data.
      return true;
    }
    auto processorName = entry.second.IsScalar() ? std::optional<std::string>(entry.second.Scalar())
                                                 : std::optional<std::string>(std::string{});
    processorName = TraceRunSchema::normalizedProcessorName(std::move(processorName));
    if (hasReferencedDataForProcessor(references, processorName)) {
      return true;
    }
  }
  return !hasProcessorName && hasReferencedDataForProcessor(references, std::nullopt);
}

void requireSequence(const std::string& path, const Node& element, std::string_view name)
{
  if (!element.IsSequence()) {
    fail(path, element, "'" + std::string(name) + "' must be an array");
  }
}

Node traceRunRoot(const std::string& path, const Node& document)
{
  if (!document.IsMap()) {
    fail(path, document, "expected a YAML map containing 'ctrace-run'");
  }

  const auto root = uniqueNode(path, document, "ctrace-run");
  if (!root) {
    fail(path, document, "missing top-level 'ctrace-run' node");
  }
  if (!root.IsMap()) {
    fail(path, root, "top-level 'ctrace-run' node must be a map");
  }
  return root;
}

std::pair<std::vector<std::uint32_t>, bool> parseSources(const std::string& path, const Node& reference)
{
  const auto sourceNode = uniqueNode(path, reference, "source");
  if (!sourceNode) {
    return {{}, false};
  }
  if (sourceNode.IsScalar() || sourceNode.IsNull()) {
    const auto scalarSource = sourceNode.IsScalar() ? sourceNode.Scalar() : std::string{};
    return {{static_cast<std::uint32_t>(
                unsignedValue(path, sourceNode, "source", scalarSource, std::numeric_limits<std::uint32_t>::max()))},
            false};
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
  return {std::move(sources), true};
}

struct ReferenceDiagnostics {
  std::optional<std::string> info;
  std::optional<std::string> warning;
  std::optional<std::string> error;
};

ReferenceDiagnostics parseReferenceDiagnostics(const std::string& path, const Node& element)
{
  const auto message = [&](std::string_view name) {
    const auto value = optionalAttribute(path, element, name);
    if (uniqueChild(path, element, name)) {
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

std::optional<TraceRunReference> parseReference(const std::string& path, const Node& element)
{
  if (!element.IsMap()) {
    return std::nullopt;
  }

  const auto requiredScalar = [&](std::string_view name) {
    const auto node = uniqueNode(path, element, name);
    if (!node || !node.IsScalar() || node.Scalar().empty()) {
      fail(path, node ? node : element, "missing required '" + std::string(name) + "' scalar in ctrace-ref entry");
    }
    return node.Scalar();
  };

  const auto typeLookup = lookupNode(element, "type");
  const auto typeNode = typeLookup.value;
  if (typeLookup.duplicateKey || !typeNode || !typeNode.IsScalar() || typeNode.Scalar().empty()) {
    return std::nullopt;
  }

  TraceRunReference reference;
  reference.type = typeNode.Scalar();
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
    if (uniqueChild(path, element, "stream")) {
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
    auto parsedSources = parseSources(path, element);
    if (reference.type == "itm" && parsedSources.second) {
      fail(path, element, "ITM 'source' must be a single channel number");
    }
    reference.sources = std::move(parsedSources.first);
    if (reference.type == "dwt") {
      reference.symbolAddress =
          bestEffortUnsignedAttribute(path, element, "symbol-address", std::numeric_limits<std::uint64_t>::max());
    }
    reference.label = optionalAttribute(path, element, "label");
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

std::vector<TraceRunReference> parseReferences(const std::string& path, const Node& root)
{
  const auto referencesNode = uniqueNode(path, root, "ctrace-refs");
  if (!referencesNode) {
    return {};
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

TraceRunSetup parseSetup(const std::string& path, const Node& element, const std::vector<TraceRunReference>& references)
{
  if (!element.IsMap()) {
    return {};
  }

  TraceRunSetup setup;
  setup.line = lineNumber(element);
  setup.processorName = processorNameAttribute(path, element);
  const auto referencedDataIndices = referencedDataSetupIndices(references, setup.processorName);

  const auto timestampsLookup = lookupNode(element, "timestamps");
  const auto timestampsNode = timestampsLookup.value;
  if (timestampsLookup.duplicateKey) {
    setup.timestamps = TraceRunTimestampSetup{};
    setup.timestamps->line = lineNumber(timestampsLookup.duplicateKey);
    setup.timestamps->clockError = errorMessage(path, timestampsLookup.duplicateKey, "duplicate 'timestamps' setting");
  } else if (timestampsNode && (timestampsNode.IsScalar() || timestampsNode.IsNull())) {
    setup.timestamps = TraceRunTimestampSetup{};
    setup.timestamps->line = lineNumber(timestampsNode);
    if (timestampsNode.IsScalar() && !timestampsNode.Scalar().empty()) {
      setup.timestamps->clockError = "'timestamps' must be empty or a map";
    }
  } else if (timestampsNode) {
    if (!timestampsNode.IsMap()) {
      setup.timestamps = TraceRunTimestampSetup{};
      setup.timestamps->line = lineNumber(timestampsNode);
      setup.timestamps->clockError = "'timestamps' must be empty or a map";
    } else {
      TraceRunTimestampSetup timestamps;
      timestamps.line = lineNumber(timestampsNode);
      const auto clock = lookupNode(timestampsNode, "clock");
      if (clock.duplicateKey) {
        timestamps.clockError = errorMessage(path, clock.duplicateKey, "duplicate 'timestamps.clock' setting");
      } else if (clock.value && !clock.value.IsScalar() && !clock.value.IsNull()) {
        timestamps.clockError = "'timestamps.clock' must be a scalar unsigned integer";
      } else {
        timestamps.clockHz = deferredUnsignedAttribute(
            path, timestampsNode, "clock", std::numeric_limits<std::uint64_t>::max(), timestamps.clockError);
      }
      if (uniqueChild(path, timestampsNode, "itm-prescaler")) {
        fail(path, timestampsNode, "'timestamps.itm-prescaler' must be a scalar unsigned integer");
      }
      const auto prescaler =
          optionalUnsignedAttribute(path, timestampsNode, "itm-prescaler", std::numeric_limits<std::uint32_t>::max());
      if (prescaler.has_value()) {
        timestamps.timestampPrescaler = static_cast<std::uint32_t>(*prescaler);
      }
      setup.timestamps = std::move(timestamps);
    }
  }

  const auto itmNode = uniqueNode(path, element, "itm");
  if (itmNode) {
    if (!itmNode.IsMap()) {
      fail(path, itmNode, "'itm' must be a map containing 'enable'");
    }
    const auto enableNode = uniqueNode(path, itmNode, "enable");
    if (!enableNode || !enableNode.IsScalar() || enableNode.Scalar().empty()) {
      fail(path, enableNode ? enableNode : itmNode, "'itm.enable' is required and must be a scalar unsigned integer");
    }
    setup.itm = TraceRunItmSetup{static_cast<std::uint32_t>(
        unsignedValue(path, enableNode, "itm.enable", enableNode.Scalar(), std::numeric_limits<std::uint32_t>::max()))};
  }

  const auto dataLookup = referencedDataIndices.empty() ? NodeLookup{} : lookupNode(element, "data");
  if (dataLookup.duplicateKey) {
    return setup;
  }
  const auto dataNode = dataLookup.value;
  if (dataNode) {
    if (!dataNode.IsSequence()) {
      return setup;
    }
    std::size_t index = 0U;
    bool foundReferencedEntry = false;
    for (const auto& item : dataNode) {
      if (referencedDataIndices.find(index++) == referencedDataIndices.end()) {
        setup.data.emplace_back();
        continue;
      }
      foundReferencedEntry = true;
      TraceRunDataSetup data;
      if (!item.IsMap()) {
        setup.data.push_back(std::move(data));
        continue;
      }
      const auto type = lookupNode(item, "symbol-type");
      if (type.duplicateKey) {
        data.symbolTypeError = "duplicate 'data.symbol-type' setting";
      } else if (type.value && !type.value.IsScalar() && !type.value.IsNull()) {
        data.symbolTypeError = "'data.symbol-type' must be a scalar string";
      } else if (type.value && !type.value.IsNull()) {
        data.symbolType = optionalAttribute(path, item, "symbol-type");
      }
      const auto size = lookupNode(item, "symbol-size");
      if (size.duplicateKey) {
        data.symbolSizeError = "duplicate 'data.symbol-size' setting";
      } else if (size.value && !size.value.IsScalar() && !size.value.IsNull()) {
        data.symbolSizeError = "'data.symbol-size' must be a scalar unsigned integer";
      } else if (size.value && !size.value.IsNull()) {
        data.symbolSize = deferredUnsignedAttribute(path, item, "symbol-size",
                                                    std::numeric_limits<std::uint64_t>::max(), data.symbolSizeError);
      }
      setup.data.push_back(std::move(data));
    }
    if (!foundReferencedEntry) {
      setup.data.clear();
    }
  }
  return setup;
}

std::vector<TraceRunSetup> parseSetups(const std::string& path, const Node& root,
                                       const std::vector<TraceRunReference>& references)
{
  const auto setupLookup = lookupNode(root, "ctrace-setup");
  if (setupLookup.duplicateKey) {
    return {};
  }
  const auto setupNode = setupLookup.value;
  if (!setupNode) {
    return {};
  }
  if (!setupNode.IsSequence()) {
    return {};
  }

  std::vector<TraceRunSetup> setups;
  for (const auto& item : setupNode) {
    if (!item.IsMap()) {
      continue;
    }
    const auto timestamps = lookupNode(item, "timestamps");
    const auto itm = lookupNode(item, "itm");
    const auto data = lookupNode(item, "data");
    const auto consumesData = (data.value || data.duplicateKey) && setupDataMayBeConsumed(item, references);
    if (!timestamps.value && !timestamps.duplicateKey && !itm.value && !itm.duplicateKey && !consumesData) {
      continue;
    }
    // The copied ctrace.yml setup semantics define the presence of
    // 'disable' itself as sufficient to ignore the complete list entry.
    // Its value therefore has no schema that ctrace needs to validate.
    const auto disable = lookupNode(item, "disable");
    if (disable.value || disable.duplicateKey) {
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

} // namespace

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
