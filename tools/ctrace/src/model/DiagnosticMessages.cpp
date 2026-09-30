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

std::string diagnosticMessage(DiagnosticMessageCode code)
{
  switch (code) {
  case DiagnosticMessageCode::SpecifyTraceDirectory:
    return "Specify <trace-dir>";
  case DiagnosticMessageCode::TraceDirectoryRequired:
    return "trace directory job requires <trace-dir>";
  case DiagnosticMessageCode::TraceConfigurationPathEmpty:
    return "trace-run configuration path is empty";
  case DiagnosticMessageCode::TraceConfigurationSourcePathMissing:
    return "trace-run configuration has no source path";
  case DiagnosticMessageCode::ReferenceDiagnosticMissing:
    return "trace generation setup failed without a diagnostic message";
  case DiagnosticMessageCode::SelectedTraceConfiguration:
    return "selected trace-run configuration";
  case DiagnosticMessageCode::SkippingExcludedTraceChannel:
    return "skipping raw trace channel excluded from active input selection";
  case DiagnosticMessageCode::AppliedTraceMetadata:
    return "applied ctrace-run meta";
  case DiagnosticMessageCode::UsingTimestampPrescaler:
    return "using timestamp prescaler";
  case DiagnosticMessageCode::SkippingUnsupportedTraceSource:
    return "skipping unsupported formatted CoreSight trace source";
  case DiagnosticMessageCode::ItmDisabledChannel:
    return "ITM data was received on a channel not enabled by ctrace-setup.itm.enable";
  case DiagnosticMessageCode::YamlRootMapRequired:
    return "expected a YAML map containing 'ctrace-run'";
  case DiagnosticMessageCode::YamlRootMissing:
    return "missing top-level 'ctrace-run' node";
  case DiagnosticMessageCode::YamlRootMapInvalid:
    return "top-level 'ctrace-run' node must be a map";
  case DiagnosticMessageCode::TraceFormatInvalid:
    return "'trace-format' must be 'unformatted' or 'formatted'";
  case DiagnosticMessageCode::ReferenceArrayMissing:
    return "missing required 'ctrace-refs' array";
  case DiagnosticMessageCode::YamlDocumentCount:
    return "expected exactly one YAML document";
  case DiagnosticMessageCode::ItmEnableMapRequired:
    return "'itm' must be a map containing 'enable'";
  case DiagnosticMessageCode::TimestampsMapRequired:
    return "'timestamps' must be empty or a map";
  case DiagnosticMessageCode::ReferenceDuplicateSourceLocated:
    return "duplicate value in 'source' array";
  case DiagnosticMessageCode::ReferenceDuplicateSource:
    return "duplicate value in source array";
  case DiagnosticMessageCode::ReferenceStreamRangeLocated:
    return "'stream' must be a CoreSight ATB trace ID between 1 and 111";
  case DiagnosticMessageCode::ReferenceStreamRange:
    return "stream must be a CoreSight ATB trace ID between 1 and 111";
  case DiagnosticMessageCode::ReferenceItmSourceRangeLocated:
    return "ITM 'source' must be between 0 and 31";
  case DiagnosticMessageCode::ReferenceItmSourceRange:
    return "ITM source must be between 0 and 31";
  case DiagnosticMessageCode::SetupProcessorRequired:
    return "pname is required for every ctrace-setup in a multi-processor configuration";
  case DiagnosticMessageCode::SingleProcessorBindingRequired:
    return "unformatted SINGLE trace requires one unambiguous processor metadata binding";
  case DiagnosticMessageCode::ReferenceProcessorRequired:
    return "pname is required for every ctrace-ref in a multi-processor configuration";
  case DiagnosticMessageCode::IgnoringReferenceMultipleProcessors:
    return "ignoring ctrace-ref without pname because multiple ctrace-setup processors are active";
  case DiagnosticMessageCode::IgnoringReferenceAmbiguousProcessor:
    return "ignoring ctrace-ref without pname because its processor binding is ambiguous";
  case DiagnosticMessageCode::ConflictingDataSizes:
    return "conflicting active ctrace-setup data.size values";
  case DiagnosticMessageCode::AmbiguousSingleClock:
    return "unformatted SINGLE trace has ambiguous timestamps.clock values across processor candidates";
  case DiagnosticMessageCode::FormattedReferenceProcessorRequired:
    return "pname is required for a formatted ctrace-ref when multiple active ctrace-setup processors are available";
  case DiagnosticMessageCode::ItmAnchorPathRequired:
    return "processor ITM route anchor path must use '[pname/]itm'";
  case DiagnosticMessageCode::ItmAnchorTypeRequired:
    return "processor ITM route anchor must use reference type 'itm'";
  case DiagnosticMessageCode::TimestampReferenceType:
    return "timestamps reference must use type 'itm' or transitional type 'dwt'";
  case DiagnosticMessageCode::ItmAnchorBusIdRequired:
    return "processor ITM route anchor requires a CoreSight Trace Bus ID";
  case DiagnosticMessageCode::TimestampPrescalerRangeLocated:
    return "'timestamps.itm-prescaler' must be one of 1, 4, 16, or 64";
  case DiagnosticMessageCode::TimestampPrescalerRange:
    return "ctrace-setup timestamps.itm-prescaler must be one of 1, 4, 16, or 64";
  case DiagnosticMessageCode::ConflictingFormattedPrescalers:
    return "conflicting timestamps.itm-prescaler values for one formatted processor ITM route";
  case DiagnosticMessageCode::ConflictingClocks:
    return "conflicting active ctrace-setup timestamps.clock values";
  case DiagnosticMessageCode::IgnoringFormattedItmEnableConflict:
    return "ignoring conflicting ctrace-setup itm.enable assignment for one formatted ITM route";
  case DiagnosticMessageCode::ActiveSetupProcessorRequired:
    return "pname is required for active ctrace-setup fragments in a multi-processor configuration";
  case DiagnosticMessageCode::UnnamedSetupMultipleProcessors:
    return "one unnamed ctrace-setup processor cannot bind multiple formatted pnames";
  case DiagnosticMessageCode::UnnamedSetupMultipleRoutes:
    return "one unnamed ctrace-setup processor cannot bind multiple formatted ITM routes";
  case DiagnosticMessageCode::FormattedRouteAnchorRequired:
    return "formatted trace input requires an ITM route anchor or supported feature fallback";
  case DiagnosticMessageCode::StreamlessReferenceAmbiguous:
    return "streamless ctrace-ref cannot be associated with one formatted ITM route";
  case DiagnosticMessageCode::ConflictingSinglePrescalers:
    return "unformatted SINGLE trace has conflicting timestamps.itm-prescaler values for one processor";
  case DiagnosticMessageCode::IgnoringSingleItmEnableConflict:
    return "ignoring conflicting ctrace-setup itm.enable values for unformatted SINGLE trace";
  case DiagnosticMessageCode::AmbiguousSinglePrescalers:
    return "unformatted SINGLE trace cannot choose between different timestamps.itm-prescaler values";
  case DiagnosticMessageCode::IgnoringProcessorItmEnableConflict:
    return "ignoring different ctrace-setup itm.enable values across processor candidates for unformatted SINGLE trace";
  case DiagnosticMessageCode::CtfConflictingSourceMetadata:
    return "CTF metadata cannot describe conflicting active metadata for one route/type/source key";
  case DiagnosticMessageCode::CtfConfiguredClockInvalid:
    return "CTF output cannot use the configured timestamps.clock";
  case DiagnosticMessageCode::CtfClockRequired:
    return "CTF output requires timestamps.clock; no default is assumed";
  case DiagnosticMessageCode::CtfClockPositive:
    return "CTF output requires timestamps.clock to be greater than zero";
  case DiagnosticMessageCode::CtfConfiguredAddressInvalid:
    return "CTF output cannot use the configured ctrace-run address";
  case DiagnosticMessageCode::CtfConfiguredDataTypeInvalid:
    return "CTF output cannot use the configured ctrace-run data-type";
  case DiagnosticMessageCode::CtfConfiguredSizeInvalid:
    return "CTF output cannot use the configured ctrace-run size";
  case DiagnosticMessageCode::CtfComparatorRange:
    return "CTF output requires DWT comparator sources between 0 and 3";
  case DiagnosticMessageCode::CtfAddressRangeInvalid:
    return "CTF output cannot represent the configured DWT address range";
  case DiagnosticMessageCode::UnknownException:
    return "unknown exception";
  case DiagnosticMessageCode::CsvFilePathRequired:
    return "CSV output path must identify a file";
  case DiagnosticMessageCode::CsvStreamFactoryRequired:
    return "CSV stream factory must be configured";
  case DiagnosticMessageCode::CtfPayloadExceedsSize:
    return "CTF record payload exceeds its declared size";
  case DiagnosticMessageCode::CtfRecordTooLarge:
    return "CTF record does not fit into a packet";
  case DiagnosticMessageCode::CtfPayloadShorterThanSize:
    return "CTF record payload is shorter than its declared size";
  case DiagnosticMessageCode::CtfRouteIdentityMismatch:
    return "CTF route identity does not match the configured normalized route catalogue";
  case DiagnosticMessageCode::CtfAddressPayloadSizeInvalid:
    return "CTF DWT address fragment has an invalid SWO payload size";
  case DiagnosticMessageCode::CtfConfiguredRouteIdentityConflict:
    return "CTF configuration contains inconsistent normalized route identities";
  case DiagnosticMessageCode::CtfRuntimeStreamRequired:
    return "CTF binary output cannot encode an event route without an exact runtime stream descriptor";
  case DiagnosticMessageCode::CtfItmPayloadSizeInvalid:
    return "CTF ITM value has an invalid SWO payload size";
  case DiagnosticMessageCode::CtfConfiguredPayloadSizeMismatch:
    return "configured ctrace-run size does not match the decoded SWO payload size";
  case DiagnosticMessageCode::XmlMultipleClockDomains:
    return "Trace Compass XML views were not generated for this input because emitted CTF streams "
           "use multiple clock domains";
  case DiagnosticMessageCode::XmlViewSelectionInvalid:
    return "Trace Compass XML contains an unsupported route view selection";
  case DiagnosticMessageCode::XmlTraceBusIdRange:
    return "Trace Compass XML requires Trace Bus IDs between 0 and 111";
  case DiagnosticMessageCode::XmlCanonicalClockUuidRequired:
    return "Trace Compass XML requires canonical lower-case clock UUIDs";
  case DiagnosticMessageCode::XmlViewRouteRequired:
    return "Trace Compass XML requires at least one view route";
  case DiagnosticMessageCode::XmlUniqueClockRouteRequired:
    return "Trace Compass XML requires unique clock UUID and Trace Bus ID pairs";
  case DiagnosticMessageCode::XmlGraphicalViewRequired:
    return "Trace Compass XML requires at least one graphical view";
  case DiagnosticMessageCode::CtfExceptionStreamUnknown:
    return "CTF exception observation references an unknown stream class";
  case DiagnosticMessageCode::CtfGraphicalStreamUnknown:
    return "CTF graphical-topic observation references an unknown stream class";
  case DiagnosticMessageCode::CtfDuplicateClockId:
    return "CTF metadata contains a duplicate clock-domain ID";
  case DiagnosticMessageCode::CtfClockNamesInvalid:
    return "CTF metadata requires unique valid clock-domain names";
  case DiagnosticMessageCode::CtfClockFrequencyNonzero:
    return "CTF metadata requires a non-zero clock-domain frequency";
  case DiagnosticMessageCode::CtfClockUuidsDistinct:
    return "CTF clock UUIDs must be distinct from the trace and other clock domains";
  case DiagnosticMessageCode::CtfDuplicateRoute:
    return "CTF metadata contains a duplicate normalized route";
  case DiagnosticMessageCode::CtfInconsistentRoutes:
    return "CTF metadata contains inconsistent normalized route identities";
  case DiagnosticMessageCode::CtfDuplicateStreamId:
    return "CTF metadata contains a duplicate stream-class ID";
  case DiagnosticMessageCode::CtfStreamClockUnknown:
    return "CTF stream class references an unknown clock domain";
  case DiagnosticMessageCode::CtfRouteBusIdRange:
    return "CTF ITM stream route requires a CoreSight ATB trace ID between 1 and 111";
  case DiagnosticMessageCode::CtfStreamRouteMismatch:
    return "CTF stream-class ID does not match its normalized route identity";
  case DiagnosticMessageCode::CtfUnreferencedClock:
    return "CTF metadata contains a clock domain without a referencing stream class";
  case DiagnosticMessageCode::CtfSourceRouteUnknown:
    return "CTF source metadata references an unknown normalized route";
  case DiagnosticMessageCode::CtfItmChannelRange:
    return "CTF ITM source metadata requires a channel between 1 and 31";
  case DiagnosticMessageCode::CtfDwtComparatorRange:
    return "CTF DWT source metadata requires a comparator between 0 and 3";
  case DiagnosticMessageCode::CtfDataTypeSizeInvalid:
    return "CTF DWT source metadata has an invalid data-type/size combination";
  case DiagnosticMessageCode::CtfSourceAddressOverflow:
    return "CTF DWT source address range exceeds the unsigned 64-bit metadata domain";
  case DiagnosticMessageCode::CtfSourceTypeInvalid:
    return "CTF source metadata type must be 'itm' or 'dwt'";
  case DiagnosticMessageCode::CtfDuplicateSource:
    return "CTF metadata contains duplicate source metadata";
  case DiagnosticMessageCode::CtfConflictingSource:
    return "CTF metadata contains conflicting source metadata for one route";
  case DiagnosticMessageCode::CtfExplicitClockUuidRequired:
    return "non-legacy CTF clock domains require an explicit UUID";
  case DiagnosticMessageCode::RawDecodeChunkTooLarge:
    return "raw decode chunk is too large";
  case DiagnosticMessageCode::CortexRoutesRequired:
    return "Cortex-M stream decoding requires at least one normalized route";
  case DiagnosticMessageCode::TimestampPrescalerPositive:
    return "ITM timestamp prescaler must be greater than zero";
  case DiagnosticMessageCode::TimestampPrescalerNonzero:
    return "ITM timestamp prescaler must not be zero";
  case DiagnosticMessageCode::CortexTraceBusIdRange:
    return "formatted Cortex-M route requires a CoreSight ATB trace ID between 1 and 111";
  case DiagnosticMessageCode::CortexDuplicateTraceBusId:
    return "duplicate CoreSight ATB trace ID in Cortex-M route configuration";
  case DiagnosticMessageCode::CortexDuplicateRouteId:
    return "duplicate normalized route ID in Cortex-M route configuration";
  case DiagnosticMessageCode::CortexRouteIdentityMismatch:
    return "OpenCSD element route identity does not match normalized route catalogue";
  case DiagnosticMessageCode::OpenCsdDecoderFinished:
    return "OpenCSD ITM decoder already finished";
  case DiagnosticMessageCode::RawTraceDataNull:
    return "raw trace data pointer is null while bytes are present";
  case DiagnosticMessageCode::OpenCsdRoutesRequired:
    return "OpenCSD ITM decoding requires at least one normalized route";
  case DiagnosticMessageCode::OpenCsdSingleRouteRequired:
    return "OpenCSD SINGLE decoding requires exactly one normalized route";
  case DiagnosticMessageCode::PacketRoutesRequired:
    return "formatted OpenCSD packet collection requires at least one normalized route";
  case DiagnosticMessageCode::PacketTraceBusIdRange:
    return "formatted OpenCSD packet route requires a Trace Bus ID between 1 and 111";
  case DiagnosticMessageCode::PacketDuplicateRouteId:
    return "duplicate normalized route ID in formatted OpenCSD packet routes";
  case DiagnosticMessageCode::PacketDuplicateTraceBusId:
    return "duplicate Trace Bus ID in formatted OpenCSD packet routes";
  case DiagnosticMessageCode::PacketUnknownCutoffRoute:
    return "route-aware OpenCSD transaction cutoff references an unknown normalized route";
  case DiagnosticMessageCode::PacketUnknownDiagnosticRoute:
    return "OpenCSD diagnostic references an unknown normalized route";
  case DiagnosticMessageCode::PacketUnknownDataLossRoute:
    return "OpenCSD data-loss interval references an unknown normalized route";
  case DiagnosticMessageCode::PacketUnknownRawRoute:
    return "raw OpenCSD packet references an unknown normalized route";
  case DiagnosticMessageCode::PacketUnknownFormattedRoute:
    return "formatted data references an unknown normalized route";
  }
  throw std::logic_error("unknown diagnostic message code");
}

