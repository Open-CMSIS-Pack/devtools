/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

// Intentionally included repeatedly to declare IDs, validate templates, and build the catalog.
// Template braces form positional arguments {0}, {1}, ... . Compact nullptr reuses Detailed.
// Both styles take the same argument list, even when one template omits positions.

// Shared trace diagnostics: detailed verbose wording and compact CLI/CSV wording.

CTRACE_MESSAGE(TraceUnknownParameter,
    "unknown trace message parameter {0}",
    nullptr)

CTRACE_MESSAGE(TraceDecodeError,
    "trace decode error",
    "Trace decode error")

CTRACE_MESSAGE(TraceDwtCounterPayload,
    "unsupported DWT event-counter payload: size {0}, value 0x{1}; expected a non-zero 1-byte mask using bits 0..5 only",
    "Invalid DWT counter")

CTRACE_MESSAGE(TracePmuCounterPayload,
    "unsupported PMU event-counter payload: size {0}, value 0x{1}; expected a non-zero 1-byte mask using bits 0..7",
    "Invalid PMU counter")

CTRACE_MESSAGE(TraceInvalidExceptionAction,
    "invalid exception action 0x0 for exception {0}",
    "Invalid exception action")

CTRACE_MESSAGE(TracePcSamplePayload,
    "unsupported DWT PC-sample payload: size {0}, value {1}; expected a 4-byte PC or a 1-byte marker (0x00: CPU Sleeping, 0xff: Trace prohibited)",
    "Invalid PC sample")

CTRACE_MESSAGE(TraceDataAddressPayload,
    "unsupported DWT data address payload size {0}; expected 1, 2, or 4 bytes",
    "Invalid DWT data address")

CTRACE_MESSAGE(TracePcOrMatchPayload,
    "unsupported DWT PC or match payload size {0}; expected 1, 2, or 4 bytes",
    "Invalid DWT PC or match")

CTRACE_MESSAGE(TraceRecoveryGeneric,
    "data loss/resync boundary; timestamps across this point may not match",
    "Data loss; timestamp discontinuity")

CTRACE_MESSAGE(TraceRecoveryConsumed,
    "OpenCSD consumed {0} raw bytes while waiting for usable ITM trace packets; data loss until a later sync/recovery point",
    "{0} raw bytes without usable ITM packets")

CTRACE_MESSAGE(TraceRecoveryResumed,
    "ITM decoding resumed at hardware SYNC at raw offset {1}; affected raw interval [{0}, {1}) spans {2} bytes",
    "ITM resynced; {2} raw bytes affected")

CTRACE_MESSAGE(TraceRecoveryUnresolved,
    "ITM decoding did not resume before end of input at raw offset {1}; no later hardware SYNC; affected raw interval [{0}, {1}) spans {2} bytes",
    "No ITM resync before EOF; {2} raw bytes affected")

CTRACE_MESSAGE(TraceNativeError,
    "OpenCSD decoder error",
    "OpenCSD error")

CTRACE_MESSAGE(TraceNativeWarning,
    "OpenCSD decoder warning",
    "OpenCSD warning")

CTRACE_MESSAGE(TraceNativeBadPacketSequence,
    "OpenCSD detected an invalid ITM packet sequence",
    "Invalid ITM packet sequence")

CTRACE_MESSAGE(TraceNativeInvalidPacketHeader,
    "OpenCSD detected an invalid ITM packet header",
    "Invalid ITM packet header")

CTRACE_MESSAGE(TraceNativeNotInitialized,
    "OpenCSD decoder is not initialized",
    "OpenCSD not initialized")

CTRACE_MESSAGE(TraceNativeOutOfMemory,
    "OpenCSD decoder ran out of memory",
    "OpenCSD out of memory")

CTRACE_MESSAGE(TraceNativeInvalidParameter,
    "OpenCSD rejected a decoder parameter",
    "Invalid OpenCSD parameter")

CTRACE_MESSAGE(TraceNativeInputRead,
    "OpenCSD could not read required input data",
    "OpenCSD input read failed")

CTRACE_MESSAGE(TraceNativeDecodeFailed,
    "OpenCSD could not decode the trace data",
    "OpenCSD decode failed")

CTRACE_MESSAGE(TraceNativeInvalidOperation,
    "OpenCSD rejected a decoder operation",
    "Invalid OpenCSD operation")

CTRACE_MESSAGE(TraceNativeInvalidData,
    "OpenCSD rejected invalid trace data",
    "Invalid OpenCSD trace data")

CTRACE_MESSAGE(TraceNativeSystemError,
    "OpenCSD reported a system error",
    "OpenCSD system error")

CTRACE_MESSAGE(TraceNativeErrorCode,
    "{0} (code {1})",
    "{0}")

CTRACE_MESSAGE(TraceNativeResponseCode,
    "{0} (response {1})",
    "{0}")

CTRACE_MESSAGE(TraceAtRawOffset,
    "{0} at raw offset {1}",
    "{0}")

CTRACE_MESSAGE(TraceNativeDetail,
    "{0}. {1}",
    nullptr)

CTRACE_MESSAGE(TracePacketReserved,
    "Reserved ITM packet",
    nullptr)

CTRACE_MESSAGE(TracePacketBadSequence,
    "Bad ITM packet sequence",
    "Invalid ITM packet sequence")

CTRACE_MESSAGE(TracePacketIncomplete,
    "incomplete ITM packet at end of input",
    "Incomplete ITM packet at EOF")

