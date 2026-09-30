# ctrace message-system design

Ctrace owns diagnostic wording in two modules in the existing `ctrace::model` library:

| Module | Responsibility |
| --- | --- |
| [TraceMessages.h](../src/model/TraceMessages.h) / [TraceMessages.cpp](../src/model/TraceMessages.cpp) | Typed decoder diagnostics with detailed CLI and compact CSV representations. |
| [DiagnosticMessages.h](../src/model/DiagnosticMessages.h) / [DiagnosticMessages.cpp](../src/model/DiagnosticMessages.cpp) | Detailed-only CLI, configuration, filesystem, output-backend and run-summary messages. |

Parameterized messages use ordinary typed C++ functions and records. Fixed operational messages use
`DiagnosticMessageCode`. The implementation requires C++17 and no additional formatting dependency. A message's
wording is independent of severity, process-failure impact, route, output filtering, and diagnostic context.

The [coverage audit](message-coverage-audit.md) records the pre-migration sources and the boundaries that motivated
this design. Both shared trace diagnostics and ctrace-owned CLI-only wording are now centralized; foreign text
continues through an explicit opaque-text path.

## Output contract

| Message family | CLI | CSV |
| --- | --- | --- |
| Decoder issue | Detailed warning or error, with existing stream/raw-offset context | Compact `note`, retaining the existing `type=error` mapping. |
| Skipped bytes | Detailed info, including cause and formatter-group offset | `type=info`: `{n} bytes skipped: {reason}`, without an offset suffix. |
| Fatal decoder abort | Detailed error, processed-byte count and cause | One final `type=error` row, count, incomplete-trace indication and compact structured cause when available. |
| Overflow | One detailed warning summary per internal route at finish | One `type=overflow` row per event with `Timestamp discontinuity`. |
| Operational/configuration/output diagnostic | Existing detailed text and context | No additional CSV row. |

CSV escaping remains in [CsvRowMapper.cpp](../src/output/csv/CsvRowMapper.cpp). Source IDs remain in the `stream`
column, including observed null/reserved IDs in byte-skip records. `CPU Sleeping` and `Trace prohibited` are
already compact PC-state notes and do not pass through the diagnostic catalog. CTF retains its structured status
codes and schema; it does not serialize rendered diagnostic messages.

Only `OpenCsdFatalError` is converted into a shared terminal abort record. Ordinary input-read exceptions and
output-backend failures keep their CLI-only catch paths. Byte-skip and terminal-abort rows bypass event filters;
normal issue/overflow events retain their selection behavior. The abort row has no invented route or timestamp.

Existing CLI wording, context, severity, failure counts and overflow aggregation are preserved. In particular,
reference-file errors retain their explicit non-failing impact. CSV warning-severity decoder issues still use
`type=error`; changing that event/schema contract is separate from text formatting.

A formatted recovery's raw interval includes formatter control bytes and interleaved streams. Its length is not
a skipped-byte count. Only `TraceByteSkip::byteCount` describes skipped payload bytes, excluding formatter control
bytes. The catalog distinguishes these cases.

## Compact trace-message catalog

Braces below document typed fields, not a proposed runtime template syntax. Counts and offsets use decimal;
payload values use hexadecimal with `0x`. Messages remain English, matching the existing output.

Keep route/source ID in the existing CSV `stream` column. Skipped-byte notes contain `{n} bytes skipped: {reason}`;
the formatter offset remains only in the detailed CLI message. For decoder issues, retain diagnostic
position in the note: append `; raw@{offset}`. A formatter-group position does not promise an exact physical
packet-byte position. For recovery spans, include the interval directly instead. When discontinuity timing is
available, append `; cycles {lastValid}..{firstResumed}`; use `?` for an unknown resumed timestamp. Do not invent
missing timestamps.

### Skipped bytes, abort, and overflow

