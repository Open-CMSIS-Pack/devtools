# ctrace Architecture

This document describes the internal structure of `ctrace`, the runtime data flow, and the intended extension points.
For command-line usage, build instructions, and release information, see the [project README](../README.md). The
[verified constraints](constraints.md) record preserved contracts; the compact [TODO list](todo.md) tracks remaining
work.

![ctrace architecture](architecture.svg)

## Scope

`ctrace` combines a trace-run configuration with raw CoreSight trace data and converts supported trace channels into
backend-independent semantic events. Output backends consume these events to create CSV or CTF artifacts.

The first release profile supports SWO data containing ITM and DWT packets. Its stable output types are `itm`, `dwt`,
`exception`, `global_ts`, `overflow`, and `error`. DWT event-counter and PMU packets are retained internally but do
not have public output semantics yet; periodic PC samples remain disabled. Trace Bus input is discovered so that a
complete trace directory can be inspected, but `*.TB.raw` files are reported and skipped until a decoder is
implemented.

The architecture separates protocol decoding, semantic interpretation, and output generation. This keeps output
formats independent of OpenCSD and allows another raw trace channel to reuse the event model and output backends.

## How it works at a glance

`ctrace` processes one solution set at a time. The common base name joins configuration and trace data; for example,
`Board.ctrace-run.yml` describes the sources and timing metadata required to decode `Board.SWO.raw`. A matching
`Board.TB.raw` is discovered as part of the same solution set but is skipped by the first release profile.

The main in-memory path is:

```text
command line + trace-run YAML + SWO raw file
                    |
                    v
        TraceDirectoryJob / FileDecodeJob
          discovery, metadata, output plan
                    |
                    v
           64 KiB non-owning byte views
                    |
                    v
                OpenCSD
       ITM framing, packets, recovery
                    |
                    v
          OpenCsdTraceElement values
                    |
                    v
 CortexMStreamDecoder / CortexMPostDecoder
     timestamps, DWT pairing, quality
                    |
                    v
             TraceEvent values
                    |
                    v
              DecodeConsumers
        /               |               \
 diagnostics        CSV backend       CTF backend
```

The `TraceEvent` boundary is the central design point. Before it, code handles byte offsets, OpenCSD packets, decoder
recovery, and Cortex-M state. After it, code sees backend-independent events in decode order and does not depend on
OpenCSD types.

| Stage | Owner | Transformation |
| --- | --- | --- |
| Discover | `TraceDirectoryJob` | Trace directory and target selection to solution-set configuration and raw inputs |
| Prepare | `CtraceRunMeta`, `FileDecodeJob` | YAML representation to normalized runtime metadata and an output plan |
| Decode protocol | `DecodePipeline`, `OpenCsdItmDecoder` | Raw byte chunks to recoverable OpenCSD trace elements |
| Interpret | `CortexMStreamDecoder`, `CortexMPostDecoder` | Protocol elements to timestamped semantic events |
| Consume | `DecodeConsumers` | One ordered event stream to diagnostics and every enabled output backend |
| Complete | `TraceOutputLifecycle` | Complete active artifacts, or remove them after output or decoder failure |

## Runtime flow

1. `CtraceMain` parses and validates the command line.
2. `TraceRunDiscovery` finds one or all `<solution-set>.ctrace-run.yml` files in the selected trace directory.
3. `YmlTraceRunConfigReader` reads fields used by `ctrace`; unrelated and unknown YAML fields are ignored.
4. `CtraceRunMeta` normalizes processor, timestamp, route, ITM, and DWT source metadata.
5. `TraceDirectoryJob` associates the configuration with matching raw trace channels.
6. `FileDecodeJob` creates the requested output plan and reads supported raw files in 64 KiB blocks.
7. `DecodePipeline` receives non-owning `RawByteView` values while preserving decoder state across every file-read
   boundary.
8. The OpenCSD adapter decodes ITM protocol elements and preserves decoder warnings, errors, and recovery boundaries.
9. The Cortex-M post-decoder converts protocol elements into semantic `TraceEvent` values.
10. `DecodeConsumers` forwards every event to diagnostics and the selected output backends.
11. Output lifecycle handling completes valid artifacts or removes incomplete artifacts after a failure.

Without `--csv`, `--ctf`, or `--all`, the same pipeline runs in validation-only mode without creating output files.

## Processing state and ownership

One `DecodePipeline` is created for each supported raw file. It owns the OpenCSD adapter and Cortex-M stream decoder,
so protocol, timestamp, and DWT state survive arbitrary file-read boundaries. `RawFileReader` owns a single 64 KiB
buffer; each `RawByteView` borrows that buffer only for the synchronous `DecodePipeline::push` call. Calling
`DecodePipeline::finish` flushes both the OpenCSD and Cortex-M layers before the pipeline is destroyed.