CTRACE_MESSAGE(TraceMissingSync,
    "no hardware ITM SYNC before end of input",
    "No ITM SYNC before EOF")

CTRACE_MESSAGE(TraceMissingSyncAtOffset,
    "no hardware ITM SYNC before end of input; first formatter group at raw offset {0}",
    "No ITM SYNC before EOF")

CTRACE_MESSAGE(TraceProgressConsumedTooMany,
    "OpenCSD reported more formatted bytes processed than were supplied",
    "Invalid consumed byte count")

CTRACE_MESSAGE(TraceProgressPartialFormatterFrame,
    "OpenCSD stopped inside a memory-aligned formatter frame",
    "Stopped inside formatter frame")

CTRACE_MESSAGE(TraceProgressFormattedInput,
    "OpenCSD made no progress on formatted trace input",
    "No formatted decode progress")

CTRACE_MESSAGE(TraceProgressFormattedDrain,
    "OpenCSD made no progress after draining formatted trace",
    "No decode progress after formatted drain")

CTRACE_MESSAGE(TraceProgressRetry,
    "OpenCSD made no progress after a retry",
    "No decode progress after retry")

CTRACE_MESSAGE(TraceProgressResetForSync,
    "OpenCSD made no progress while raw data was present; decoder reset and searching for next real ITM async sync",
    "No decode progress; reset for ITM SYNC")

CTRACE_MESSAGE(TraceAbortedSuffix,
    "{0}; decode aborted",
    "{0}")

CTRACE_MESSAGE(TraceWaitTimeout,
    "OpenCSD WAIT did not clear after {0} FLUSH operations",
    "OpenCSD WAIT timeout")

CTRACE_MESSAGE(TraceDrainTimeout,
    "OpenCSD formatted drain did not clear after {0} FLUSH operations",
    "OpenCSD formatted drain timeout")

CTRACE_MESSAGE(TraceInvalidFormattedChunk,
    "formatted raw trace chunk is not a multiple of 16 bytes",
    "Invalid formatted chunk size")

CTRACE_MESSAGE(TraceInitializationFailure,
    "OpenCSD initialization failed",
    nullptr)

CTRACE_MESSAGE(TraceFormattedSessionFailure,
    "formatted OpenCSD session operation failed: {0} at raw input offset {1}",
    "OpenCSD session operation failed")

CTRACE_MESSAGE(TraceAbortNone,
    "{0}",
    "{0}")

CTRACE_MESSAGE(TraceAbortDecode,
    "OpenCSD aborted decode: {0}",
    "{0}")

CTRACE_MESSAGE(TraceAbortEndOfTrace,
    "OpenCSD aborted end-of-trace processing: {0}",
    "End of trace: {0}")

CTRACE_MESSAGE(TraceAbortWaitFlush,
    "OpenCSD aborted while flushing a WAIT response: {0}",
    "WAIT flush: {0}")

CTRACE_MESSAGE(TraceAbortFormattedDrain,
    "OpenCSD aborted while draining formatted trace: {0}",
    "Formatted drain: {0}")

CTRACE_MESSAGE(TraceAbortDecoderReset,
    "OpenCSD decoder reset failed: {0}",
    "OpenCSD decoder reset failed: {0}")

CTRACE_MESSAGE(TraceAbortRouteReset,
    "OpenCSD route-local decoder reset failed: {0}",
    "OpenCSD route reset failed: {0}")

CTRACE_MESSAGE(TraceAppendPacket,
    "{0}; {1}",
    nullptr)

CTRACE_MESSAGE(TraceAppendPacketAfterSeparator,
    "{0} {1}",
    nullptr)

CTRACE_MESSAGE(TraceTimestampRange,
    "{0}; timestamp {1} .. {2}.",
    "{0}")

CTRACE_MESSAGE(TraceTimestampRangeUnknown,
    "{0}; timestamp {1} .. unknown.",
    "{0}")

CTRACE_MESSAGE(TraceFallbackDataLoss,
    "trace data at raw offset {0} could not be decoded before the next hardware ITM sync",
    "ITM data loss")

CTRACE_MESSAGE(TraceFallbackDataLossCount,
    "{0} raw bytes from raw offset {1} could not be decoded before the next hardware ITM sync",
    "ITM data loss; {0} raw bytes affected")

CTRACE_MESSAGE(TraceFallbackBadPacketSequence,
    "invalid ITM packet sequence at raw offset {0}",
    "Invalid ITM packet sequence")

CTRACE_MESSAGE(TraceFallbackInvalidPacketHeader,
    "invalid ITM packet header at raw offset {0}",
    "Invalid ITM packet header")

CTRACE_MESSAGE(TraceFallbackIncompleteTail,
    "incomplete ITM packet starting at raw offset {0} at end of input",
    "Incomplete ITM packet at EOF")

CTRACE_MESSAGE(TraceFallbackNoProgress,
    "OpenCSD made no decode progress at raw offset {0}",
    "No decode progress")

CTRACE_MESSAGE(TraceFallbackWaitTimeout,
    "OpenCSD remained blocked while flushing pending data",
    "OpenCSD flush timeout")

CTRACE_MESSAGE(TraceFallbackDecodeError,
    "trace decode error at raw offset {0}",
    "Trace decode error")

CTRACE_MESSAGE(TraceFallbackInvalidExceptionAction,
    "trace decode error at raw offset {0}",
    "Invalid exception action")

CTRACE_MESSAGE(TraceFallbackDwtCounter,
    "trace decode error at raw offset {0}",
    "Invalid DWT counter")

