# ctrace Architecture

This document describes the internal structure of `ctrace`, the runtime data flow, and the intended extension points.
For command-line usage and build instructions, see the [project README](../README.md). The [verified
constraints](constraints.md) record preserved contracts; the compact [TODO list](todo.md) tracks remaining work.

![ctrace architecture](architecture.svg)

## Scope

`ctrace` combines a trace-run configuration with raw CoreSight trace data and converts supported trace channels into
backend-independent semantic events. Output backends consume these events to create CSV or CTF artifacts.

The first release profile supports SWO data containing ITM and DWT packets. The command line accepts the stable type
names `itm`, `dwt`, `event`, `pmu`, `exception`, `pcsample`, `global_ts`, `overflow`, and `error`. Output semantics are
currently implemented for every listed type. Valid DWT event-counter and PMU trace-on-overflow packets reach CSV as
one row containing the hardware mask. The CTF backend expands each mask into one timestamped record per set bit so
Trace Compass can show exact table rows and labeled one-microsecond visualization pulses. DWT records use their fixed
architectural counter names; PMU records provisionally use `Event0` through `Event7` until trace-run configuration can
resolve the programmable counter assignments. Periodic PC samples reach CSV and CTF as semantic events; the CTF event
distinguishes a sampled PC from a processor-sleep indication, and Trace Compass shows processor-sleep intervals as a
timeline. Trace Bus input is discovered so that a complete trace directory can be inspected, but `*.TB.raw` files are
reported and skipped until a decoder is implemented.

The architecture separates protocol decoding, semantic interpretation, and output generation. This keeps output
formats independent of OpenCSD and allows another raw trace channel to reuse the event model and output backends.

## How it works at a glance

`ctrace` processes one solution set at a time. The [README](../README.md#trace-directory) describes how configuration,
raw input, and generated output files are grouped by their common base name.

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

## Processing state and ownership

One `DecodePipeline` is created for each supported raw file. It owns the OpenCSD adapter and Cortex-M stream decoder,
so protocol, timestamp, and DWT state survive arbitrary file-read boundaries. `RawFileReader` owns a single 64 KiB
buffer; each `RawByteView` borrows that buffer only for the synchronous `DecodePipeline::push` call. Calling
`DecodePipeline::finish` flushes both the OpenCSD and Cortex-M layers before the pipeline is destroyed.

`CortexMStreamDecoder` maintains an independent post-decoder for each observed Trace Bus ID. All post-decoders emit
into the same `TraceEventSink`, preserving input order while keeping stream-specific timestamp and DWT state apart.

There is no application-wide event queue. `DecodeConsumers` forwards each event synchronously to the output
lifecycle and issue reporter.

## Recovery after damaged trace

Recoverable OpenCSD packet errors establish a discontinuity at the reported raw-file offset. Decoder callbacks before
that offset are retained; callbacks at or after it belong to the failed transaction and are discarded. `ctrace` then
resets the OpenCSD decoder and feeds the following input bytes to it unchanged. OpenCSD resumes only after finding a
real ITM hardware-sync sequence; `ctrace` never inserts a synthetic sync sequence.

Bytes consumed while no usable trace elements are produced form one explicit `data-loss` interval. At its boundary,
the Cortex-M post-decoder flushes pending events, resets incomplete DWT correlation, and marks timestamps unreliable
until the stream supplies enough timing information again. The issue remains part of the ordered `TraceEvent` stream,
so diagnostics and enabled output backends observe the same recovery boundary.

Failure to reset OpenCSD, repeated lack of decoder progress, or an unsuccessful wait/flush operation aborts the
current raw-file job.

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

The YAML reader's validation and metadata rules are recorded in the [constraints](constraints.md#boundaries).

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

Diagnostics carry a severity, message, context, and impact. Severity describes the issue, while impact
determines whether the current job must fail. This distinction allows a trace-run generation error to remain visible
without necessarily preventing the decoding of otherwise valid trace input.

Decoder issue packets remain part of the event stream. `DecodeConsumers` reports every issue to stderr independently
of output filters and forwards all events to the backends. The backends apply stream and type selection internally;
selected issues become CSV error rows or CTF trace-status events. Repeated issues are not silently collapsed.

An invocation-wide diagnostic sink aggregates failures while other solution sets continue where possible, then
determines the final process status. Errors are rendered as `error` even when their impact causes a non-zero exit
status. Unhandled internal ctrace failures also terminate the command after an error diagnostic.

## External dependencies

`cxxopts` provides command-line parsing, `yaml-cpp` is confined to the trace-run reader, OpenCSD is isolated behind
the decode adapters, and GoogleTest is used only by test targets. Exact revisions, licenses, and dependency build
configuration are documented in the [third-party notices](THIRD_PARTY_NOTICES.md).

The ITM adapter currently uses OpenCSD `common/` and `interfaces/` headers because the public OpenCSD 1.8.3 boundary
does not provide equivalent access: its installed headers omit the ITM configuration and packet types required by the
decoder callbacks, and its C API exposes only the last structured error rather than all errors from one data-path
operation. Moving the adapter to the public API therefore also requires resolving these two gaps.

A known decoder defect and its proposed upstream fix are recorded in the [OpenCSD issue notes](opencsd-issues.md).

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
`test/unit/support`. One CTest entry runs the complete GoogleTest executable and writes its XML report.

Executable-level coverage and fixture ownership are documented next to the
[integration tests](../test/integration/README.md) and [test data](../test/data/README.md).

## Build and CI structure

The source tree has seven static library targets: `model`, `cli`, `trace-run`, `diagnostics`, `decode`, `output`, and
`control`. The shared `ctracelib` object contains `CtraceMain`; the executable adds only the platform trampoline and
manifest where required. Dependencies form a directed, cycle-free graph with `control` as the composition root.

The tool-specific GitHub workflow is selected by a `tools/ctrace/<version>` release tag. It builds Windows AMD64 and
Arm64, Linux AMD64 and Arm64, and macOS Arm64 binaries. Unit and integration tests run on Windows AMD64 and Linux
AMD64; the remaining targets are compile-only.
The version compiled into the executable is derived from the same tag. Archive contents and license material are
described in the [third-party notices](THIRD_PARTY_NOTICES.md); unfinished release work remains in the
[TODO list](todo.md).