`CortexMStreamDecoder` maintains an independent post-decoder for each observed Trace Bus ID. All post-decoders emit
into the same `TraceEventSink`, preserving input order while keeping stream-specific timestamp and DWT state apart.

There is no application-wide event queue. `DecodeConsumers` forwards each event synchronously to the output
lifecycle and issue reporter. Output backends own their files and are isolated from one another: failure of one
backend aborts its incomplete artifact but does not directly stop another active backend. A non-recoverable decoder
error aborts every still-active output for that raw file.

The diagnostic sink lives for the complete command invocation. It therefore aggregates failures across solution sets
and determines the final process status after processing has continued wherever possible.

## Recovery after damaged trace

Recoverable OpenCSD packet errors establish a discontinuity at the reported raw-file offset. Decoder callbacks before
that offset are retained; callbacks at or after it belong to the failed transaction and are discarded. `ctrace` then
resets the OpenCSD decoder and feeds the following input bytes to it unchanged. OpenCSD resumes only after finding a
real ITM hardware-sync sequence; `ctrace` never inserts a synthetic sync sequence.

Bytes consumed while no usable trace elements are produced form one explicit `data-loss` interval. At its boundary,
the Cortex-M post-decoder flushes pending events, resets incomplete DWT correlation, and marks timestamps unreliable
until the stream supplies enough timing information again. The issue remains part of the ordered `TraceEvent` stream,
so diagnostics and enabled output backends observe the same recovery boundary.

Failure to reset OpenCSD, repeated lack of decoder progress, or an unsuccessful wait/flush operation aborts only the
current raw-file job. Other solution sets continue to be processed where possible.

## Suggested code-reading path

1. Start at [`CtraceMain.cpp`](../src/CtraceMain.cpp) for command-line handling and top-level error policy.
2. Follow [`TraceDirectoryJob.cpp`](../src/control/TraceDirectoryJob.cpp) to see how solution sets, YAML, SWO, and
   unsupported Trace Bus input are associated.
3. Read [`FileDecodeJob.cpp`](../src/control/FileDecodeJob.cpp) for output preflight, chunked input, pipeline
   construction, and finalization.
4. Continue through [`DecodePipeline.cpp`](../src/decode/DecodePipeline.cpp),
   [`OpenCsdItmDecoder.cpp`](../src/decode/OpenCsdItmDecoder.cpp), and
   [`CortexMStreamDecoder.cpp`](../src/decode/CortexMStreamDecoder.cpp) for the two decode representations.
5. Use [`TraceEvent.h`](../src/model/TraceEvent.h) as the semantic contract between decoding and all consumers.
6. Finish with [`DecodeConsumers.cpp`](../src/control/DecodeConsumers.cpp) and
   [`TraceOutputLifecycle.cpp`](../src/output/TraceOutputLifecycle.cpp), then inspect either the CSV or CTF backend.

This path follows one trace file through the system without requiring the build-target graph or every backend detail
up front.

## Module boundaries

### Entry point and orchestration

| Module | Responsibility |
| --- | --- |
| `src/CtraceMain.h` | Platform-independent entry point used by the executable trampoline |
| `src/cli` | Command-line parsing, value normalization, and validation |
| `src/control` | Solution-set orchestration, raw-file access, output setup, and per-file decode jobs |
| `src/diagnostics` | Structured diagnostics, severity tracking, and decoder issue reporting |

`control` is the composition layer. The raw-file reader is a private implementation detail of `FileDecodeJob`, not a
separate module or public abstraction. Control may depend on the other application modules, while lower-level modules
must not depend on control jobs or command-line details.

### Trace-run metadata

| Module | Responsibility |
| --- | --- |
| `src/tracerun` | File discovery, YAML parsing, schema subset validation, and normalized metadata |

The YAML reader intentionally consumes only data required by `ctrace`. A malformed consumed field is an error, while
an unknown field is ignored. This permits compatible trace-run format extensions without weakening validation of the
data used for decoding or output generation.

`CtraceRunMeta` is the boundary between the YAML representation and runtime processing. Decode and output modules use
normalized metadata instead of navigating YAML nodes.

### Decode and event model

| Module | Responsibility |
| --- | --- |
| `src/decode` | Raw-byte decoder interface, OpenCSD integration, packet recovery, timestamps, and Cortex-M semantics |
| `src/model` | Backend-independent event types, quality information, and event selection |

OpenCSD is isolated behind adapter classes in `src/decode`. OpenCSD-specific elements do not escape into the output
modules. The post-decoder maps them to `TraceEvent` variants such as software trace, DWT data, exceptions, timestamps,
overflow, and trace issues. `TraceSelection` owns the stable public type names and maps semantic events onto that
release-facing set.

Each event can retain its raw index, Trace Bus ID, timestamp, and quality state. This allows diagnostics and backends
to make independent decisions without reconstructing decoder state.