| Condition | Detailed wording / behavior | Compact CSV note |
| --- | --- | --- |
| Missing source ID | `{n} bytes skipped due to missing source ID` | `{n} bytes skipped: no source ID` |
| Null source ID | `{n} bytes skipped for null source ID 0` | `{n} bytes skipped: null source ID` |
| Reserved source ID | `{n} bytes skipped for reserved source ID {id}` | `{n} bytes skipped: reserved source ID` |
| Unconfigured source ID | `{n} bytes skipped for unconfigured source ID {id}` | `{n} bytes skipped: unconfigured source ID` |
| Missing synchronization | `{n} bytes skipped due to missing SYNC` | `{n} bytes skipped: no SYNC` |
| Fatal decoder abort | `decode aborted after processing {n} input bytes; trace is incomplete: {reason}` | `Decode aborted after {n} bytes; trace incomplete` |
| Overflow event | `overflow: new timestamp segment; time across boundary may be unreliable` | `Timestamp discontinuity` |

Every skipped-byte message currently ends with `; first formatter group at raw offset {offset}`. Keep that exact
CLI suffix. The compact CSV text keeps the byte count and reason, without an offset suffix.
The source ID remains in `stream`, including null/reserved IDs when known.
Abort retains its full cause on the CLI. If a structured cause is available, a compact cause may follow the CSV
summary; arbitrary exception text must not be truncated or parsed to manufacture one.

Overflow's CLI summary (`first overflow occurred at ...; ... more occurred`) describes an aggregate, not an
individual event. Keep it as a separate detailed catalog entry and retain its current route-local aggregation.

### Decoder issues

The following covers every current `TraceIssueCode` and the distinct causes currently sharing a code. The
existing long text, including native OpenCSD text and bounded packet previews, remains available on the CLI.

| Existing code / condition | Compact CSV message, before position/timing suffixes |
| --- | --- |
| `DecodeError` | `Trace decode error` |
| `DataLoss`: generic discontinuity | `Data loss; timestamp discontinuity` |
| `DataLoss`: SINGLE input consumed while awaiting usable packets | `{n} raw bytes without usable ITM packets` |
| `DataLoss`: formatted recovery at hardware SYNC | `ITM resynced; raw span [{start},{end}): {n} bytes` |
| `DataLoss`: no formatted recovery before end of input | `No ITM resync before EOF; raw span [{start},{end}): {n} bytes` |
| `DataLoss`: legacy fallback with only an optional count | `ITM data loss` or `ITM data loss; {n} raw bytes affected` |
| `InvalidExceptionAction` | `Invalid action 0x0: exception {number}` |
| `UnsupportedDwtEventCounterPayload` | `Invalid DWT counter: {size} B; 0x{value}` |
| `UnsupportedPmuEventCounterPayload` | `Invalid PMU counter: {size} B; 0x{value}` |
| `UnsupportedDwtAddressPayload` | `Invalid DWT {data address / PC or match}: {size} B` |
| `UnsupportedDwtPcSamplePayload` | `Invalid PC sample: {size} B; 0x{value}` |
| `OpenCsdBadPacketSequence` | `Invalid ITM packet sequence` |
| `OpenCsdInvalidPacketHeader` | `Invalid ITM packet header` |
| `OpenCsdIncompleteTail` | `Incomplete ITM packet at EOF` |
| `OpenCsdMissingSync` | `No ITM SYNC before EOF` |
| `OpenCsdNoProgress`: generic fallback | `No decode progress` |
| `OpenCsdNoProgress`: consumed count exceeds supplied count | `Invalid consumed byte count` |
| `OpenCsdNoProgress`: partial formatter frame | `Stopped inside formatter frame` |
| `OpenCsdNoProgress`: formatted input stalled | `No formatted decode progress` |
| `OpenCsdNoProgress`: formatted retry after draining stalled | `No decode progress after formatted drain` |
| `OpenCsdNoProgress`: SINGLE retry stalled | `No decode progress after retry` |
| `OpenCsdNoProgress`: reset to search for synchronization | `No decode progress; reset for ITM SYNC` |
| `OpenCsdWaitTimeout`: bounded flush exhausted | `OpenCSD {WAIT / formatted drain} timeout: {n} flushes` |
| `OpenCsdWaitTimeout`: fallback without phase/count | `OpenCSD flush timeout` |
| `OpenCsdInitializationError` | `OpenCSD initialization failed` |
| `OpenCsdDecodeError`: formatted chunk size is not a multiple of 16 | `Invalid formatted chunk size` |
| `OpenCsdDecodeError`: formatted session-operation exception | `OpenCSD session operation failed` |
| `OpenCsdDecodeError`: route-local reset failure | `OpenCSD route reset failed` |
| `OpenCsdDecodeError`: raw monitor reserved packet | `Reserved ITM packet` |
| `OpenCsdDecodeError`: raw monitor bad sequence | `Invalid ITM packet sequence` |
| `OpenCsdDecodeError`: native not-initialized condition | `OpenCSD not initialized` |
| `OpenCsdDecodeError`: native memory error | `OpenCSD out of memory` |
| `OpenCsdDecodeError`: native/root parameter error | `Invalid OpenCSD parameter` |
| `OpenCsdDecodeError`: native input-read error | `OpenCSD input read failed` |
| `OpenCsdDecodeError`: native fatal decode error | `OpenCSD decode failed` |
| `OpenCsdDecodeError`: invalid root operation | `Invalid OpenCSD operation` |
| `OpenCsdDecodeError`: invalid root data | `Invalid OpenCSD trace data` |
| `OpenCsdDecodeError`: root system error | `OpenCSD system error` |
| `OpenCsdDecodeError`: warning root response | `OpenCSD warning` |
| `OpenCsdDecodeError`: other native/root condition | `OpenCSD error` or `OpenCSD warning`, according to severity |

