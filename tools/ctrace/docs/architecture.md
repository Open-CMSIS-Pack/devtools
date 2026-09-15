# ctrace Architecture

This document describes the internal structure of `ctrace`, the runtime data flow, and the intended extension points.
For command-line usage and build instructions, see the [project README](../README.md). The [verified
constraints](constraints.md) record preserved contracts; the compact [TODO list](todo.md) tracks remaining work.

![ctrace architecture](architecture.svg)

## Scope

`ctrace` combines a trace-run configuration with raw CoreSight trace data and converts supported trace channels into
backend-independent semantic events. Output backends consume these events to create CSV or CTF artifacts.

The first release profile supports unformatted SWO and memory-aligned formatted CoreSight input carrying ITM and DWT
packets. The command line accepts the stable type names `itm`, `dwt`, `event`, `pmu`, `exception`, `pcsample`,
`global_ts`, `overflow`, and `error`. Output semantics are implemented for every listed type on every configured ITM
route. Valid DWT event-counter and PMU trace-on-overflow packets reach CSV as one row containing the hardware mask.
The CTF backend expands each mask into one timestamped record per set bit so Trace Compass can show exact event-table
rows and labeled one-microsecond visualization pulses. DWT records use their fixed architectural counter names; PMU
records provisionally use `Event0` through `Event7` until trace-run configuration can resolve the programmable
counter assignments. Periodic PC samples reach CSV and CTF as semantic events; the CTF event distinguishes a sampled
PC from a processor-sleep indication, and Trace Compass shows processor-sleep intervals as a `Processor State`
timeline. Sampled PCs, ITM payloads, and trace-status records remain in the generic event table instead of being
misrepresented as states with duration.

Exactly one raw input is active for each trace-run configuration. Legacy configurations select `*.SWO.raw` as an
unformatted stream. An explicit provisional `trace-format` declaration can select one SWO, TB, or named-TB file as
unformatted or formatted input. Formatted input is routed by Trace Bus ID; normal IDs without a configured ITM route
are diagnosed once and skipped without guessing whether they carry instruction trace or another protocol.

The architecture separates protocol decoding, semantic interpretation, and output generation. This keeps output
formats independent of OpenCSD and allows another raw trace channel to reuse the event model and output backends.

## How it works at a glance