CTRACE_MESSAGE(TraceFallbackPmuCounter,
    "trace decode error at raw offset {0}",
    "Invalid PMU counter")

CTRACE_MESSAGE(TraceFallbackDwtAddress,
    "trace decode error at raw offset {0}",
    "Invalid DWT address payload")

CTRACE_MESSAGE(TraceFallbackPcSample,
    "trace decode error at raw offset {0}",
    "Invalid PC sample")

CTRACE_MESSAGE(TraceFallbackNativeError,
    "trace decode error at raw offset {0}",
    "OpenCSD error")

CTRACE_MESSAGE(TraceFallbackNativeWarning,
    "trace decode error at raw offset {0}",
    "OpenCSD warning")

CTRACE_MESSAGE(TraceSkipNoSourceId,
    "{0} bytes skipped due to missing source ID; first formatter group at raw offset {1}",
    "{0} bytes skipped: no source ID")

CTRACE_MESSAGE(TraceSkipNullSourceId,
    "{0} bytes skipped for null source ID 0; first formatter group at raw offset {1}",
    "{0} bytes skipped: null source ID")

CTRACE_MESSAGE(TraceSkipReservedSourceId,
    "{0} bytes skipped for reserved source ID {2}; first formatter group at raw offset {1}",
    "{0} bytes skipped: reserved source ID")

CTRACE_MESSAGE(TraceSkipUnconfiguredSourceId,
    "{0} bytes skipped for unconfigured source ID {2}; first formatter group at raw offset {1}",
    "{0} bytes skipped: unconfigured source ID")

CTRACE_MESSAGE(TraceSkipMissingSync,
    "{0} bytes skipped due to missing SYNC; first formatter group at raw offset {1}",
    "{0} bytes skipped: no SYNC")

CTRACE_MESSAGE(TraceUnknownSourceId,
    "unknown",
    nullptr)

CTRACE_MESSAGE(TraceDecodeAbort,
    "decode aborted after processing {0} input bytes; trace is incomplete: {1}",
    "Decode aborted; trace incomplete; {1}")

CTRACE_MESSAGE(TraceDecodeAbortOpaque,
    "decode aborted after processing {0} input bytes; trace is incomplete: {1}",
    "Decode aborted; trace incomplete")

CTRACE_MESSAGE(TraceOverflow,
    "overflow: new timestamp segment; time across boundary may be unreliable",
    "Trace overflow; timestamp discontinuity")

CTRACE_MESSAGE(TracePacketPreview,
    "packet={0}, packet_size={1}, packet_bytes={2}, packet_bytes_truncated={3}",
    nullptr)

CTRACE_MESSAGE(TracePacketBytesUnavailable,
    "unavailable",
    nullptr)

CTRACE_MESSAGE(TracePacketBytes,
    "[{0}]",
    nullptr)

CTRACE_MESSAGE(TracePacketKindReserved,
    "RESERVED",
    nullptr)

CTRACE_MESSAGE(TracePacketKindIncomplete,
    "INCOMPLETE_EOT",
    nullptr)

CTRACE_MESSAGE(TracePacketKindBadSequence,
    "BAD_SEQUENCE",
    nullptr)

CTRACE_MESSAGE(TracePacketKindAsync,
    "ASYNC",
    nullptr)

CTRACE_MESSAGE(TracePacketKindOverflow,
    "OVERFLOW",
    nullptr)

CTRACE_MESSAGE(TracePacketKindSoftware,
    "SWIT",
    nullptr)

CTRACE_MESSAGE(TracePacketKindDwt,
    "DWT",
    nullptr)

CTRACE_MESSAGE(TracePacketKindLocalTimestamp,
    "TS_LOCAL",
    nullptr)

CTRACE_MESSAGE(TracePacketKindGlobalTimestamp1,
    "TS_GLOBAL_1",
    nullptr)

CTRACE_MESSAGE(TracePacketKindGlobalTimestamp2,
    "TS_GLOBAL_2",
    nullptr)

CTRACE_MESSAGE(TracePacketKindExtension,
    "EXTENSION",
    nullptr)

CTRACE_MESSAGE(TraceSetupAlreadyActive,
    "another OpenCSD DecodeTree session is already active",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupUnsupportedSource,
    "unsupported OpenCSD DecodeTree source type",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupMissingCreator,
    "OpenCSD DecodeTree creator is not configured",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupMissingDestroyer,
    "OpenCSD DecodeTree destroyer is not configured",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupCreateTree,
    "failed to create OpenCSD DecodeTree",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupCreateDecoder,
    "failed to create OpenCSD decoder",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupDecoderElement,
    "OpenCSD decoder element is not initialized",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupDecoderManager,
    "OpenCSD decoder manager is not initialized",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupFullDecoderComponent,
    "OpenCSD full decoder component is not initialized",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupPacketProcessor,
    "OpenCSD associated packet processor is not initialized",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupAttachPacketLogger,
    "failed to attach OpenCSD packet-processor error logger",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupAttachPacketMonitor,
    "failed to attach OpenCSD packet monitor",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupRawMonitorRequiresFormatted,
    "OpenCSD raw frame monitor requires a formatted DecodeTree",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupFrameDeformatter,
    "OpenCSD frame deformatter is not initialized",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupRawFrameAttachPoint,
    "OpenCSD raw-frame attach point is not initialized",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupAttachRawFrameMonitor,
    "failed to attach OpenCSD raw frame monitor",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupDecoderComponent,
    "OpenCSD decoder component is not initialized",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupResolveDecoderInput,
    "failed to resolve OpenCSD decoder input",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupDecoderInput,
    "OpenCSD decoder input is not initialized",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupSingleChannel,
    "OpenCSD SINGLE tree requires decoder channel 0",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupFormattedChannel,
    "OpenCSD formatted tree requires a decoder channel between 1 and 111",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupMissingSession,
    "OpenCSD ITM session factory returned no session",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupEmptyFormattedRoutes,
    "formatted OpenCSD ITM session requires at least one route",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupInvalidFormattedRoute,
    "formatted OpenCSD ITM route requires a Trace Bus ID between 1 and 111",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceSetupDuplicateFormattedRoute,
    "formatted OpenCSD ITM routes require unique Trace Bus IDs",
    "OpenCSD initialization failed")