std::string locatedDiagnosticMessage(std::string_view path, std::size_t line, std::string_view detail)
{
  auto result = std::string(path);
  if (line > 0U) {
    result += "(" + std::to_string(line) + ")";
  }
  return result + ": " + std::string(detail);
}

std::string unknownNormalizedRouteMessage(std::uint64_t routeId)
{
  return "OpenCSD element references unknown normalized route " + std::to_string(routeId);
}

std::string unsignedArgumentMessage(std::string_view option, std::string_view value)
{
  return std::string(option) + " must be an unsigned integer, got " + std::string(value);
}

std::string missingArgumentValueMessage(std::string_view option)
{
  return "Missing value for " + std::string(option);
}

std::string argumentValueSeparatorMessage(std::string_view option)
{
  return std::string(option) + " values must be separated by spaces";
}

std::string unknownArgumentMessage(std::string_view argument)
{
  return "Unknown argument: " + std::string(argument);
}

std::string positionalArgumentMessage(std::string_view argument)
{
  return "Unexpected positional argument: " + std::string(argument);
}

std::string streamArgumentMessage(std::string_view value)
{
  return "--stream must be a CoreSight Trace Bus ID between 0 and 111, got " + std::string(value);
}

std::string typeArgumentMessage(std::string_view value, std::string_view accepted)
{
  return "Invalid --type value: " + std::string(value) + " (accepted: " + std::string(accepted) + ")";
}