When supplied, append the native OpenCSD error code or root-response code to the corresponding compact message,
for example `OpenCSD error (code {code})` or `OpenCSD warning (response {response})`. Distinguish the two code domains.
Preserve opaque native text and packet previews only in the detailed form. A known code without its typed
parameters gets a meaningful category-only fallback; never substitute invented zero values.

## Typed trace-message transport

`TraceMessage` is a value containing a `std::variant` of supported parameter records. Examples include
`TraceCounterPayload { kind, size, value }`, `TracePcSamplePayload { size, value }`,
`TraceRecovery { kind, startOffset, endOffset, byteCount }`, and `TraceFlushTimeout { phase, limit, decodeAborted }`.
These records identify the wording without unchecked positional arguments, string-keyed maps, or runtime
placeholder substitution. Both text versions are adjacent in the corresponding formatter.

The value also retains optional packet context, an optional timestamp range, and an abort phase. Counts and raw
positions remain 64-bit. Optional data stays optional: no formatter manufactures a missing offset, timestamp,
source ID, native code, or payload value.

```cpp
TraceByteSkip skipped;
skipped.byteCount = 24;
skipped.formatterOffset = 128;
skipped.reason = TraceByteSkipReason::MissingSync;

formatTraceMessage(skipped, TraceMessageStyle::Detailed);
// 24 bytes skipped due to missing SYNC; first formatter group at raw offset 128
formatTraceMessage(skipped, TraceMessageStyle::Compact);
// 24 bytes skipped: no SYNC

const auto issue = makeTraceIssue(TracePcSamplePayload{2U, 0xffU});
formatTraceIssue(issue, 128U, TraceMessageStyle::Compact);
// Invalid PC sample: 2 B; 0xff; raw@128
```

The relevant output API is:

```cpp
std::string formatTraceMessage(const TraceMessage&, TraceMessageStyle);
std::string formatTraceMessage(const TraceByteSkip&, TraceMessageStyle);
std::string formatTraceMessage(const TraceDecodeAbort&, TraceMessageStyle);
std::string formatTraceMessage(const OverflowTraceEvent&, TraceMessageStyle);
std::string formatTraceIssue(const TraceIssueEvent&, std::uint64_t rawOffset,
                             TraceMessageStyle);
std::string formatOverflowSummary(const TraceOverflowSummary&);
```