### Output

| Module | Responsibility |
| --- | --- |
| `src/output` | Backend requirements, output planning, and lifecycle management |
| `src/output/csv` | Stable CSV schema, row mapping, filtering, and file output |
| `src/output/ctf` | CTF metadata and stream encoding plus Trace Compass analysis XML |

Output requirements are evaluated per backend. For example, missing CTF-specific metadata may disable CTF while an
independent CSV output remains valid. `--all` therefore does not make the backends share failure state unnecessarily.

Outputs use an explicit `start`, `writeEvent`, `stop`, and `abort` lifecycle. A successful backend can finish even if
another backend fails. Decode or finalization failures trigger cleanup of incomplete artifacts.

## Diagnostics and failure semantics

Diagnostics carry a severity, category, code, message, context, and impact. Severity describes the issue, while impact
determines whether the current job must fail. This distinction allows a trace-run generation error to remain visible
without necessarily preventing the decoding of otherwise valid trace input.

Decoder issue packets remain part of the event stream. They can therefore be written to CSV or CTF and reported to
stderr independently of payload filters. Repeated issues are not silently collapsed.

Processing continues with other solution sets where possible. Errors are rendered as `error` even when their impact
causes a non-zero exit status; `fatal` is reserved for an internal ctrace crash.

## External dependencies

| Dependency | Use |
| --- | --- |
| `cxxopts` | Command-line parsing |
| `yaml-cpp` | Trace-run YAML parsing |
| `OpenCSD` 1.8.3 | ITM protocol decoding; pinned as a repository submodule |
| GoogleTest | Unit-test framework; not linked into the product executable |

Dependencies are provided by the devtools repository. `tools/ctrace` does not maintain private library copies.
The pinned OpenCSD source is built without a downstream source patch. A known unsafe empty-buffer access in an
upstream diagnostic path, its reachability, and a proposed upstream fix are recorded in the
[OpenCSD issue notes](opencsd-issues.md).

## Extension points

### Add a raw trace channel

Implement a decoder that produces `TraceEvent` values, add it below `src/decode`, and select it in the control layer
for the corresponding channel. A Trace Bus implementation should replace the current warning and skip behavior while
reusing diagnostics, event selection, and output backends.

### Add an event type

Extend the model and its stable type names first. Then update the relevant decoder, selection behavior, and every
backend that can represent the event. Tests should cover semantic mapping separately from backend serialization.

### Add an output backend

Implement `TraceOutput`, define backend-specific preflight requirements, and add it to the output plan and lifecycle.
Do not introduce backend-specific state into the decode pipeline or event model.

## Test architecture

Unit tests under `test/unit/src` mirror the production modules. Shared file, event, and diagnostic helpers live under
`test/unit/support`. GoogleTest discovery registers every test case separately with CTest, so IDEs and CI can execute
individual cases.

Integration tests under `test/integration` execute the real `ctrace` binary and verify command-line behavior,
diagnostics, output cleanup, and fixture conversion. Test data and expected artifacts live under `test/data`; generated
files are written only below the CMake build directory.

The `CtraceUnitTests` target preserves one CI entry point while linking the ctrace modules explicitly. Integration
tests use the `ctrace-` CTest name prefix. CI runs both suites on Windows AMD64 and Linux AMD64. ARM64 targets are
compiled but not executed, matching the other devtools workflows.

## Build and release structure

The source tree has seven static library targets: `model`, `cli`, `trace-run`, `diagnostics`, `decode`, `output`, and
`control`. The `ctrace` executable adds only the platform trampoline and `CtraceMain`. Dependencies form a directed,
cycle-free graph with `control` as the composition root.

The tool-specific GitHub workflow is selected by a `tools/ctrace/<version>` release tag. It builds Windows AMD64 and
Arm64, Linux AMD64 and Arm64, and macOS Arm64 binaries.
The release archive contains the Apache-2.0 project license, application-dependency notices and license texts,
retained OpenCSD copyright notices, and per-file SHA-256 checksums. The version compiled into the executable is derived
from the same tag. The actual compiler and operating-system runtime content still requires inspection for each
production release.

The checked-in SWO and TB captures are approved ctrace test assets and may be redistributed with devtools. Together
with the tool-specific build, test, packaging, versioning, and license integration, this forms the technical basis for
the first open-source release.

## Architectural constraints

- Runtime dependencies point from orchestration toward decode, model, and output modules.
- Output modules consume semantic events and never OpenCSD packet types.
- YAML nodes do not cross the `tracerun` boundary.
- Diagnostics are structured and are not inferred from formatted stderr text inside the application.
- Output backends own their artifacts and must support cleanup after partial failure.
- Published test fixtures must be approved for redistribution and must not contain private or unstable trace
  payloads.