std::string targetArgumentMessage(std::string_view value)
{
  return "--target must be a solution-set name, got " + std::string(value);
}

std::string configFieldMessage(std::string_view field, ConfigFieldProblem problem)
{
  const auto name = "'" + std::string(field) + "'";
  switch (problem) {
  case ConfigFieldProblem::ScalarString:
    return name + " must be a scalar string";
  case ConfigFieldProblem::ScalarUnsigned:
    return name + " must be a scalar unsigned integer";
  case ConfigFieldProblem::Unsigned:
    return name + " must be an unsigned integer";
  case ConfigFieldProblem::UnsignedRange:
    return name + " must be an unsigned integer in range";
  case ConfigFieldProblem::Array:
    return name + " must be an array";
  case ConfigFieldProblem::Scalar:
    return name + " must be a scalar value";
  case ConfigFieldProblem::StringOrList:
    return name + " must be a string or list of strings";
  case ConfigFieldProblem::StringEntry:
    return "each " + name + " entry must be a string";
  case ConfigFieldProblem::MapEntry:
    return "each " + name + " entry must be a map";
  case ConfigFieldProblem::UnsignedEntry:
    return "each " + name + " entry must be an unsigned integer";
  case ConfigFieldProblem::RequiredReferenceScalar:
    return "missing required " + name + " scalar in ctrace-ref entry";
  }
  throw std::logic_error("unknown configuration field problem");
}