`ctrace` processes one solution set at a time. The [README](../README.md#trace-directory) describes how configuration,
raw input, and generated output files are grouped by their common base name.

The main in-memory path is:

```text
command line + trace-run YAML + selected SWO/TB raw file
                    |
                    v
        TraceDirectoryJob / FileDecodeJob
       input descriptor, routes, output plan
                    |
                    v
           64 KiB non-owning byte views
                    |
                    v
          OpenCSD DecodeTree session
       SINGLE or FRAME_FORMATTED root
          route-bound ITM decoders
                    |
                    v
       routed OpenCsdTraceElement values
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

The similarly named decode types are consecutive pipeline stages, not interchangeable implementations of one decoder
contract. `OpenCsdItmDecoder` decodes transport bytes, `CortexMStreamDecoder` routes normalized elements, and each
`CortexMPostDecoder` reconstructs semantic events with help from `DwtPacketDecoder`. Common interfaces exist only at
actual substitution boundaries such as `OpenCsdItmSessionInterface`, `OpenCsdTraceElementSink`, and `TraceEventSink`;
there is deliberately no common base class for all decode stages.

## Input and compatibility contract

Root-level `trace-format` is a ctrace-private working field, not yet part of the CMSIS-Toolbox specification. Missing
or null selects the legacy `unformatted` default and SWO-only discovery; an explicit `unformatted` or `formatted`
value enables selection of exactly one eligible SWO/TB file. The value describes the selected file's effective bytes,
not the target's formatter capability. Its specification and producer integration remain separate follow-up work.

Formatted input currently means complete, 16-byte memory-aligned CoreSight frames. Memory alignment is one internal
decoder contract, not a stored or user-selectable value; there is no public `trace-framing` YAML field, and FSYNC/HSYNC
modes remain deferred. Generated `ctrace-refs.stream` values in the architectural range `1..111` bind formatter IDs
to ITM routes. The legacy unformatted path remains one synthetic route with no architectural ID and preserves its
existing CSV and CTF shape.

## Processing state and ownership

One `DecodePipeline` is created for the selected raw file. It owns the OpenCSD adapter and Cortex-M stream decoder, so
protocol, formatter, timestamp, and DWT state survive arbitrary file-read boundaries. `RawFileReader` owns a single
64 KiB buffer; each `RawByteView` borrows that buffer only for the synchronous `DecodePipeline::push` call. Calling
`DecodePipeline::finish` flushes both the OpenCSD and Cortex-M layers before the pipeline is destroyed.

Both input formats use the same `OpenCSD DecodeTree` ownership boundary. `SINGLE` connects one synthetic, no-ATB-ID
route to one ITM decoder. `FRAME_FORMATTED` owns the frame deformatter and one route-bound ITM decoder for each
configured Trace Bus ID. Ctrace creates and feeds one tree at a time because OpenCSD's alternate logger and live-tree
registry use process-global state; the session restores the previously installed logger when it is destroyed.

Ownership is deliberately split by responsibility while preserving one enclosing lifetime: `OpenCsdTreeSession`
owns the tree and configured decoder components; the format-specific session owns callback adapters, monitors, and
captured callback errors; and the decoder implementation owns the event collector and error controller. Members are
ordered so the tree is destroyed first, before any callback target or diagnostic state it can reference. This keeps
format-specific feed and recovery policy out of the low-level tree wrapper without weakening callback lifetime
safety.

`CortexMStreamDecoder` maintains an independent post-decoder for each normalized route. All post-decoders emit into
the same `TraceEventSink`, preserving input order while keeping route-specific timestamp and DWT state apart.

There is no application-wide event queue. `DecodeConsumers` forwards each event synchronously to the output
lifecycle and issue reporter.

## Recovery after damaged trace

Recoverable OpenCSD packet errors establish a discontinuity on the affected route at the reported raw-file offset.
Callbacks before that offset are retained. Route-aware transaction buffering discards callbacks from the failing
route at or after it while retaining callbacks from other routes in their original order. Error callbacks only
capture a stable batch; rollback and reset decisions happen after the synchronous OpenCSD operation returns.

For a known ITM route, `ctrace` resets only that route's packet-processor/full-decoder chain. The formatted
deformatter, its current source ID, partially delivered frame, and every other route remain intact. A bounded root
flush drains already unpacked frame segments before the next aligned input block. The raw cursor advances only by the
byte count returned by the DecodeTree, so consumed bytes are never fed twice. OpenCSD resumes the affected route only
after finding a real ITM hardware-sync sequence; `ctrace` never inserts a synthetic sync sequence.

Bytes lost before resynchronization form one explicit route-bound `data-loss` interval. At its boundary, that route's
Cortex-M post-decoder flushes pending events, resets incomplete DWT correlation, and marks timestamps unreliable until
the stream supplies enough timing information again. A route that does not resynchronize closes its interval as
unresolved at end of input. The issue remains part of the ordered `TraceEvent` stream, so diagnostics and enabled
output backends observe the same recovery boundary.

An error without a usable route, a deformatter error, failure to reset the route, repeated lack of decoder progress,
or an unsuccessful bounded wait/flush operation aborts the current raw-file job and every active output.

## Suggested code-reading path

1. Start at [`CtraceMain.cpp`](../src/CtraceMain.cpp) for command-line handling and top-level error policy.
2. Follow [`TraceDirectoryJob.cpp`](../src/control/TraceDirectoryJob.cpp) and
   [`TraceRunDiscovery.cpp`](../src/tracerun/TraceRunDiscovery.cpp) to see how a solution set, YAML, and exactly one
   SWO/TB input become a preflighted descriptor.
3. Read [`FileDecodeJob.cpp`](../src/control/FileDecodeJob.cpp) for output preflight, chunked input, pipeline
   construction, and finalization.
4. Continue through [`DecodePipeline.cpp`](../src/decode/DecodePipeline.cpp),
   [`OpenCsdItmDecoder.cpp`](../src/decode/OpenCsdItmDecoder.cpp),
   [`OpenCsdTreeSession.cpp`](../src/decode/OpenCsdTreeSession.cpp), and
   [`CortexMStreamDecoder.cpp`](../src/decode/CortexMStreamDecoder.cpp) for the tree, protocol, and semantic decode
   representations.
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

The YAML reader's validation, provisional input-format, and metadata rules are recorded in the
[constraints](constraints.md).

`CtraceRunMeta` is the boundary between the YAML representation and runtime processing. `TraceRunInputDescriptor`
combines it with the selected open raw file and effective format. Decode and
output modules consume these normalized values instead of navigating YAML nodes or repeating discovery decisions.

### Decode and event model

| Module | Responsibility |
| --- | --- |
| `src/decode` | Raw-byte decoder interface, OpenCSD integration, packet recovery, timestamps, and Cortex-M semantics |
| `src/model` | Backend-independent event types, quality information, and event selection |

OpenCSD is isolated behind adapter classes in `src/decode`. OpenCSD-specific elements do not escape into the output
modules. The post-decoder maps them to `TraceEvent` variants such as software trace, DWT data, exceptions, timestamps,
overflow, and trace issues. `TraceSelection` owns the stable public type names and maps semantic events onto that
release-facing set.

Each event retains a normalized route identity and can retain its raw index, architectural Trace Bus ID, timestamp,
and quality state. The private route ID distinguishes even routes without an ATB ID; it is never exposed as a public
stream number. This allows diagnostics and backends to make independent decisions without reconstructing decoder
state.

### Output

| Module | Responsibility |
| --- | --- |
| `src/output` | Backend requirements, output planning, and lifecycle management |
| `src/output/csv` | Stable CSV schema, row mapping, filtering, and file output |
| `src/output/ctf` | CTF metadata and stream encoding plus Trace Compass analysis XML |

The generated event IDs, fields, enum values, quality markers, and visualization semantics are specified in the
[ctrace CTF profile](ctf-format.md).

Output requirements are evaluated per backend and selected route. For example, missing CTF-specific metadata on an
active route may disable CTF while an independent CSV output remains valid; metadata on a route excluded by the
stream filter is not required. `--all` therefore does not make the backends share failure state unnecessarily.

CSV remains one combined file in synchronous semantic callback order; formatted rows carry their architectural Trace
Bus ID and the legacy unformatted stream column stays empty. CTF owns one bundle-local metadata model and lazily
creates one `stream_<id>` writer per formatted route that emits a selected event. Each stream class references an
explicit clock domain. Generalized metadata stores the optional processor name in a stream-scoped environment entry
and exposes the same display identity through a private `ctrace_route` enum. Generated Trace Compass XML creates a
graphical provider only when the completed stream contains trace data for that topic. Synthetic exception bootstrap
records alone do not create an exception view. A `Processor State` provider requires an actual sleep indication;
ordinary PC samples do not create it. Visible provider names append the resolved processor name but never a numeric
ID; without a resolved name they retain the topic name alone. The provider ID and its state query select the
architectural `cmsis_trace_bus_id`, so equal display names cannot merge routes. Internally, an unbound `ctrace_route`
enum label falls back to its decimal CTF stream-class ID to keep the state path unique. The existing
`uint8_t cmsis_trace_bus_id` field remains unchanged for CMSIS-profile consumers. Distinct processor bindings remain
distinct domains even when their clock frequencies match.
Because the supported Trace Compass reader cannot reliably combine multiple clock declarations, ctrace keeps that
valid CTF bundle but omits any stale/new companion XML and reports one Warning. When selected, the legacy unformatted
CTF path keeps its eager `stream_0`, `swo_clock`, original event context, and single-clock XML behavior.

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
the decode adapters, and GoogleTest is used only by test targets. Babeltrace and Trace Compass are acceptance
consumers, not linked runtime dependencies. Exact revisions, licenses, and build configuration for the packaged
dependencies are documented in the [third-party notices](THIRD_PARTY_NOTICES.md).

The ITM adapter currently uses OpenCSD `common/` and `interfaces/` headers because the public OpenCSD 1.8.3 boundary
does not provide equivalent access: its installed headers omit the ITM configuration and packet types required by the
decoder callbacks, and its C API exposes only the last structured error rather than all errors from one data-path
operation. Moving the adapter to the public API therefore also requires resolving these two gaps.

A known decoder defect and its proposed upstream fix are recorded in the [OpenCSD issue notes](opencsd-issues.md).

## Extension points

### Add a raw trace channel

Implement a decoder that produces `TraceEvent` values, add it below `src/decode`, and select it in the control layer
for the corresponding channel. A new protocol carried by formatted Trace Bus input additionally needs an explicit
protocol route from configuration; an observed unknown formatter ID is deliberately not enough to choose a decoder.
Reuse the input descriptor, diagnostics, event selection, and output backends rather than creating a second raw-input
architecture.

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

The tool-specific GitHub workflow runs for matching pull requests and pushes to `main`, can be called by another
workflow, and reacts to published releases. Only its release job is selected by a `tools/ctrace/<version>` release
tag. The build matrix covers Windows AMD64 and Arm64, Linux AMD64 and Arm64, and macOS Arm64 binaries. Unit and
integration tests run on Windows AMD64, Linux AMD64, and macOS Arm64; Windows Arm64 and Linux Arm64 remain
compile-only. Native Linux additionally runs the exact Babeltrace 2.0.5 consumer gate on AMD64. The versioned manual
Trace Compass Server/TSP acceptance record is documented beside the
[integration tests](../test/integration/README.md).
The release version compiled into the executable is derived from the same tag. Archive contents and license material
are described in the [third-party notices](THIRD_PARTY_NOTICES.md); unfinished release work remains in the
[TODO list](todo.md).