`TraceOverflowSummary` contains the optional first timestamp and overflow count. It represents the CLI aggregate,
while `OverflowTraceEvent` represents a single event. Route context remains with the reporter.

`makeTraceIssue` derives the normal semantic `TraceIssueCode` from a typed message. That code remains separate
from wording because recovery and CTF logic use it. Collector entry points preserve their explicit issue state,
severity, route, callback order and discontinuity policy while transporting the message object.

The shared model has no OpenCSD dependency. [OpenCsdErrorController.cpp](../src/decode/OpenCsdErrorController.cpp)
translates native enums into `TraceNativeCategory`, retaining native error/response codes and normalized foreign
text. `OpenCsdPacketCollector` retains typed packet kind, original size and a copy of at most 16 preview bytes;
`nullopt` distinguishes unavailable bytes from an empty packet. Native strings are not parsed back into data.

Messages travel through collector APIs and `OpenCsdTraceElement::errorMessage` into `TraceIssueEvent::message`.
`CortexMPostDecoder` completes `TraceMessage::timestampRange` when recovery timing becomes known. It does not
append prose. The CLI reporter then chooses `Detailed`; the CSV mapper chooses `Compact` and supplies the raw
position from the enclosing `TraceEvent`.

`OpenCsdTreeSessionError` retains a structured setup operation and native detail. `OpenCsdFatalError` retains the
structured cause and processed-byte count, and `FileDecodeJob` copies that cause into `TraceDecodeAbort`.
Exceptions also provide the existing detailed `what()` text for ordinary exception handlers. Abort phases retain
DATA, end-of-trace, WAIT flush, formatted drain, SINGLE reset and formatted route-reset context. Unknown exception
text remains opaque, including details captured by the formatted-session exception wrapper.

## Detailed-only operational messages

`DiagnosticMessages` centralizes CLI argument checks, trace-run discovery, YAML field validation and source
locations, processor/route binding, deferred metadata errors, path/IO failures, backend lifecycle failures and
run summaries. These messages are formatted at their existing call sites because only the CLI consumes them.
They do not become `TraceIssueEvent` values or gain artificial CSV representations.

Fixed text uses `diagnosticMessage(DiagnosticMessageCode::...)`. Parameterized families use typed functions such
as `configFieldMessage(field, ConfigFieldProblem::UnsignedRange)`,
`pathDiagnosticMessage(PathDiagnosticCode::CsvOpen, path)`, and
`decodeSummaryMessage(bytes, seconds, records)`. Owned wrappers around foreign text, such as YAML source
locations or backend failure context, are formatted here too.

Reference/pyTS messages, native YAML/cxxopts/OpenCSD text, OS errors and unexpected exception text retain their
existing passthrough behavior. `TraceOpaqueMessage` supports foreign or caller-supplied trace descriptions: the
CLI preserves them, while CSV uses the issue-code category as its compact fallback. Opaque abort causes do not
pretend to supply a structured compact explanation. Severity and failure impact stay at the existing call sites.

## Extending and validating the system

1. For a new shared trace message, add a typed parameter record and adjacent detailed/compact renderers. Add its
   semantic mapping to `makeTraceIssue` where applicable, and preserve the record through every intermediate API.
2. For a CLI-only message, add a fixed code or an appropriate typed formatter to `DiagnosticMessages`. Preserve
   the exception type, context, severity and failure impact of its call site.
3. Keep foreign text opaque. Do not infer a message kind or extract parameters by matching rendered strings.
4. Test both representations at the catalog boundary and the relevant producer/consumer path. Preserve exact CLI
   expectations; check compact CSV semantics, actual parameters and CSV escaping independently.

Catalog tests cover 64-bit values, missing data, decimal/hex rendering, native categories/codes, packet previews,
timestamp ranges, setup/abort phases and overflow summaries. Decoder, CSV, diagnostics and integration tests
cover transport, callback ordering, route attribution, event filters, recovery, and terminal-abort lifecycle.
The checked-in CSV reference changes only diagnostic notes; its remaining columns and the CTF/XML fixtures keep
the same contents.