std::string yamlParseMessage(std::string_view path, std::optional<std::size_t> line,
                             std::optional<std::size_t> column, std::string_view nativeDetail)
{
  std::string message = "failed to parse trace-run configuration: " + std::string(path);
  if (line.has_value()) {
    message += "(" + std::to_string(*line);
    if (column.has_value()) {
      message += "," + std::to_string(*column);
    }
    message += ')';
  }
  return message + ": " + std::string(nativeDetail);
}

std::string processorBindingMessage(ProcessorBindingProblem problem, std::string_view name)
{
  switch (problem) {
  case ProcessorBindingProblem::IgnoredUnmatchedSetup:
    return "ignoring ctrace-ref pname '" + std::string(name) + "' because it has no matching ctrace-setup";
  case ProcessorBindingProblem::ConflictingReferencePath:
    return "ctrace-ref path processor conflicts with pname '" + std::string(name) + "'";
  case ProcessorBindingProblem::NoActiveSetup:
    return "ctrace-ref pname '" + std::string(name) + "' has no matching active ctrace-setup processor";
  case ProcessorBindingProblem::MultipleBusIds:
    return "processor '" + std::string(name) + "' has ITM routes bound to multiple CoreSight Trace Bus IDs";
  }
  throw std::logic_error("unknown processor binding problem");
}

