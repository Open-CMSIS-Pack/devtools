# ctrace message-system design

All ctrace-owned diagnostic templates live in one [message catalog][catalog] in `ctrace::model`. Each entry has a
`MessageId`, detailed CLI text, and compact CSV text. The enum is generated from the same table, so there is no
separate list of IDs to keep synchronized. Foreign descriptions remain opaque values supplied to owned templates.

| Component | Responsibility |
| --- | --- |
| [MessageCatalog.inc][catalog] | IDs and adjacent detailed/compact templates. |
| [Messages.h][messages-header] / [Messages.cpp][messages-source] | Argument values, template validation and rendering. |
| [TraceMessages][trace-header] | Typed trace records and adapters that select catalog IDs and arguments. |
| [DiagnosticMessages][diagnostic-header] | Typed adapters for operational message families. |

The adapters retain decisions about protocol categories, optional fields, hexadecimal values, and numeric precision.
They do not own a second table of diagnostic wording. Fixed CLI-only messages call `formatMessage(MessageId::...)`
directly. Parameterized CLI-only messages use an appropriate typed adapter or pass explicit catalog arguments.
The implementation requires C++17 and no external formatting dependency.

## IDs, templates and arguments

The central API is:

```cpp
std::string formatMessage(
    MessageId id,
    MessageStyle style = MessageStyle::Detailed,
    std::initializer_list<MessageArgument> arguments = {});
```

A catalog row uses `CTRACE_MESSAGE(Id, detailed, compact)`. Templates accept positional placeholders such as `{0}`
and `{1}`. Braces in templates must form positional placeholders. Argument values are copied as text and are never
interpreted as nested templates. There are no named placeholders or format specifiers.

```cpp
CTRACE_MESSAGE(TraceSkipMissingSync,
               "{0} bytes skipped due to missing SYNC; first formatter group at raw offset {1}",
               "{0} bytes skipped: no SYNC")

formatMessage(MessageId::TraceSkipMissingSync, MessageStyle::Compact, {24U, 128U});
// 24 bytes skipped: no SYNC
```

`MessageArgument` owns its value. It accepts strings, string views, non-null C strings, and integral values rendered
in decimal. Typed adapters format hexadecimal payloads and floating-point precision before passing those values.
Counts and offsets remain 64-bit throughout trace transport; formatting does not narrow them.

Detailed and compact templates share one argument list, but either template may omit positions it does not display.
For example, the detailed skipped-byte message can use a count, source ID and formatter offset, while its compact
version uses only the count. Both styles receive all three arguments. The table is checked at compile time for valid
placeholder syntax and a contiguous union of argument positions starting at zero. At runtime, an invalid ID, invalid
style or wrong argument count is rejected rather than producing partial text.

A `nullptr` compact template reuses the detailed template. This is the normal declaration for CLI-only messages;
it does not cause a new CSV record to be emitted. An empty string is an intentionally empty template, not a request
for fallback. Message IDs are internal identifiers; they are not serialized as new CSV fields or CTF attributes.

## Typed trace transport

`TraceMessage` contains a `std::variant` of supported parameter records, including `TraceCounterPayload`,
`TracePcSamplePayload`, `TraceRecovery`, and `TraceFlushTimeout`. Producers choose a meaningful record instead of
building prose or passing an untyped list through the decoder. The output adapter converts that record into the
appropriate `MessageId` and arguments only when the output style is known.

The value also retains optional packet context, an optional timestamp range, and an abort phase. Missing data stays
optional: no formatter invents an offset, timestamp, source ID, native code, or payload value. The same byte-skip
record can therefore produce both representations:

```cpp
TraceByteSkip skipped;
skipped.byteCount = 24;
skipped.formatterOffset = 128;
skipped.reason = TraceByteSkipReason::MissingSync;

formatTraceMessage(skipped, MessageStyle::Detailed);
// 24 bytes skipped due to missing SYNC; first formatter group at raw offset 128
formatTraceMessage(skipped, MessageStyle::Compact);
// 24 bytes skipped: no SYNC
```

