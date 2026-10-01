/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "DiagnosticMessages.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

std::string locatedDiagnosticMessage(std::string_view path, std::size_t line, std::string_view detail)
{
  return line > 0U ? formatMessage(MessageId::DiagnosticLineLocation, MessageStyle::Detailed, {path, line, detail})
                   : formatMessage(MessageId::DiagnosticLocation, MessageStyle::Detailed, {path, detail});
}

std::string unknownNormalizedRouteMessage(std::uint64_t routeId)
{
  return formatMessage(MessageId::DiagnosticUnknownNormalizedRoute, MessageStyle::Detailed, {routeId});
}

std::string unsignedArgumentMessage(std::string_view option, std::string_view value)
{
  return formatMessage(MessageId::DiagnosticUnsignedArgument, MessageStyle::Detailed, {option, value});
}

std::string missingArgumentValueMessage(std::string_view option)
{
  return formatMessage(MessageId::DiagnosticMissingArgumentValue, MessageStyle::Detailed, {option});
}

std::string argumentValueSeparatorMessage(std::string_view option)
{
  return formatMessage(MessageId::DiagnosticArgumentValueSeparator, MessageStyle::Detailed, {option});
}

std::string unknownArgumentMessage(std::string_view argument)
{
  return formatMessage(MessageId::DiagnosticUnknownArgument, MessageStyle::Detailed, {argument});
}

std::string positionalArgumentMessage(std::string_view argument)
{
  return formatMessage(MessageId::DiagnosticPositionalArgument, MessageStyle::Detailed, {argument});
}

std::string streamArgumentMessage(std::string_view value)
{
  return formatMessage(MessageId::DiagnosticStreamArgument, MessageStyle::Detailed, {value});
}

std::string typeArgumentMessage(std::string_view value, std::string_view accepted)
{
  return formatMessage(MessageId::DiagnosticTypeArgument, MessageStyle::Detailed, {value, accepted});
}

std::string targetArgumentMessage(std::string_view value)
{
  return formatMessage(MessageId::DiagnosticTargetArgument, MessageStyle::Detailed, {value});
}

std::string configFieldMessage(std::string_view field, ConfigFieldProblem problem)
{
  switch (problem) {
  case ConfigFieldProblem::ScalarString:
    return formatMessage(MessageId::DiagnosticConfigFieldScalarString, MessageStyle::Detailed, {field});
  case ConfigFieldProblem::ScalarUnsigned:
    return formatMessage(MessageId::DiagnosticConfigFieldScalarUnsigned, MessageStyle::Detailed, {field});
  case ConfigFieldProblem::Unsigned:
    return formatMessage(MessageId::DiagnosticConfigFieldUnsigned, MessageStyle::Detailed, {field});
  case ConfigFieldProblem::UnsignedRange:
    return formatMessage(MessageId::DiagnosticConfigFieldUnsignedRange, MessageStyle::Detailed, {field});
  case ConfigFieldProblem::Array:
    return formatMessage(MessageId::DiagnosticConfigFieldArray, MessageStyle::Detailed, {field});
  case ConfigFieldProblem::Scalar:
    return formatMessage(MessageId::DiagnosticConfigFieldScalar, MessageStyle::Detailed, {field});
  case ConfigFieldProblem::StringOrList:
    return formatMessage(MessageId::DiagnosticConfigFieldStringOrList, MessageStyle::Detailed, {field});
  case ConfigFieldProblem::StringEntry:
    return formatMessage(MessageId::DiagnosticConfigFieldStringEntry, MessageStyle::Detailed, {field});
  case ConfigFieldProblem::MapEntry:
    return formatMessage(MessageId::DiagnosticConfigFieldMapEntry, MessageStyle::Detailed, {field});
  case ConfigFieldProblem::UnsignedEntry:
    return formatMessage(MessageId::DiagnosticConfigFieldUnsignedEntry, MessageStyle::Detailed, {field});
  case ConfigFieldProblem::RequiredReferenceScalar:
    return formatMessage(MessageId::DiagnosticConfigFieldRequiredReferenceScalar, MessageStyle::Detailed, {field});
  }
  throw std::logic_error(formatMessage(MessageId::DiagnosticInvalidConfigFieldProblem));
}

