# Internal ctrace Design Constraints

This is maintainer documentation for implementation boundaries and regression
tests. User-facing operation belongs in the [ctrace README](../README.md). The
CMSIS-Toolbox [trace specification](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/Experimental-Features.md#file-structure-of-ctrace-runyml)
defines the external `*.ctrace-run.yml` format and remains authoritative; this
document does not redefine it.

The constraints below describe only ctrace-specific implementation decisions.
Each is implemented in the current code and covered by focused CTest cases.

## Architecture

- **OpenCSD details remain inside the decode layer; outputs consume semantic `TraceEvent` values.**
  OpenCSD includes occur only below [`src/decode`](../src/decode); output targets depend on `model`, not OpenCSD.

- **YAML syntax and nodes do not escape the trace-run reader boundary.**
  YAML types occur only in
  [`YmlTraceRunConfigReader.cpp`](../src/tracerun/YmlTraceRunConfigReader.cpp); normalized configuration and metadata
  cross the boundary.

- **The YAML reader validates the fields consumed by ctrace and ignores unrelated extensions.**
  `TraceRunReaderConsumesOnlyRelevantFields` and `TraceRunReaderValidatesConsumedFields` cover both sides.

- **Production dependencies retain seven explicit, cycle-free module boundaries and point toward lower-level
  modules.** [`src/CMakeLists.txt`](../src/CMakeLists.txt) defines model, CLI, trace-run, diagnostics, decode, output,
  and control targets. The executable adds the platform-independent `CtraceMain` entry point directly; raw-file
  reading remains private to control rather than creating another target.

- **CSV and CTF requirements are evaluated independently after shared decode configuration.**
  `testOutputRequirementsAreBackendSpecific` verifies that CTF-only failures do not disable valid CSV output.

## Input and decoding

- **Raw SWO input is authoritative; recovery never rewrites input or injects synthetic synchronization.**
  `testDecodePipelineDoesNotInjectSync`, `testDecodePipelineRecoversAtRealSync`, and
  `testDecodePipelineCountsBytesUntilRealSync` exercise the rule.

- **File-read boundaries are not packet boundaries; partial OpenCSD state survives across chunks.**
  `FileDecodeJob` reads 64 KiB blocks and passes non-owning `RawByteView` values to `DecodePipeline`.
  `testDecodePipelineRecoversWhenErrorSpansChunks` and the persistent `OpenCsdItmSession` cover chunked input.

- **Incomplete input and unrecoverable decoder responses remain visible errors, not successful packets.**
  `testDecodePipelineReportsIncompletePacketAtEndOfInput` and `testOpenCsdErrorControllerClassifiesResponses` verify
  classification.

- **Overflow, reset, synchronization, and recovery boundaries cannot retain stale DWT state silently.**
  `testCortexMPostDecoderOverflowFlushesDwtSegments` and post-decoder reset paths flush or clear pending state.

- **Unformatted input uses Trace Bus ID `0`; formatted IDs are restricted to `1` through `111`.**
  [`TraceStreamId.h`](../src/model/TraceStreamId.h), `testTraceSelection`, and `testCliParser` enforce the domains.

- **ITM timestamp prescalers default to `1`, accept `1`, `4`, `16`, or `64`, and remain stream-specific.**
  `testTimestampPrescalerMetadataDefaults`, `testCortexMStreamDecoderAppliesPerStreamPrescalers`, and metadata tests
  cover the behavior.

## Events, diagnostics, and output

- **The first public output profile contains exactly `itm`, `dwt`, `exception`, `global_ts`, `overflow`, and
  `error`.** `TraceSelection` owns this stable name set. Event-counter and PMU events have no public output type, and
  periodic PC samples remain suppressed; `testTraceSelection`, `testCliParser`, `testCsvRowMapper`, and
  `testDwtPcSampleIsSuppressedUntilDedicatedEventExists` enforce the boundary.

- **Decoder warnings and errors remain observable independently of payload filtering; errors fail the command.**
  `testTraceIssueReporterReportsEveryIssue` and
  `testDecodeConsumersForwardsWarningsAndFailsOnErrorsWithOutputs` verify ordering and impact.

- **ITM channel `0` is excluded from trace artifacts, but associated decoder errors remain visible.**
  `testTraceSelection` and `testCtfBundleOutputExcludesSoftwareChannelZero` cover selection and CTF metadata.

- **CTF timestamps never regress; global timestamps do not establish local timestamp quality.**
  `testCtfHoldsRegressingEventTimestamps` and `testCtfGlobalTimestampDoesNotEstablishLocalTimeQuality` verify both.

- **Each output owns a direct lifecycle and removes incomplete artifacts after failure.**
  `testCsvFileOutputWritesDirectly`, `testCtfBundleOutputAbortRemovesPartialBundle`, and lifecycle tests cover cleanup
  and independent completion.

- **Existing directories or overlapping CTF/XML targets are rejected before destructive replacement.**
  `testCtfBundleOutputRejectsOverlappingTargetsBeforeDeletion` and CSV path tests cover target safety.

- **Validation-only mode creates no output; unsupported trace channels are diagnosed and skipped.**
  `TraceDirectoryJob`, `ctrace-trace-run-yaml`, and the SWO/TB fixture integration test enforce the workflow.

- **Process success comes from structured diagnostic impact, not formatted stderr text.**
  [`DiagnosticSink`](../src/diagnostics/DiagnosticSink.h) tracks failure impact independently of the rendered
  severity; `CtraceMain` returns failure when the failure count is non-zero.

## Distribution

- **The executable version and release workflow are tool-specific.** A release tag named `tools/ctrace/<version>`
  supplies the compiled version and selects the ctrace workflow. Its matrix builds Windows AMD64/Arm64, Linux
  AMD64/Arm64, and macOS Arm64 artifacts. Tests execute on Windows AMD64 and Linux AMD64; ARM64 targets are compiled
  but not executed, matching the other devtools workflows.

- **Product dependencies come from devtools and carry repository-level license records.** `cxxopts`, `yaml-cpp`, and
  the pinned OpenCSD submodule are listed in the top-level `LICENSE.md`. The release archive includes the project
  license, [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md), OpenCSD source-file copyright notices, application
  dependency license texts, and verified per-file checksums. The release workflow assembles this archive with the same
  explicit download/copy/zip stages used by the other devtools workflows. Runtime contents remain subject to
  per-artifact inspection rather than being implicitly covered by the application dependency list.

- **The Blinky SWO and TB captures are redistributable ctrace test assets.** Their purpose, origin, derived golden CSV,
  and SPDX sidecars are documented in the [fixture README](../test/data/README.md); generated test output remains
  confined to the build tree.

## Maintenance rule

When changing one of these contracts, update its focused unit test, the relevant integration scenario, this document,
and the [architecture description](architecture.md) in the same change.