std::string ignoredProcessorMismatchMessage(std::string_view referenceName, std::string_view setupName)
{
  return "ignoring ctrace-ref pname '" + std::string(referenceName) +
         "' because it does not match ctrace-setup pname '" + std::string(setupName) + "'";
}

std::string disabledReferenceMessage(std::string_view reference, std::size_t ordinal)
{
  return "ctrace-ref '" + std::string(reference) + "' resolves only to disabled ctrace-setup fragment " +
         std::to_string(ordinal);
}

std::string traceBusBindingMessage(TraceBusBindingProblem problem, std::uint32_t traceBusId)
{
  switch (problem) {
  case TraceBusBindingProblem::ConflictingProcessors:
    return "CoreSight Trace Bus ID " + std::to_string(traceBusId) + " has conflicting ITM processor bindings";
  case TraceBusBindingProblem::MissingAnchor:
    return "ctrace-ref describes CoreSight Trace Bus ID " + std::to_string(traceBusId) +
           " without an ITM route anchor or supported feature fallback";
  }
  throw std::logic_error("unknown trace bus binding problem");
}

std::string pathDiagnosticMessage(PathDiagnosticCode code, std::string_view path,
                                  std::optional<std::string_view> nativeDetail)
{
  std::string prefix;
  switch (code) {
  case PathDiagnosticCode::TraceConfigMissing:
    prefix = "trace-run configuration not found: ";
    break;
  case PathDiagnosticCode::TraceDirectoryMissing:
    prefix = "trace directory not found: ";
    break;
  case PathDiagnosticCode::RawInputMissing:
    prefix = "no eligible raw trace input found for solution-set ";
    break;
  case PathDiagnosticCode::RawInputNotRegular:
    prefix = "raw trace input is not a regular file: ";
    break;
  case PathDiagnosticCode::RawInputUnreadable:
    prefix = "raw trace input is not readable: ";
    break;
  case PathDiagnosticCode::RawInputReadFailed:
    prefix = "failed to read input file: ";
    break;
  case PathDiagnosticCode::ArtifactNamesInvalid:
    prefix = "cannot derive trace artifact names from ";
    break;
  case PathDiagnosticCode::CsvInspect:
    prefix = "Failed to inspect existing CSV output ";
    break;
  case PathDiagnosticCode::CsvRefuseDirectory:
    prefix = "Refusing to replace CSV output because the target is a directory: ";
    break;
  case PathDiagnosticCode::CsvRemove:
    prefix = "Failed to remove existing CSV output ";
    break;
  case PathDiagnosticCode::CsvCreateDirectory:
    prefix = "Failed to create CSV output directory ";
    break;
  case PathDiagnosticCode::CsvOpen:
    prefix = "Failed to open CSV output ";
    break;
  case PathDiagnosticCode::CsvWrite:
    prefix = "Failed to write CSV output ";
    break;
  case PathDiagnosticCode::CtfInspect:
    prefix = "Failed to inspect existing CTF output ";
    break;
  case PathDiagnosticCode::CtfRefuseFile:
    prefix = "Refusing to replace CTF output because the target is not a directory: ";
    break;
  case PathDiagnosticCode::CtfRemove:
    prefix = "Failed to remove CTF directory ";
    break;
  case PathDiagnosticCode::CtfCreateDirectory:
    prefix = "Failed to create CTF output directory ";
    break;
  case PathDiagnosticCode::CtfStreamOpen:
    prefix = "Failed to open CTF stream ";
    break;
  case PathDiagnosticCode::CtfStreamWrite:
    prefix = "Failed to write CTF stream in ";
    break;
  case PathDiagnosticCode::CtfMetadataWrite:
    prefix = "Failed to write CTF metadata ";
    break;
  case PathDiagnosticCode::XmlInspect:
    prefix = "Failed to inspect existing Trace Compass XML ";
    break;
  case PathDiagnosticCode::XmlRefuseDirectory:
    prefix = "Refusing to replace Trace Compass XML because the target is a directory: ";
    break;
  case PathDiagnosticCode::XmlRemove:
    prefix = "Failed to remove existing Trace Compass XML ";
    break;
  case PathDiagnosticCode::XmlWrite:
    prefix = "Failed to write Trace Compass XML ";
    break;
  case PathDiagnosticCode::XmlClockUuidMissing:
    prefix = "Trace Compass XML requires an explicit clock UUID for input ";
    break;
  case PathDiagnosticCode::XmlClockUuidShared:
    prefix = "Trace Compass XML requires independent clock UUIDs for separate inputs: ";
    break;
  }
  auto message = prefix + std::string(path);
  if (nativeDetail.has_value()) {
    message += ": " + std::string(*nativeDetail);
  }
  return message;
}