std::string yamlParseMessage(std::string_view path, std::optional<std::size_t> line,
                             std::optional<std::size_t> column, std::string_view nativeDetail)
{
  if (!line.has_value()) {
    return formatMessage(MessageId::DiagnosticYamlParse, MessageStyle::Detailed, {path, nativeDetail});
  }
  return column.has_value()
             ? formatMessage(MessageId::DiagnosticYamlParseColumn, MessageStyle::Detailed,
                             {path, *line, *column, nativeDetail})
             : formatMessage(MessageId::DiagnosticYamlParseLine, MessageStyle::Detailed, {path, *line, nativeDetail});
}

std::string processorBindingMessage(ProcessorBindingProblem problem, std::string_view name)
{
  switch (problem) {
  case ProcessorBindingProblem::IgnoredUnmatchedSetup:
    return formatMessage(MessageId::DiagnosticProcessorIgnoredUnmatchedSetup, MessageStyle::Detailed, {name});
  case ProcessorBindingProblem::ConflictingReferencePath:
    return formatMessage(MessageId::DiagnosticProcessorConflictingReferencePath, MessageStyle::Detailed, {name});
  case ProcessorBindingProblem::NoActiveSetup:
    return formatMessage(MessageId::DiagnosticProcessorNoActiveSetup, MessageStyle::Detailed, {name});
  case ProcessorBindingProblem::MultipleBusIds:
    return formatMessage(MessageId::DiagnosticProcessorMultipleBusIds, MessageStyle::Detailed, {name});
  }
  throw std::logic_error(formatMessage(MessageId::DiagnosticInvalidProcessorBindingProblem));
}

std::string ignoredProcessorMismatchMessage(std::string_view referenceName, std::string_view setupName)
{
  return formatMessage(MessageId::DiagnosticIgnoredProcessorMismatch, MessageStyle::Detailed,
                       {referenceName, setupName});
}

std::string disabledReferenceMessage(std::string_view reference, std::size_t ordinal)
{
  return formatMessage(MessageId::DiagnosticDisabledReference, MessageStyle::Detailed, {reference, ordinal});
}

std::string traceBusBindingMessage(TraceBusBindingProblem problem, std::uint32_t traceBusId)
{
  switch (problem) {
  case TraceBusBindingProblem::ConflictingProcessors:
    return formatMessage(MessageId::DiagnosticTraceBusConflictingProcessors, MessageStyle::Detailed, {traceBusId});
  case TraceBusBindingProblem::MissingAnchor:
    return formatMessage(MessageId::DiagnosticTraceBusMissingAnchor, MessageStyle::Detailed, {traceBusId});
  }
  throw std::logic_error(formatMessage(MessageId::DiagnosticInvalidTraceBusBindingProblem));
}

/** @brief Maps a filesystem operation to the common message catalogue. */
static MessageId pathMessageId(PathDiagnosticCode code)
{
  switch (code) {
  case PathDiagnosticCode::TraceConfigMissing: return MessageId::DiagnosticPathTraceConfigMissing;
  case PathDiagnosticCode::TraceDirectoryMissing: return MessageId::DiagnosticPathTraceDirectoryMissing;
  case PathDiagnosticCode::RawInputMissing: return MessageId::DiagnosticPathRawInputMissing;
  case PathDiagnosticCode::RawInputNotRegular: return MessageId::DiagnosticPathRawInputNotRegular;
  case PathDiagnosticCode::RawInputUnreadable: return MessageId::DiagnosticPathRawInputUnreadable;
  case PathDiagnosticCode::RawInputReadFailed: return MessageId::DiagnosticPathRawInputReadFailed;
  case PathDiagnosticCode::ArtifactNamesInvalid: return MessageId::DiagnosticPathArtifactNamesInvalid;
  case PathDiagnosticCode::CsvInspect: return MessageId::DiagnosticPathCsvInspect;
  case PathDiagnosticCode::CsvRefuseDirectory: return MessageId::DiagnosticPathCsvRefuseDirectory;
  case PathDiagnosticCode::CsvRemove: return MessageId::DiagnosticPathCsvRemove;
  case PathDiagnosticCode::CsvCreateDirectory: return MessageId::DiagnosticPathCsvCreateDirectory;
  case PathDiagnosticCode::CsvOpen: return MessageId::DiagnosticPathCsvOpen;
  case PathDiagnosticCode::CsvWrite: return MessageId::DiagnosticPathCsvWrite;
  case PathDiagnosticCode::CtfInspect: return MessageId::DiagnosticPathCtfInspect;
  case PathDiagnosticCode::CtfRefuseFile: return MessageId::DiagnosticPathCtfRefuseFile;
  case PathDiagnosticCode::CtfRemove: return MessageId::DiagnosticPathCtfRemove;
  case PathDiagnosticCode::CtfCreateDirectory: return MessageId::DiagnosticPathCtfCreateDirectory;
  case PathDiagnosticCode::CtfStreamOpen: return MessageId::DiagnosticPathCtfStreamOpen;
  case PathDiagnosticCode::CtfStreamWrite: return MessageId::DiagnosticPathCtfStreamWrite;
  case PathDiagnosticCode::CtfMetadataWrite: return MessageId::DiagnosticPathCtfMetadataWrite;
  case PathDiagnosticCode::XmlInspect: return MessageId::DiagnosticPathXmlInspect;
  case PathDiagnosticCode::XmlRefuseDirectory: return MessageId::DiagnosticPathXmlRefuseDirectory;
  case PathDiagnosticCode::XmlRemove: return MessageId::DiagnosticPathXmlRemove;
  case PathDiagnosticCode::XmlWrite: return MessageId::DiagnosticPathXmlWrite;
  case PathDiagnosticCode::XmlClockUuidMissing: return MessageId::DiagnosticPathXmlClockUuidMissing;
  case PathDiagnosticCode::XmlClockUuidShared: return MessageId::DiagnosticPathXmlClockUuidShared;
  }
  throw std::logic_error(formatMessage(MessageId::DiagnosticInvalidPathCode));
}