`makeTraceIssue` derives the normal semantic `TraceIssueCode` from a typed message. This code remains separate from
`MessageId`: recovery and CTF logic use the semantic issue code, while the catalog ID selects wording. Collector APIs
preserve severity, route, callback order and discontinuity policy while transporting the message object.

Messages travel through collector APIs and `OpenCsdTraceElement::errorMessage` into `TraceIssueEvent::message`.
`CortexMPostDecoder` completes `TraceMessage::timestampRange` when recovery timing becomes known; it does not append
prose. The CLI reporter chooses `MessageStyle::Detailed`; the CSV mapper chooses `MessageStyle::Compact` and supplies
the raw position from the enclosing `TraceEvent`.

The shared model has no OpenCSD dependency. [OpenCsdErrorController][error-controller] translates native enums into
`TraceNativeCategory`, retaining native error/response codes and normalized foreign text. `OpenCsdPacketCollector`
retains typed packet kind, original size and a copy of at most 16 preview bytes. `nullopt` distinguishes unavailable
bytes from an empty packet. Native strings are never parsed back into parameters.

`OpenCsdTreeSessionError` retains its structured setup operation and native detail. `OpenCsdFatalError` retains the
structured cause and processed-byte count, and `FileDecodeJob` copies the cause into `TraceDecodeAbort`. Exceptions
also expose the existing detailed `what()` text for ordinary exception handlers. Abort phases preserve DATA,
end-of-trace, WAIT flush, formatted drain, SINGLE reset and formatted route-reset context.

## Output contract

| Message family | CLI | CSV |
| --- | --- | --- |
| Decoder issue | Detailed warning or error. | Compact note in the existing `error` row. |
| Skipped bytes | Detailed Info with formatter offset. | `info` row with count and cause. |
| Fatal decoder abort | Detailed error with processed count and cause. | Final global `error` row. |
| Overflow | One warning summary per internal route. | One `overflow` row per event. |
| Operational diagnostic | Detailed text and context. | No additional row. |

Wording is independent of severity, process-failure impact, route, filtering and diagnostic context. Existing CLI
text, failure counts and overflow aggregation remain unchanged. Reference-file errors retain their explicit
non-failing impact. CSV warning-severity decoder issues still use `type=error`; there is no new severity column.

[CsvRowMapper][csv-mapper] continues to escape CSV fields. Source IDs stay in `stream`, including observed null and
reserved IDs in byte-skip records. `CPU Sleeping` and `Trace prohibited` remain compact PC-state notes outside the
diagnostic catalog. CTF keeps its structured status codes and schema; it does not serialize rendered messages.

### Skipped bytes

Compact skipped-byte notes retain the count and cause:

| Cause | Compact note |
| --- | --- |
| Missing source ID | `N bytes skipped: no source ID` |
| Null source ID | `N bytes skipped: null source ID` |
| Reserved source ID | `N bytes skipped: reserved source ID` |
| Unconfigured source ID | `N bytes skipped: unconfigured source ID` |
| Missing synchronization | `N bytes skipped: no SYNC` |

The detailed CLI text also identifies the first formatter output group's raw offset. The compact note omits that
offset; it remains available internally. `TraceByteSkip::byteCount` counts skipped deformatted payload, excluding
formatter control bytes. In contrast, a formatted recovery interval can include control bytes and interleaved routes;
its length is not a skipped-payload count.

Byte-skip rows bypass type and stream filters, have no timestamp, and use the observed source ID only when known.
They do not create decoded routes, hardware synchronization events, or CTF streams.

### Decoder issues

Compact issue text retains the diagnostic category and meaningful parameters, such as invalid payload size/value,
flush limit, or recovery interval. It includes a numeric native error code or response code when available and keeps
those code domains distinct. Native descriptions, packet types, and bounded byte previews remain in detailed CLI
output. Known semantic issue codes without typed parameters receive meaningful category-only compact fallbacks.

For ordinary decoder issues, CSV includes `; raw@N`. A formatter-group position does not promise an exact physical
packet-byte position. Recovery messages include their raw interval directly. When discontinuity timing is available,
CSV adds `; cycles A..B`, using `?` for an unknown resumed timestamp. Missing timestamps are not invented.