std::string configurationFilesMissingMessage(std::string_view suffix, std::string_view directory)
{
  return "no *" + std::string(suffix) + " files found in trace directory: " + std::string(directory);
}

std::string configurationFilenameMessage(std::string_view suffix, std::string_view path)
{
  return "expected <solution-set>" + std::string(suffix) + ", got " + std::string(path);
}

std::string rawInputAlignmentMessage(std::uint64_t alignment, std::string_view path, std::uint64_t size)
{
  return "formatted raw trace input size must be a multiple of " + std::to_string(alignment) +
         " bytes: " + std::string(path) + " (size=" + std::to_string(size) + ")";
}

std::string specificOutputTargetMessage(std::string_view description)
{
  return std::string(description) + " must identify a specific output path";
}

std::string outputParentMessage(std::string_view description, std::string_view parent)
{
  return std::string(description) + " parent is not a directory: " + std::string(parent);
}

std::string decodeSummaryMessage(std::uint64_t bytes, double seconds, std::uint64_t records)
{
  const auto mebibytes = static_cast<double>(bytes) / (1024.0 * 1024.0);
  const auto mebibytesPerSecond = seconds > 0.0 ? mebibytes / seconds : 0.0;
  std::ostringstream out;
  out << "processed " << bytes << " input bytes in " << std::fixed << std::setprecision(3) << seconds
      << " s (" << std::setprecision(2) << mebibytesPerSecond << " MiB/s); trace/diagnostic records: " << records;
  return out.str();
}

std::string backendFailureMessage(std::string_view backend, std::string_view target,
                                  std::string_view phase, std::string_view detail)
{
  return std::string(backend) + " output" + (target.empty() ? std::string() : " '" + std::string(target) + "'") +
         " failed during " + std::string(phase) + ": " + std::string(detail);
}

/** @brief Describes the accepted CTF value schema without depending on the output layer. */
static constexpr std::string_view ctfValueTypeRequirements =
    "supported data-type values are 'unsigned', 'signed', and 'float'; "
    "size must be 1, 2, or 4, and float requires size 4";

std::string ctfDataTypeMessage(std::string_view dataType)
{
  return "CTF output cannot use ctrace-run data-type '" + std::string(dataType) + "'; " +
         std::string(ctfValueTypeRequirements);
}

std::string ctfDataSizeMessage(std::uint64_t size, std::string_view dataType)
{
  return "CTF output cannot use ctrace-run size " + std::to_string(size) + " with data-type '" +
         std::string(dataType) + "'; " + std::string(ctfValueTypeRequirements);
}
