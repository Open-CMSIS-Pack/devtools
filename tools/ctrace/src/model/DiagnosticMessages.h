/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_MODEL_DIAGNOSTICMESSAGES_H
#define CTRACE_SRC_MODEL_DIAGNOSTICMESSAGES_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/** @brief Identifies ctrace-owned diagnostics that have no text parameters. */
enum class DiagnosticMessageCode {
  SpecifyTraceDirectory,
  TraceDirectoryRequired,
  TraceConfigurationPathEmpty,
  TraceConfigurationSourcePathMissing,
  ReferenceDiagnosticMissing,
  SelectedTraceConfiguration,
  SkippingExcludedTraceChannel,
  AppliedTraceMetadata,
  UsingTimestampPrescaler,
  SkippingUnsupportedTraceSource,
  ItmDisabledChannel,
  YamlRootMapRequired,
  YamlRootMissing,
  YamlRootMapInvalid,
  TraceFormatInvalid,
  ReferenceArrayMissing,
  YamlDocumentCount,
  ItmEnableMapRequired,
  TimestampsMapRequired,
  ReferenceDuplicateSourceLocated,
  ReferenceDuplicateSource,
  ReferenceStreamRangeLocated,
  ReferenceStreamRange,
  ReferenceItmSourceRangeLocated,
  ReferenceItmSourceRange,
  SetupProcessorRequired,
  SingleProcessorBindingRequired,
  ReferenceProcessorRequired,
  IgnoringReferenceMultipleProcessors,
  IgnoringReferenceAmbiguousProcessor,
  ConflictingDataSizes,
  AmbiguousSingleClock,
  FormattedReferenceProcessorRequired,
  ItmAnchorPathRequired,
  ItmAnchorTypeRequired,
  TimestampReferenceType,
  ItmAnchorBusIdRequired,
  TimestampPrescalerRangeLocated,
  TimestampPrescalerRange,
  ConflictingFormattedPrescalers,
  ConflictingClocks,
  IgnoringFormattedItmEnableConflict,
  ActiveSetupProcessorRequired,
  UnnamedSetupMultipleProcessors,
  UnnamedSetupMultipleRoutes,
  FormattedRouteAnchorRequired,
  StreamlessReferenceAmbiguous,
  ConflictingSinglePrescalers,
  IgnoringSingleItmEnableConflict,
  AmbiguousSinglePrescalers,
  IgnoringProcessorItmEnableConflict,
  CtfConflictingSourceMetadata,
  CtfConfiguredClockInvalid,
  CtfClockRequired,
  CtfClockPositive,
  CtfConfiguredAddressInvalid,
  CtfConfiguredDataTypeInvalid,
  CtfConfiguredSizeInvalid,
  CtfComparatorRange,
  CtfAddressRangeInvalid,
  UnknownException,
  CsvFilePathRequired,
  CsvStreamFactoryRequired,
  CtfPayloadExceedsSize,
  CtfRecordTooLarge,
  CtfPayloadShorterThanSize,
  CtfRouteIdentityMismatch,
  CtfAddressPayloadSizeInvalid,
  CtfConfiguredRouteIdentityConflict,
  CtfRuntimeStreamRequired,
  CtfItmPayloadSizeInvalid,
  CtfConfiguredPayloadSizeMismatch,
  XmlMultipleClockDomains,
  XmlViewSelectionInvalid,
  XmlTraceBusIdRange,
  XmlCanonicalClockUuidRequired,
  XmlViewRouteRequired,
  XmlUniqueClockRouteRequired,
  XmlGraphicalViewRequired,
  CtfExceptionStreamUnknown,
  CtfGraphicalStreamUnknown,
  CtfDuplicateClockId,
  CtfClockNamesInvalid,
  CtfClockFrequencyNonzero,
  CtfClockUuidsDistinct,
  CtfDuplicateRoute,
  CtfInconsistentRoutes,
  CtfDuplicateStreamId,
  CtfStreamClockUnknown,
  CtfRouteBusIdRange,
  CtfStreamRouteMismatch,
  CtfUnreferencedClock,
  CtfSourceRouteUnknown,
  CtfItmChannelRange,
  CtfDwtComparatorRange,
  CtfDataTypeSizeInvalid,
  CtfSourceAddressOverflow,
  CtfSourceTypeInvalid,
  CtfDuplicateSource,
  CtfConflictingSource,
  CtfExplicitClockUuidRequired,
  RawDecodeChunkTooLarge,
  CortexRoutesRequired,
  TimestampPrescalerPositive,
  TimestampPrescalerNonzero,
  CortexTraceBusIdRange,
  CortexDuplicateTraceBusId,
  CortexDuplicateRouteId,
  CortexRouteIdentityMismatch,
  OpenCsdDecoderFinished,
  RawTraceDataNull,
  OpenCsdRoutesRequired,
  OpenCsdSingleRouteRequired,
  PacketRoutesRequired,
  PacketTraceBusIdRange,
  PacketDuplicateRouteId,
  PacketDuplicateTraceBusId,
  PacketUnknownCutoffRoute,
  PacketUnknownDiagnosticRoute,
  PacketUnknownDataLossRoute,
  PacketUnknownRawRoute,
  PacketUnknownFormattedRoute,
};

/** @brief Formats a detailed-only operational diagnostic; severity and context remain with the caller. */
std::string diagnosticMessage(DiagnosticMessageCode code);

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
