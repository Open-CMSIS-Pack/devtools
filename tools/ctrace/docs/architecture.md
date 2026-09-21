# ctrace Architecture

This document describes the internal structure of `ctrace`, the runtime data flow, and the intended extension points.
For command-line usage and build instructions, see the [project README](../README.md). The [verified
constraints](constraints.md) record preserved contracts; the compact [TODO list](todo.md) tracks remaining work.

![ctrace architecture](architecture.svg)

## Scope

`ctrace` combines a trace-run configuration with raw CoreSight trace data and converts supported trace channels into
backend-independent semantic events. Output backends consume these events to create CSV or CTF artifacts.

The current profile supports unformatted ITM byte streams from SWO or explicitly declared TB input, and
memory-aligned formatted CoreSight input carrying ITM and DWT packets. Each configured ITM route supports the public
event selections `itm`, `dwt`, `event`, `pmu`, `exception`, `pcsample`, `global_ts`, `overflow`, and `error`.
Backend-specific representations are documented in the [CTF profile](ctf-format.md), not in the decoder contract.

Exactly one raw input is active for each trace-run configuration. Formatted input distributes bytes to configured
ITM routes by Trace Bus ID; unformatted input uses one synthetic route. Other protocols require explicit decoder
integration, not guesses based on observed IDs. Deferred inputs and decoders are tracked in the [TODO list](todo.md).

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

Formatter-skipped payload and initial ITM synchronization skips follow a separate `TraceByteSkip` path from the
OpenCSD adapter through `DecodePipeline` directly to `TraceEventSink::appendByteSkip` and `DecodeConsumers`. These
input annotations bypass route-local Cortex-M interpretation: they carry an observed ID, if known, but no decoded
event or timestamp. Consumers retain CSV `info` rows and report CLI Info; CTF does not encode these annotations.

The similarly named decode types are consecutive pipeline stages, not interchangeable implementations of one decoder
contract. `OpenCsdItmDecoder` decodes transport bytes, `CortexMStreamDecoder` routes normalized elements, and each
`CortexMPostDecoder` reconstructs semantic events with help from `DwtPacketDecoder`. Common interfaces exist only at
actual substitution boundaries such as `OpenCsdItmSessionInterface`, `OpenCsdTraceElementSink`, and `TraceEventSink`;
there is deliberately no common base class for all decode stages.

## Input and compatibility contract

Input selection and route binding belong to `tracerun` and complete before decoder or output construction.
The provisional, ctrace-private `trace-format` declaration describes effective capture bytes rather than target
capability or file identity. Discovery first selects exactly one SWO, TB, or named-TB input, independently of whether
the format is declared. An explicit format wins; otherwise SWO defaults to `unformatted` and TB or named-TB to
`formatted`. The selected format is resolved before `CtraceRunMeta` normalizes routes, so the decoder and route model
use the same effective format. This channel-based heuristic does not inspect capture bytes or resolve ambiguous input
selection. Framing remains an internal decoder contract.

