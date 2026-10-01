/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_MODEL_DIAGNOSTICMESSAGES_H
#define CTRACE_SRC_MODEL_DIAGNOSTICMESSAGES_H

#include "Messages.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/** @brief Retains opaque diagnostic detail and adds an optional one-based source line. */
std::string locatedDiagnosticMessage(std::string_view path, std::size_t line, std::string_view detail);

/** @brief Formats the normalized-route invariant using the internal numeric route ID. */
std::string unknownNormalizedRouteMessage(std::uint64_t routeId);

/** @brief Formats command-line validation with its explicitly named arguments. */
std::string unsignedArgumentMessage(std::string_view option, std::string_view value);
std::string missingArgumentValueMessage(std::string_view option);
std::string argumentValueSeparatorMessage(std::string_view option);
std::string unknownArgumentMessage(std::string_view argument);
std::string positionalArgumentMessage(std::string_view argument);
std::string streamArgumentMessage(std::string_view value);
std::string typeArgumentMessage(std::string_view value, std::string_view accepted);
std::string targetArgumentMessage(std::string_view value);

/** @brief Selects the validation applied to one named YAML field. */
enum class ConfigFieldProblem {
  ScalarString, ScalarUnsigned, Unsigned, UnsignedRange, Array, Scalar,
  StringOrList, StringEntry, MapEntry, UnsignedEntry, RequiredReferenceScalar,
};
std::string configFieldMessage(std::string_view field, ConfigFieldProblem problem);
std::string yamlParseMessage(std::string_view path, std::optional<std::size_t> line,
                             std::optional<std::size_t> column, std::string_view nativeDetail);

enum class ProcessorBindingProblem { IgnoredUnmatchedSetup, ConflictingReferencePath, NoActiveSetup, MultipleBusIds };
std::string processorBindingMessage(ProcessorBindingProblem problem, std::string_view name);
std::string ignoredProcessorMismatchMessage(std::string_view referenceName, std::string_view setupName);
std::string disabledReferenceMessage(std::string_view reference, std::size_t ordinal);
enum class TraceBusBindingProblem { ConflictingProcessors, MissingAnchor };
std::string traceBusBindingMessage(TraceBusBindingProblem problem, std::uint32_t traceBusId);

/** @brief Selects the operation whose detailed diagnostic contains a path or input name. */
enum class PathDiagnosticCode {
  TraceConfigMissing, TraceDirectoryMissing, RawInputMissing, RawInputNotRegular,
  RawInputUnreadable, RawInputReadFailed, ArtifactNamesInvalid,
  CsvInspect, CsvRefuseDirectory, CsvRemove, CsvCreateDirectory, CsvOpen, CsvWrite,
  CtfInspect, CtfRefuseFile, CtfRemove, CtfCreateDirectory, CtfStreamOpen, CtfStreamWrite, CtfMetadataWrite,
  XmlInspect, XmlRefuseDirectory, XmlRemove, XmlWrite, XmlClockUuidMissing, XmlClockUuidShared,
};
/** @brief Preserves native OS detail as an opaque optional suffix. */
std::string pathDiagnosticMessage(PathDiagnosticCode code, std::string_view path,
                                  std::optional<std::string_view> nativeDetail = std::nullopt);
std::string configurationFilesMissingMessage(std::string_view suffix, std::string_view directory);
std::string configurationFilenameMessage(std::string_view suffix, std::string_view path);
std::string rawInputAlignmentMessage(std::uint64_t alignment, std::string_view path, std::uint64_t size);
std::string specificOutputTargetMessage(std::string_view description);
std::string outputParentMessage(std::string_view description, std::string_view parent);

std::string decodeSummaryMessage(std::uint64_t bytes, double seconds, std::uint64_t records);
std::string backendFailureMessage(std::string_view backend, std::string_view target,
                                  std::string_view phase, std::string_view detail);
std::string ctfDataTypeMessage(std::string_view dataType);
std::string ctfDataSizeMessage(std::uint64_t size, std::string_view dataType);

#endif // CTRACE_SRC_MODEL_DIAGNOSTICMESSAGES_H