Ordinary issue and overflow rows retain their existing type/stream selection behavior; CLI reporting is unfiltered.
An overflow event uses the compact note `Timestamp discontinuity`. Its CLI aggregate remains a separate catalog
entry selected by `TraceOverflowSummary`, with the optional first timestamp and count for the internal route.

### Fatal aborts

The final CSV note starts with `Decode aborted after N bytes; trace incomplete`. A compact structured cause follows
when available. Arbitrary exception text is not truncated or parsed to manufacture a compact cause. CLI output
retains the complete detailed reason.

Only `OpenCsdFatalError` becomes a shared terminal abort record. Ordinary input-read exceptions and output-backend
failures retain their CLI-only catch paths. The abort row bypasses event filters and has no route or timestamp.
Previously committed CSV rows remain; incomplete CTF artifacts are removed. A CSV write or close failure still
removes the unreliable file. A failure before CSV startup does not create a CSV file.

## Operational messages and foreign detail

`DiagnosticMessages` provides typed adapters for CLI argument checks, trace-run discovery, YAML field validation,
source locations, processor/route binding, deferred metadata errors, path/IO failures, backend lifecycle failures
and run summaries. Each adapter selects an entry in the central catalog. Messages are formatted at their existing
call sites because only the CLI consumes them; they do not become `TraceIssueEvent` values.

Examples include `configFieldMessage(field, ConfigFieldProblem::UnsignedRange)`,
`pathDiagnosticMessage(PathDiagnosticCode::CsvOpen, path)`, and `decodeSummaryMessage(bytes, seconds, records)`.
Fixed messages need no adapter and call `formatMessage` directly. Owned wrappers around foreign text, such as YAML
source locations or backend failure context, also use catalog templates.

Reference/pyTS messages, native YAML/cxxopts/OpenCSD text, OS errors and unexpected exception text retain their
passthrough behavior. `TraceOpaqueMessage` retains foreign or caller-supplied trace descriptions: CLI preserves them,
while CSV uses the semantic issue category as its compact fallback. Opaque abort causes do not pretend to supply a
structured explanation. Severity and failure impact remain at their existing call sites.

## Migration and extension

CSV diagnostic notes intentionally change to compact text. Consumers that compare complete notes must update their
expectations. Columns, event types, filters, route attribution and CSV escaping are unchanged. Use `type` and `stream`
for selection, and detailed CLI diagnostics when investigating native errors or packet bytes. Internal message IDs
are not an additional interchange format.

When adding or changing a message:

1. Add its `MessageId` and adjacent detailed/compact templates to `MessageCatalog.inc`. Use `nullptr` for a CLI-only
   compact template and preserve the existing detailed wording when migrating a caller.
2. For a shared trace diagnostic, add or reuse a typed record and adapt it to catalog arguments at the output
   boundary. Preserve semantic issue mapping and transport the record through every intermediate API.
3. For a CLI-only message, call `formatMessage` directly or extend an appropriate typed adapter. Preserve exception
   type, context, severity and failure impact. Keep foreign text opaque.
4. Test the catalog arguments and both output styles. Verify the producer/consumer path when parameters, optional
   context, filtering, ordering or output lifecycle can change.

Catalog tests cover placeholder validation, argument ownership and substitution, omitted compact parameters,
64-bit values, decimal/hex rendering, native categories/codes, packet previews, timestamp ranges and abort phases.
Decoder, CSV, diagnostics and integration tests cover transport, callback ordering, route attribution, event filters,
recovery, overflow summaries and terminal-abort lifecycle. CSV reference changes are confined to diagnostic notes;
other columns and the CTF/XML fixtures retain their contents.

[catalog]: ../src/model/MessageCatalog.inc
[messages-header]: ../src/model/Messages.h
[messages-source]: ../src/model/Messages.cpp
[trace-header]: ../src/model/TraceMessages.h
[diagnostic-header]: ../src/model/DiagnosticMessages.h
[error-controller]: ../src/decode/OpenCsdErrorController.cpp
[csv-mapper]: ../src/output/csv/CsvRowMapper.cpp