The [input constraints](constraints.md#input-format-framing-and-discovery) define the accepted declarations, defaults,
file candidates, and framing limits. The [routing invariants](constraints.md#routing-invariants) define reference
binding, valid IDs, and the synthetic unformatted route; backends preserve its legacy output representation.

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
owns the tree and configured decoder components. For formatted input, `OpenCsdFormattedItmSession` additionally owns
the route adapters, packet/frame monitors, and captured callback errors installed in that tree. For SINGLE input,
the decoder implementation owns the packet collector and error controller that `OpenCsdItmSession` connects
directly. Members are ordered so the tree is destroyed first, before any callback target or diagnostic state it can
reference. This keeps format-specific feed and recovery policy out of the low-level tree wrapper without weakening
callback lifetime safety.

`CortexMStreamDecoder` maintains an independent post-decoder for each normalized route. All post-decoders emit into
the same `TraceEventSink`, preserving input order while keeping route-specific timestamp and DWT state apart.

There is no application-wide event queue. `DecodeConsumers` forwards each event synchronously to the output
lifecycle and issue reporter.

## Recovery after damaged trace

For formatted input, the frame monitor counts skipped payload without a known source ID and payload assigned to
NULL, reserved, or unconfigured IDs. Bounded accounting produces input annotations with the reason, observed ID
when known, byte count, and first formatter output group's raw offset. Counts are deformatted payload bytes,
excluding formatter control bytes; the offset is not the position of each discarded byte. The unassigned prefix
is reported after an assigned group appears or at end of input; skipped-ID totals are reported at end of input.
These Info records do not make the input fatal. No capture bytes are removed or rewritten by ctrace.

An already identified formatted route can also discard initial ITM bytes while seeking its first hardware sync.
The packet collector counts the deformatted bytes reported as `NOTSYNC`, rather than subtracting raw offsets across
interleaved formatter groups. If the route later reaches a committed real sync, it emits one `MissingSync` byte-skip
Info immediately before that sync, not a semantic warning or a manufactured sync. An existing route error/recovery
interval suppresses duplicate initial-loss accounting. The input annotation retains the observed ID and bypasses
output filters. This path covers initial formatted synchronization, not general SWO or malformed-packet recovery.

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

Initial ITM synchronization is tracked independently per formatted route. At end of input, a configured route that
received payload but has no committed real hardware sync emits a route-bound `OpenCsdMissingSync` Error. A sync from
a rolled-back transaction does not satisfy this check. The error retains healthy route output and completed
diagnostic artifacts but makes the invocation fail. Routes with no received payload are not diagnosed as missing sync.

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

`TraceByteSkip` is deliberately separate from `TraceEvent`: it carries a formatter-group offset, skipped payload-byte
count, reason, and optional observed ID. An observed ID may be NULL or reserved; it must not be turned into a decoded
route or assigned an invented clock for output convenience.

### Output

| Module | Responsibility |
| --- | --- |
| `src/output` | Backend requirements, output planning, and lifecycle management |
| `src/output/csv` | Stable CSV schema, row mapping, filtering, and file output |
| `src/output/ctf` | CTF metadata and stream encoding plus Trace Compass analysis XML |

Output requirements are evaluated per backend and selected route. For example, missing CTF-specific metadata on an
active route may disable CTF while an independent CSV output remains valid; metadata on a route excluded by the
stream filter is not required. `--all` therefore does not make the backends share failure state unnecessarily.

CSV writes one combined file in decode callback order. `CtfBundleOutput` owns a bundle-local metadata model and
lazily creates a stream writer for each formatted route that emits selected events. Representation changes stay in
the backends: for example, CSV retains a DWT/PMU counter mask in one row while CTF expands it into individual records.

CTF finalization retains only emitted streams, then generates Trace Compass XML from their observed graphical topics.
This avoids empty views and invented durations for point events. Route identity stays separate from display labels,
so equal processor names cannot merge views. Formatted routes retain distinct clock domains because the input contract
does not establish cross-route synchronization. Multi-clock data remains valid CTF but cannot safely drive the supported
reader's combined XML analysis.

The [CTF profile](ctf-format.md) defines event schemas, metadata, clock mappings, legacy layouts, and
[XML projection](ctf-format.md#generated-trace-compass-analysis). Cross-backend compatibility and failure rules belong
to the [output constraints](constraints.md#observable-behavior-and-output-safety).

Outputs use an explicit `start`, `writeEvent` / `writeByteSkip`, `stop`, and `abort` lifecycle. `TraceOutput`
owns the active state and common write-failure cleanup; concrete backends implement the protected lifecycle hooks.
The byte-skip hook defaults to no output, as required by CTF; CSV implements it independently of event filters.
A successful backend can finish even if another backend fails. Fatal decode or finalization failures trigger cleanup
of incomplete artifacts.

## Diagnostics and failure semantics

Diagnostics carry a severity, message, context, and impact. Severity describes the issue, while impact
determines whether the current job must fail. This distinction allows a trace-run generation error to remain visible
without necessarily preventing the decoding of otherwise valid trace input.

Decoder issue packets remain part of the event stream. `DecodeConsumers` reports every issue to stderr independently
of output filters and forwards all events to the backends. The backends apply stream and type selection internally;
selected issues become CSV error rows or CTF trace-status events. Repeated issues are not silently collapsed.

Byte-skip annotations are non-failing Info, not synchronization events. CSV uses `type=info`, a descriptive note,
the observed formatter ID in `stream` when known, and empty `cycles`, `source`, `value`, `pc`, and `address` fields.
These rows bypass both type and stream filters; `info` is not a new selectable event type. CTF ignores them instead
of creating routes or clocks, and CLI Info remains visible in CTF-only mode. The existing once-per-ID unsupported
source warning remains separate from byte accounting. A route-bound missing-sync Error follows ordinary output
selection but always contributes to command failure; its text does not repeat the byte count already reported as Info.

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
for the corresponding channel. A new protocol carried by formatted CoreSight input additionally needs an explicit
protocol route from configuration; an observed unknown formatter ID is deliberately not enough to choose a decoder.
Reuse the input descriptor, diagnostics, event selection, and output backends rather than creating a second raw-input
architecture.

### Add an event type

Extend the model and its stable type names first. Then update the relevant decoder, selection behavior, and every
backend that can represent the event. Tests should cover semantic mapping separately from backend serialization.

### Add an output backend

Derive from `TraceOutput`, implement its backend hooks, define backend-specific preflight requirements, and add it to
the output plan and lifecycle. The concrete destructor must call `abortNoexcept()` while its backend members are still
alive. Do not introduce backend-specific state into the decode pipeline or event model.

## Test architecture

Unit tests under `test/unit/src` mirror the production modules. Shared file, event, and diagnostic helpers live under
`test/unit/support`. One CTest entry runs the complete GoogleTest executable and writes its XML report.

Executable-level coverage and fixture ownership are documented next to the
[integration tests](../test/integration/README.md) and [test data](../test/data/README.md).

## Build and CI structure

The source tree has seven static library targets: `ctrace-model`, `ctrace-cli`, `ctrace-trace-run`,
`ctrace-diagnostics`, `ctrace-decode`, `ctrace-output`, and `ctrace-control`. Their `ctrace::` aliases expose the
shorter module names inside CMake. The shared `ctracelib` object contains `CtraceMain`; the executable adds only the
platform trampoline and manifest where required. Dependencies form a directed, cycle-free graph with `control` as
the composition root.

The tool-specific GitHub workflow is triggered for matching pull requests and pushes to `main` and reacts to
published releases. Only its release job is selected by a `tools/ctrace/<version>` release tag. The build matrix
covers Windows AMD64 and Arm64, Linux AMD64 and Arm64, and macOS Arm64 binaries. Unit and
integration tests run on Windows AMD64, Linux AMD64, and macOS Arm64; Windows Arm64 and Linux Arm64 remain
compile-only. Native Linux additionally runs the exact Babeltrace 2.0.5 consumer gate on AMD64. CI enforces 100%
source-line coverage for `tools/ctrace/src`; branch coverage is retained for review but is not the merge gate. The
versioned manual Trace Compass Server/TSP acceptance record is documented beside the
[integration tests](../test/integration/README.md).
The release version compiled into the executable is derived from the same tag. Archive contents and license material
are described in the [third-party notices](THIRD_PARTY_NOTICES.md); unfinished release work remains in the
[TODO list](todo.md).