CTRACE_MESSAGE(TraceOverflowSummary,
    "first overflow occurred at cycle timestamp {0}",
    "Trace overflow; timestamp discontinuity")

CTRACE_MESSAGE(TraceOverflowSummaryUnknown,
    "first overflow occurred at an unknown cycle timestamp",
    "Trace overflow; timestamp discontinuity")

CTRACE_MESSAGE(TraceOverflowSummaryMore,
    "{0}; {1} more occurred",
    nullptr)


// Detailed-only operational diagnostics and their parameterized wrappers.
CTRACE_MESSAGE(SpecifyTraceDirectory, "Specify <trace-dir>", nullptr)
CTRACE_MESSAGE(TraceDirectoryRequired, "trace directory job requires <trace-dir>", nullptr)
CTRACE_MESSAGE(TraceConfigurationPathEmpty, "trace-run configuration path is empty", nullptr)
CTRACE_MESSAGE(TraceConfigurationSourcePathMissing, "trace-run configuration has no source path", nullptr)
CTRACE_MESSAGE(ReferenceDiagnosticMissing, "trace generation setup failed without a diagnostic message", nullptr)
CTRACE_MESSAGE(SelectedTraceConfiguration, "selected trace-run configuration", nullptr)
CTRACE_MESSAGE(RawInputInspection, "raw input inspection context", nullptr)
CTRACE_MESSAGE(SkippingExcludedTraceChannel, "skipping raw trace channel excluded from active input selection", nullptr)
CTRACE_MESSAGE(AppliedTraceMetadata, "applied ctrace-run meta", nullptr)
CTRACE_MESSAGE(UsingTimestampPrescaler, "using timestamp prescaler", nullptr)
CTRACE_MESSAGE(SkippingUnsupportedTraceSource, "skipping unsupported formatted CoreSight trace source", nullptr)
CTRACE_MESSAGE(ItmDisabledChannel, "ITM data was received on a channel not enabled by ctrace-setup.itm.enable", nullptr)
CTRACE_MESSAGE(YamlRootMapRequired, "expected a YAML map containing 'ctrace-run'", nullptr)
CTRACE_MESSAGE(YamlRootMissing, "missing top-level 'ctrace-run' node", nullptr)
CTRACE_MESSAGE(YamlRootMapInvalid, "top-level 'ctrace-run' node must be a map", nullptr)
CTRACE_MESSAGE(TraceFormatInvalid, "'trace-format' must be 'unformatted' or 'formatted'", nullptr)
CTRACE_MESSAGE(ReferenceArrayMissing, "missing required 'ctrace-refs' array", nullptr)
CTRACE_MESSAGE(ReferenceLegacySourceUnsupported, "'source' is no longer supported; use 'index' in 'ctrace-refs' entries", nullptr)
CTRACE_MESSAGE(YamlDocumentCount, "expected exactly one YAML document", nullptr)
CTRACE_MESSAGE(ItmEnableMapRequired, "'itm' must be a map containing 'enable'", nullptr)
CTRACE_MESSAGE(TimestampsMapRequired, "'timestamps' must be empty or a map", nullptr)
CTRACE_MESSAGE(ReferenceDuplicateIndexLocated, "duplicate value in 'index' array", nullptr)
CTRACE_MESSAGE(ReferenceDuplicateIndex, "duplicate value in index array", nullptr)
CTRACE_MESSAGE(ReferenceStreamRangeLocated, "'stream' must be a CoreSight ATB trace ID between 1 and 111", nullptr)
CTRACE_MESSAGE(ReferenceStreamRange, "stream must be a CoreSight ATB trace ID between 1 and 111", nullptr)
CTRACE_MESSAGE(ReferenceItmIndexRangeLocated, "ITM 'index' must be between 0 and 31", nullptr)
CTRACE_MESSAGE(ReferenceItmIndexRange, "ITM index must be between 0 and 31", nullptr)
CTRACE_MESSAGE(SetupProcessorRequired, "pname is required for every ctrace-setup in a multi-processor configuration", nullptr)
CTRACE_MESSAGE(SingleProcessorBindingRequired, "unformatted SINGLE trace requires one unambiguous processor metadata binding", nullptr)
CTRACE_MESSAGE(ReferenceProcessorRequired, "pname is required for every ref in a multi-processor configuration", nullptr)
CTRACE_MESSAGE(IgnoringReferenceMultipleProcessors, "ignoring ref without pname because multiple ctrace-setup processors are active", nullptr)
CTRACE_MESSAGE(IgnoringReferenceAmbiguousProcessor, "ignoring ref without pname because its processor binding is ambiguous", nullptr)
CTRACE_MESSAGE(ConflictingDataSizes, "conflicting active ctrace-setup data.size values", nullptr)
CTRACE_MESSAGE(AmbiguousSingleClock, "unformatted SINGLE trace has ambiguous timestamps.clock values across processor candidates", nullptr)
CTRACE_MESSAGE(FormattedReferenceProcessorRequired, "pname is required for a formatted ref when multiple active ctrace-setup processors are available", nullptr)
CTRACE_MESSAGE(ItmAnchorPathRequired, "processor ITM route anchor path must use '[pname/]itm'", nullptr)
CTRACE_MESSAGE(ItmAnchorTypeRequired, "processor ITM route anchor must use reference type 'itm'", nullptr)
CTRACE_MESSAGE(TimestampReferenceType, "timestamps reference must use type 'itm' or transitional type 'dwt'", nullptr)
CTRACE_MESSAGE(ItmAnchorBusIdRequired, "processor ITM route anchor requires a CoreSight Trace Bus ID", nullptr)
CTRACE_MESSAGE(TimestampPrescalerRangeLocated, "'timestamps.itm-prescaler' must be one of 1, 4, 16, or 64", nullptr)
CTRACE_MESSAGE(TimestampPrescalerRange, "ctrace-setup timestamps.itm-prescaler must be one of 1, 4, 16, or 64", nullptr)
CTRACE_MESSAGE(ConflictingFormattedPrescalers, "conflicting timestamps.itm-prescaler values for one formatted processor ITM route", nullptr)
CTRACE_MESSAGE(ConflictingClocks, "conflicting active ctrace-setup timestamps.clock values", nullptr)
CTRACE_MESSAGE(IgnoringFormattedItmEnableConflict, "ignoring conflicting ctrace-setup itm.enable assignment for one formatted ITM route", nullptr)
CTRACE_MESSAGE(ActiveSetupProcessorRequired, "pname is required for active ctrace-setup fragments in a multi-processor configuration", nullptr)
CTRACE_MESSAGE(UnnamedSetupMultipleProcessors, "one unnamed ctrace-setup processor cannot bind multiple formatted pnames", nullptr)
CTRACE_MESSAGE(UnnamedSetupMultipleRoutes, "one unnamed ctrace-setup processor cannot bind multiple formatted ITM routes", nullptr)
CTRACE_MESSAGE(FormattedRouteAnchorRequired, "formatted trace input requires an ITM route anchor or supported feature fallback", nullptr)
CTRACE_MESSAGE(StreamlessReferenceAmbiguous, "streamless ref cannot be associated with one formatted ITM route", nullptr)
CTRACE_MESSAGE(ConflictingSinglePrescalers, "unformatted SINGLE trace has conflicting timestamps.itm-prescaler values for one processor", nullptr)
CTRACE_MESSAGE(IgnoringSingleItmEnableConflict, "ignoring conflicting ctrace-setup itm.enable values for unformatted SINGLE trace", nullptr)
CTRACE_MESSAGE(AmbiguousSinglePrescalers, "unformatted SINGLE trace cannot choose between different timestamps.itm-prescaler values", nullptr)
CTRACE_MESSAGE(IgnoringProcessorItmEnableConflict, "ignoring different ctrace-setup itm.enable values across processor candidates for unformatted SINGLE trace", nullptr)
CTRACE_MESSAGE(CtfConflictingSourceMetadata, "CTF metadata cannot describe conflicting active metadata for one route/type/source key", nullptr)
CTRACE_MESSAGE(CtfConfiguredClockInvalid, "CTF output cannot use the configured timestamps.clock", nullptr)
CTRACE_MESSAGE(CtfClockRequired, "CTF output requires timestamps.clock; no default is assumed", nullptr)
CTRACE_MESSAGE(CtfClockPositive, "CTF output requires timestamps.clock to be greater than zero", nullptr)
CTRACE_MESSAGE(CtfConfiguredAddressInvalid, "CTF output cannot use the configured ctrace-run address", nullptr)
CTRACE_MESSAGE(CtfConfiguredDataTypeInvalid, "CTF output cannot use the configured ctrace-run data-type", nullptr)
CTRACE_MESSAGE(CtfConfiguredSizeInvalid, "CTF output cannot use the configured ctrace-run size", nullptr)
CTRACE_MESSAGE(CtfComparatorRange, "CTF output requires DWT comparator sources between 0 and 3", nullptr)
CTRACE_MESSAGE(CtfAddressRangeInvalid, "CTF output cannot represent the configured DWT address range", nullptr)
CTRACE_MESSAGE(UnknownException, "unknown exception", nullptr)
CTRACE_MESSAGE(CsvFilePathRequired, "CSV output path must identify a file", nullptr)
CTRACE_MESSAGE(CsvStreamFactoryRequired, "CSV stream factory must be configured", nullptr)
CTRACE_MESSAGE(CtfPayloadExceedsSize, "CTF record payload exceeds its declared size", nullptr)
CTRACE_MESSAGE(CtfRecordTooLarge, "CTF record does not fit into a packet", nullptr)
CTRACE_MESSAGE(CtfPayloadShorterThanSize, "CTF record payload is shorter than its declared size", nullptr)
CTRACE_MESSAGE(CtfRouteIdentityMismatch, "CTF route identity does not match the configured normalized route catalogue", nullptr)
CTRACE_MESSAGE(CtfAddressPayloadSizeInvalid, "CTF DWT address fragment has an invalid SWO payload size", nullptr)
CTRACE_MESSAGE(CtfConfiguredRouteIdentityConflict, "CTF configuration contains inconsistent normalized route identities", nullptr)
CTRACE_MESSAGE(CtfRuntimeStreamRequired, "CTF binary output cannot encode an event route without an exact runtime stream descriptor", nullptr)
CTRACE_MESSAGE(CtfItmPayloadSizeInvalid, "CTF ITM value has an invalid SWO payload size", nullptr)
CTRACE_MESSAGE(CtfConfiguredPayloadSizeMismatch, "configured ctrace-run size does not match the decoded SWO payload size", nullptr)
CTRACE_MESSAGE(XmlMultipleClockDomains, "Trace Compass XML views were not generated for this input because emitted CTF streams use multiple clock domains", nullptr)
CTRACE_MESSAGE(XmlViewSelectionInvalid, "Trace Compass XML contains an unsupported route view selection", nullptr)
CTRACE_MESSAGE(XmlTraceBusIdRange, "Trace Compass XML requires Trace Bus IDs between 0 and 111", nullptr)
CTRACE_MESSAGE(XmlCanonicalClockUuidRequired, "Trace Compass XML requires canonical lower-case clock UUIDs", nullptr)
CTRACE_MESSAGE(XmlViewRouteRequired, "Trace Compass XML requires at least one view route", nullptr)
CTRACE_MESSAGE(XmlUniqueClockRouteRequired, "Trace Compass XML requires unique clock UUID and Trace Bus ID pairs", nullptr)
CTRACE_MESSAGE(XmlGraphicalViewRequired, "Trace Compass XML requires at least one graphical view", nullptr)
CTRACE_MESSAGE(CtfExceptionStreamUnknown, "CTF exception observation references an unknown stream class", nullptr)
CTRACE_MESSAGE(CtfGraphicalStreamUnknown, "CTF graphical-topic observation references an unknown stream class", nullptr)
CTRACE_MESSAGE(CtfDuplicateClockId, "CTF metadata contains a duplicate clock-domain ID", nullptr)
CTRACE_MESSAGE(CtfClockNamesInvalid, "CTF metadata requires unique valid clock-domain names", nullptr)
CTRACE_MESSAGE(CtfClockFrequencyNonzero, "CTF metadata requires a non-zero clock-domain frequency", nullptr)
CTRACE_MESSAGE(CtfClockUuidsDistinct, "CTF clock UUIDs must be distinct from the trace and other clock domains", nullptr)
CTRACE_MESSAGE(CtfDuplicateRoute, "CTF metadata contains a duplicate normalized route", nullptr)
CTRACE_MESSAGE(CtfInconsistentRoutes, "CTF metadata contains inconsistent normalized route identities", nullptr)
CTRACE_MESSAGE(CtfDuplicateStreamId, "CTF metadata contains a duplicate stream-class ID", nullptr)
CTRACE_MESSAGE(CtfStreamClockUnknown, "CTF stream class references an unknown clock domain", nullptr)
CTRACE_MESSAGE(CtfRouteBusIdRange, "CTF ITM stream route requires a CoreSight ATB trace ID between 1 and 111", nullptr)
CTRACE_MESSAGE(CtfStreamRouteMismatch, "CTF stream-class ID does not match its normalized route identity", nullptr)
CTRACE_MESSAGE(CtfUnreferencedClock, "CTF metadata contains a clock domain without a referencing stream class", nullptr)
CTRACE_MESSAGE(CtfSourceRouteUnknown, "CTF source metadata references an unknown normalized route", nullptr)
CTRACE_MESSAGE(CtfItmChannelRange, "CTF ITM source metadata requires a channel between 1 and 31", nullptr)
CTRACE_MESSAGE(CtfDwtComparatorRange, "CTF DWT source metadata requires a comparator between 0 and 3", nullptr)
CTRACE_MESSAGE(CtfDataTypeSizeInvalid, "CTF DWT source metadata has an invalid data-type/size combination", nullptr)
CTRACE_MESSAGE(CtfSourceAddressOverflow, "CTF DWT source address range exceeds the unsigned 64-bit metadata domain", nullptr)
CTRACE_MESSAGE(CtfSourceTypeInvalid, "CTF source metadata type must be 'itm' or 'dwt'", nullptr)
CTRACE_MESSAGE(CtfDuplicateSource, "CTF metadata contains duplicate source metadata", nullptr)
CTRACE_MESSAGE(CtfConflictingSource, "CTF metadata contains conflicting source metadata for one route", nullptr)
CTRACE_MESSAGE(CtfExplicitClockUuidRequired, "non-legacy CTF clock domains require an explicit UUID", nullptr)
CTRACE_MESSAGE(RawDecodeChunkTooLarge, "raw decode chunk is too large", nullptr)
CTRACE_MESSAGE(CortexRoutesRequired, "Cortex-M stream decoding requires at least one normalized route", nullptr)
CTRACE_MESSAGE(TimestampPrescalerPositive, "ITM timestamp prescaler must be greater than zero", nullptr)
CTRACE_MESSAGE(TimestampPrescalerNonzero, "ITM timestamp prescaler must not be zero", nullptr)
CTRACE_MESSAGE(CortexTraceBusIdRange, "formatted Cortex-M route requires a CoreSight ATB trace ID between 1 and 111", nullptr)
CTRACE_MESSAGE(CortexDuplicateTraceBusId, "duplicate CoreSight ATB trace ID in Cortex-M route configuration", nullptr)
CTRACE_MESSAGE(CortexDuplicateRouteId, "duplicate normalized route ID in Cortex-M route configuration", nullptr)
CTRACE_MESSAGE(CortexRouteIdentityMismatch, "OpenCSD element route identity does not match normalized route catalogue", nullptr)
CTRACE_MESSAGE(OpenCsdDecoderFinished, "OpenCSD ITM decoder already finished", nullptr)
CTRACE_MESSAGE(RawTraceDataNull, "raw trace data pointer is null while bytes are present", nullptr)
CTRACE_MESSAGE(OpenCsdRoutesRequired, "OpenCSD ITM decoding requires at least one normalized route", nullptr)
CTRACE_MESSAGE(OpenCsdSingleRouteRequired, "OpenCSD SINGLE decoding requires exactly one normalized route", nullptr)
CTRACE_MESSAGE(PacketRoutesRequired, "formatted OpenCSD packet collection requires at least one normalized route", nullptr)
CTRACE_MESSAGE(PacketTraceBusIdRange, "formatted OpenCSD packet route requires a Trace Bus ID between 1 and 111", nullptr)
CTRACE_MESSAGE(PacketDuplicateRouteId, "duplicate normalized route ID in formatted OpenCSD packet routes", nullptr)
CTRACE_MESSAGE(PacketDuplicateTraceBusId, "duplicate Trace Bus ID in formatted OpenCSD packet routes", nullptr)
CTRACE_MESSAGE(PacketUnknownCutoffRoute, "route-aware OpenCSD transaction cutoff references an unknown normalized route", nullptr)
CTRACE_MESSAGE(PacketUnknownDiagnosticRoute, "OpenCSD diagnostic references an unknown normalized route", nullptr)
CTRACE_MESSAGE(PacketUnknownDataLossRoute, "OpenCSD data-loss interval references an unknown normalized route", nullptr)
CTRACE_MESSAGE(PacketUnknownRawRoute, "raw OpenCSD packet references an unknown normalized route", nullptr)
CTRACE_MESSAGE(PacketUnknownFormattedRoute, "formatted data references an unknown normalized route", nullptr)
CTRACE_MESSAGE(DiagnosticLocation, "{0}: {1}", nullptr)
CTRACE_MESSAGE(DiagnosticLineLocation, "{0}({1}): {2}", nullptr)
CTRACE_MESSAGE(DiagnosticUnknownNormalizedRoute, "OpenCSD element references unknown normalized route {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathTraceConfigMissing, "trace-run configuration not found: {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathTraceDirectoryMissing, "trace directory not found: {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathRawInputMissing, "no eligible raw trace input found for solution-set {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathRawInputNotRegular, "raw trace input is not a regular file: {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathRawInputUnreadable, "raw trace input is not readable: {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathRawInputReadFailed, "failed to read input file: {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathArtifactNamesInvalid, "cannot derive trace artifact names from {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathCsvInspect, "Failed to inspect existing CSV output {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathCsvRefuseDirectory, "Refusing to replace CSV output because the target is a directory: {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathCsvRemove, "Failed to remove existing CSV output {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathCsvCreateDirectory, "Failed to create CSV output directory {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathCsvOpen, "Failed to open CSV output {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathCsvWrite, "Failed to write CSV output {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathCtfInspect, "Failed to inspect existing CTF output {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathCtfRefuseFile, "Refusing to replace CTF output because the target is not a directory: {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathCtfRemove, "Failed to remove CTF directory {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathCtfCreateDirectory, "Failed to create CTF output directory {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathCtfStreamOpen, "Failed to open CTF stream {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathCtfStreamWrite, "Failed to write CTF stream in {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathCtfMetadataWrite, "Failed to write CTF metadata {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathXmlInspect, "Failed to inspect existing Trace Compass XML {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathXmlRefuseDirectory, "Refusing to replace Trace Compass XML because the target is a directory: {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathXmlRemove, "Failed to remove existing Trace Compass XML {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathXmlWrite, "Failed to write Trace Compass XML {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathXmlClockUuidMissing, "Trace Compass XML requires an explicit clock UUID for input {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPathXmlClockUuidShared, "Trace Compass XML requires independent clock UUIDs for separate inputs: {0}", nullptr)
CTRACE_MESSAGE(DiagnosticYamlParse, "failed to parse trace-run configuration: {0}: {1}", nullptr)
CTRACE_MESSAGE(DiagnosticYamlParseLine, "failed to parse trace-run configuration: {0}({1}): {2}", nullptr)
CTRACE_MESSAGE(DiagnosticYamlParseColumn, "failed to parse trace-run configuration: {0}({1},{2}): {3}", nullptr)
CTRACE_MESSAGE(DiagnosticNativeDetail, "{0}: {1}", nullptr)
CTRACE_MESSAGE(DiagnosticBackendFailure, "{0} output failed during {1}: {2}", nullptr)
CTRACE_MESSAGE(DiagnosticTargetBackendFailure, "{0} output '{1}' failed during {2}: {3}", nullptr)
CTRACE_MESSAGE(DiagnosticDecodeSummary, "processed {0} input bytes in {1} s ({2} MiB/s); trace/diagnostic records: {3}", nullptr)
CTRACE_MESSAGE(DiagnosticInvalidConfigFieldProblem, "unknown configuration field problem", nullptr)
CTRACE_MESSAGE(DiagnosticInvalidProcessorBindingProblem, "unknown processor binding problem", nullptr)
CTRACE_MESSAGE(DiagnosticInvalidTraceBusBindingProblem, "unknown trace bus binding problem", nullptr)
CTRACE_MESSAGE(DiagnosticInvalidPathCode, "unknown path diagnostic code", nullptr)
CTRACE_MESSAGE(DiagnosticCtfValueRequirements, "supported data-type values are 'unsigned', 'signed', and 'float'; size must be 1, 2, or 4, and float requires size 4", nullptr)
CTRACE_MESSAGE(DiagnosticUnsignedArgument, "{0} must be an unsigned integer, got {1}", nullptr)
CTRACE_MESSAGE(DiagnosticMissingArgumentValue, "Missing value for {0}", nullptr)
CTRACE_MESSAGE(DiagnosticArgumentValueSeparator, "{0} values must be separated by spaces", nullptr)
CTRACE_MESSAGE(DiagnosticUnknownArgument, "Unknown argument: {0}", nullptr)
CTRACE_MESSAGE(DiagnosticPositionalArgument, "Unexpected positional argument: {0}", nullptr)
CTRACE_MESSAGE(DiagnosticStreamArgument, "--stream must be a CoreSight Trace Bus ID between 0 and 111, got {0}", nullptr)
CTRACE_MESSAGE(DiagnosticTypeArgument, "Invalid --type value: {0} (accepted: {1})", nullptr)
CTRACE_MESSAGE(DiagnosticTargetArgument, "--target must be a solution-set name, got {0}", nullptr)
CTRACE_MESSAGE(DiagnosticConfigFieldScalarString, "'{0}' must be a scalar string", nullptr)
CTRACE_MESSAGE(DiagnosticConfigFieldScalarUnsigned, "'{0}' must be a scalar unsigned integer", nullptr)
CTRACE_MESSAGE(DiagnosticConfigFieldUnsigned, "'{0}' must be an unsigned integer", nullptr)
CTRACE_MESSAGE(DiagnosticConfigFieldUnsignedRange, "'{0}' must be an unsigned integer in range", nullptr)
CTRACE_MESSAGE(DiagnosticConfigFieldArray, "'{0}' must be an array", nullptr)
CTRACE_MESSAGE(DiagnosticConfigFieldScalar, "'{0}' must be a scalar value", nullptr)
CTRACE_MESSAGE(DiagnosticConfigFieldStringOrList, "'{0}' must be a string or list of strings", nullptr)
CTRACE_MESSAGE(DiagnosticConfigFieldStringEntry, "each '{0}' entry must be a string", nullptr)
CTRACE_MESSAGE(DiagnosticConfigFieldMapEntry, "each '{0}' entry must be a map", nullptr)
CTRACE_MESSAGE(DiagnosticConfigFieldUnsignedEntry, "each '{0}' entry must be an unsigned integer", nullptr)
CTRACE_MESSAGE(DiagnosticConfigFieldRequiredReferenceScalar, "missing required '{0}' scalar in 'ctrace-refs' entry", nullptr)
CTRACE_MESSAGE(DiagnosticProcessorIgnoredUnmatchedSetup, "ignoring ref pname '{0}' because it has no matching ctrace-setup", nullptr)
CTRACE_MESSAGE(DiagnosticProcessorConflictingReferencePath, "ref path processor conflicts with pname '{0}'", nullptr)
CTRACE_MESSAGE(DiagnosticProcessorNoActiveSetup, "ref pname '{0}' has no matching active ctrace-setup processor", nullptr)
CTRACE_MESSAGE(DiagnosticProcessorMultipleBusIds, "processor '{0}' has ITM routes bound to multiple CoreSight Trace Bus IDs", nullptr)
CTRACE_MESSAGE(DiagnosticIgnoredProcessorMismatch, "ignoring ref pname '{0}' because it does not match ctrace-setup pname '{1}'", nullptr)
CTRACE_MESSAGE(DiagnosticDisabledReference, "ref '{0}' resolves only to disabled ctrace-setup fragment {1}", nullptr)
CTRACE_MESSAGE(DiagnosticTraceBusConflictingProcessors, "CoreSight Trace Bus ID {0} has conflicting ITM processor bindings", nullptr)
CTRACE_MESSAGE(DiagnosticTraceBusMissingAnchor, "ref describes CoreSight Trace Bus ID {0} without an ITM route anchor or supported feature fallback", nullptr)
CTRACE_MESSAGE(DiagnosticConfigurationFilesMissing, "no *{0} files found in trace directory: {1}", nullptr)
CTRACE_MESSAGE(DiagnosticConfigurationFilename, "expected <solution-set>{0}, got {1}", nullptr)
CTRACE_MESSAGE(DiagnosticRawInputAlignment, "formatted raw trace input size must be a multiple of {0} bytes: {1} (size={2})", nullptr)
CTRACE_MESSAGE(DiagnosticSpecificOutputTarget, "{0} must identify a specific output path", nullptr)
CTRACE_MESSAGE(DiagnosticOutputParent, "{0} parent is not a directory: {1}", nullptr)
CTRACE_MESSAGE(DiagnosticCtfDataType, "CTF output cannot use ctrace-run data-type '{0}'; {1}", nullptr)
CTRACE_MESSAGE(DiagnosticCtfDataSize, "CTF output cannot use ctrace-run size {0} with data-type '{1}'; {2}", nullptr)

// Formatter contract failures use direct lookup to avoid recursive formatting.
CTRACE_MESSAGE(MessageInvalidId, "invalid message ID", nullptr)
CTRACE_MESSAGE(MessageInvalidStyle, "invalid message style", nullptr)
CTRACE_MESSAGE(MessageNullArgument, "message argument must not be null", nullptr)
CTRACE_MESSAGE(MessageArgumentCountMismatch, "incorrect message argument count", nullptr)