std::string pathDiagnosticMessage(PathDiagnosticCode code, std::string_view path,
                                  std::optional<std::string_view> nativeDetail)
{
  auto message = formatMessage(pathMessageId(code), MessageStyle::Detailed, {path});
  return nativeDetail.has_value()
             ? formatMessage(MessageId::DiagnosticNativeDetail, MessageStyle::Detailed, {message, *nativeDetail})
             : message;
}

std::string configurationFilesMissingMessage(std::string_view suffix, std::string_view directory)
{
  return formatMessage(MessageId::DiagnosticConfigurationFilesMissing, MessageStyle::Detailed, {suffix, directory});
}

std::string configurationFilenameMessage(std::string_view suffix, std::string_view path)
{
  return formatMessage(MessageId::DiagnosticConfigurationFilename, MessageStyle::Detailed, {suffix, path});
}

std::string rawInputAlignmentMessage(std::uint64_t alignment, std::string_view path, std::uint64_t size)
{
  return formatMessage(MessageId::DiagnosticRawInputAlignment, MessageStyle::Detailed, {alignment, path, size});
}

std::string specificOutputTargetMessage(std::string_view description)
{
  return formatMessage(MessageId::DiagnosticSpecificOutputTarget, MessageStyle::Detailed, {description});
}

std::string outputParentMessage(std::string_view description, std::string_view parent)
{
  return formatMessage(MessageId::DiagnosticOutputParent, MessageStyle::Detailed, {description, parent});
}

std::string decodeSummaryMessage(std::uint64_t bytes, double seconds, std::uint64_t records)
{
  const auto mebibytes = static_cast<double>(bytes) / (1024.0 * 1024.0);
  const auto mebibytesPerSecond = seconds > 0.0 ? mebibytes / seconds : 0.0;
  std::ostringstream duration;
  duration << std::fixed << std::setprecision(3) << seconds;
  std::ostringstream rate;
  rate << std::fixed << std::setprecision(2) << mebibytesPerSecond;
  return formatMessage(MessageId::DiagnosticDecodeSummary, MessageStyle::Detailed,
                       {bytes, duration.str(), rate.str(), records});
}

std::string backendFailureMessage(std::string_view backend, std::string_view target,
                                  std::string_view phase, std::string_view detail)
{
  return target.empty()
             ? formatMessage(MessageId::DiagnosticBackendFailure, MessageStyle::Detailed, {backend, phase, detail})
             : formatMessage(MessageId::DiagnosticTargetBackendFailure, MessageStyle::Detailed,
                             {backend, target, phase, detail});
}

std::string ctfDataTypeMessage(std::string_view dataType)
{
  return formatMessage(MessageId::DiagnosticCtfDataType, MessageStyle::Detailed,
                       {dataType, formatMessage(MessageId::DiagnosticCtfValueRequirements)});
}

std::string ctfDataSizeMessage(std::uint64_t size, std::string_view dataType)
{
  return formatMessage(MessageId::DiagnosticCtfDataSize, MessageStyle::Detailed,
                       {size, dataType, formatMessage(MessageId::DiagnosticCtfValueRequirements)});
}
