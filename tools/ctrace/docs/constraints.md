# ctrace Constraints

This document records contracts that implementation changes must preserve. Runtime design and the supported feature
profile belong in the [architecture description](architecture.md), working instructions in the [README](../README.md),
and unfinished work in the [TODO list](todo.md). The CMSIS-Toolbox [trace
specification](https://open-cmsis-pack.github.io/cmsis-toolbox/Experimental-Features/#trace) remains authoritative for
standardized `*.ctrace-run.yml` fields. The `ctrace-run.trace-format` field described below is a ctrace-private,
provisional extension, not a normative CMSIS-Toolbox field or a producer-emission requirement. Its standardization
and producer integration remain tracked as unfinished work.

## Boundaries

- OpenCSD types remain inside the decode layer. Other modules and output backends consume semantic `TraceEvent`
  values, `TraceByteSkip` input annotations without a decoded route or clock, and input-wide `TraceDecodeAbort`
  finalization context.
- An OpenCSD API migration must retain access to typed ITM configuration and packet data and must preserve every
  structured decoder error from each data-path operation; falling back to only the last error or formatted log text
  would change recovery behavior.
- YAML types remain inside the trace-run reader. The rest of ctrace consumes normalized configuration and metadata.
- The YAML reader ignores unrelated fields. Structural and routing fields required to construct the normalized
  configuration are validated while reading or normalizing; malformed consumed values remain errors. Optional null
  scalars and null collection entries are read as absent wherever their schema permits it, while presence-only nodes
  retain their defined flag semantics. Backend-dependent values such as `timestamps.clock` and DWT `address`,
  `data-type`, and `size` retain parse failures for later validation. CTF preflight reports them only when the
  corresponding route or DWT source is selected; absence remains valid where the field is optional. Defaults and
  other operation-specific requirements are likewise evaluated after reading. Missing or null
  `ctrace-setup.itm.enable` is absent; a malformed enable value bound to an active route is an Error. Conflicting valid
  masks on one route produce one Warning and disable that route's optional received-on-disabled-channel check. An ITM
  reference without `source` values is valid and contributes no source events.
- DWT data metadata comes from reference-level `address`, `size`, and `data-type`. When reference `size` is absent,
  the referenced `ctrace-setup.data.size` supplies it. DWT instruction-control references may bind a processor stream
  but do not create decoded data-source routes.
- Backend-specific requirements and failures remain independent; requesting CTF must not disable otherwise valid CSV
  output, or vice versa.

## Input format, framing, and discovery

- Optional `ctrace-run.trace-format` accepts only `unformatted` or `formatted`. An explicit value overrides the
  channel-based default. Missing or null selects `unformatted` for SWO and `formatted` for TB or named-TB without an
  Error. Discovery resolves this effective format before route normalization.
- Exactly one existing `<set>.SWO.raw`, `<set>.TB.raw`, or `<set>.TB_<name>.raw` must be selected, independently of a
  format declaration. Zero or multiple candidates fail before decoder or output construction; SWO has no priority
  over coexisting TB input. The selected input must be a regular, readable file and is opened during preflight,
  before decoder or output construction. Event Recorder input remains diagnosed and excluded from the active candidate count.
- The standardized `trace-buffer` selection belongs to solution/build-run producer configuration, not to the
  `*.ctrace-run.yml` file consumed by ctrace. Until the producer passes an unambiguous selected-file identity and its
  effective format/framing, ctrace's single-input discovery and channel-based defaults remain a transitional input
  policy.
- Formatted input globally uses 16-byte memory-aligned CoreSight frames. Its length must be a multiple of 16, and it
  contains neither FSYNC nor HSYNC framing. Ctrace does not parse or emit a `trace-framing` YAML field; supporting
  another framing mode requires a public trace contract first.
- Format describes the effective bytes in the selected capture, not target capability. The channel-based default is
  a heuristic, not byte-content detection: ctrace does not infer format from synchronization patterns, configured
  route count, or target setup. An explicit declaration is required when the bytes differ from the channel default.

## Routing invariants

- Generated `ctrace-refs.stream` values are the routing authority. Processor `itm` references using a `[pname/]itm`
  path are the preferred route anchors. For compatibility with current pyTS output, only these reference-type and
  `[pname/]feature` pairs may establish a route without that anchor, and only when the reference supplies `stream`:
  `dwt` with a resolved `data#<index>`, `timestamps`, or `synchronization`; `itm` with `timestamps`; `exception` with
  `exceptions`; `event` or `pmu` with `events#<index>`; and `pcsample` with `pcsampling`. The optional `pname/` prefix
  is one path segment; nested feature paths are not fallbacks. `overflow`/`overflow` and `global_ts`/`timesync` may
  describe an established route but cannot establish one. A copied or enriched `ctrace-setup.itm.atbid` is tolerated
  but never creates, changes, or invalidates a route.
- Configured architectural Trace Bus IDs are restricted to `1` through `111` and bind one supported ITM protocol
  route each. DWT and PMU data travel on that processor's ITM route rather than creating separate decoders.
- Unformatted input has one synthetic route with no architectural Trace Bus ID. OpenCSD channel `0`, public stream
  selector `0`, and CTF stream-class ID `0` are compatibility representations of that route, not configured ATB ID 0.
- Formatted ID `0` is NULL/padding and creates no route, semantic event, CTF stream, or Trace Compass lane. A normal
  observed ID without a configured ITM route is diagnosed once and skipped without guessing its protocol.
  Skipped-byte Info may still expose the observed ID, including NULL/reserved IDs, without creating a decoded route.
- Normalized route identity, processor binding, timestamp prescaler, source metadata, errors, synchronization,
  overflow, and data-loss state remain route-local. ITM stimulus ports are restricted to `0` through `31`; port `0`
  is decoded for stream integrity but excluded from payload output.

## Decode invariants

- Both unformatted and formatted inputs use an OpenCSD `DecodeTree`: `SINGLE` for the synthetic route and
  `FRAME_FORMATTED` with one decoder per configured ITM Trace Bus ID. There is no direct-ITM fallback path.
- Only one tree may be live and fed at a time. Its session owns the process-global OpenCSD logger lease, destroys the
  tree before releasing callback state, and restores the previously installed logger on every exit path.
- Raw trace bytes are passed to the decoder unchanged; ctrace never injects synthetic synchronization. Recovery
  resumes only at synchronization present in the input.
- Payload skipped before a source ID is known or assigned to NULL, reserved, or unconfigured IDs is accounted as
  non-failing Info. Bounded accounting counts deformatted payload bytes, not raw/control bytes, and records the first
  formatter output group's raw offset, not an exact discarded-byte position. The source ID is unknown only before
  its first assignment: formatted decoding neither resets the frontend nor enables reset-on-FSYNC.
- On a known formatted route, initial `NOTSYNC` payload is counted in deformatted ITM bytes, never inferred from
  differences between raw offsets. If real synchronization is later committed, a byte-skip Info annotation precedes
  that first sync. An existing route error/recovery interval suppresses duplicate initial-loss accounting. If no
  real sync is committed by end of input, the skipped-byte Info and the separate missing-sync Error below apply.
  These annotations do not replace the existing accounting for general SWO or malformed-packet recovery.
- File-read chunks are not packet boundaries. Decoder state must survive arbitrary read boundaries.
- Error callbacks collect complete stable batches; they do not reset, roll back, emit output, or throw through
  OpenCSD. Classification and recovery happen only after the current synchronous data-path operation returns.
- A recoverable error assigned to a known ITM route discards only that route's failed transaction suffix and resets
  only its packet-processor/full-decoder chain. The frame deformatter, current formatter ID, partially delivered
  frame, and unaffected routes remain intact. A bounded root flush drains pending frame segments before new input.
- The DecodeTree-reported processed-byte count is the only formatted-input cursor. Bytes reported as consumed are
  never re-fed. Only the affected route remains in data loss until a real hardware sync; unresolved loss is closed at
  end of input.
- At formatted end of input, each configured route with received ITM payload but no committed real hardware sync
  reports one route-bound `OpenCsdMissingSync` Error. Rolled-back sync callbacks do not establish synchronization;
  configured routes without received bytes are not errors. This failure returns non-zero while retaining completed
  outputs and healthy routes, instead of silently producing an apparently successful empty conversion.
- A channel-less or deformatter error, failed route reset, incomplete formatted framing/input, unrecoverable response,
  exhausted wait, or repeated lack of progress is input-fatal and stops decoding. Outputs follow the fatal-abort policy
  below: CSV retains committed rows with an abort marker; incomplete CTF and XML are removed. An incomplete packet at
  the end of an unformatted ITM stream retains the legacy recoverable behavior: it is published as a decoder issue and
  does not by itself abort otherwise valid output.
- Discontinuities flush or clear pending route-local DWT state and invalidate timestamp quality before decoding
  continues. Timestamp prescalers default to `1`, accept only `1`, `4`, `16`, or `64`, and are applied exactly once
  after OpenCSD exposes raw ITM ticks.

## Observable behavior and output safety

- CSV remains one combined file in decode callback order. The unformatted route has an empty `stream` field;
  formatted routes expose their architectural IDs. Type and stream filters affect output, not decoding or diagnostic
  reporting. The seventh CSV column is `address`, matching the published CMSIS-Toolbox
  [CSV schema](https://open-cmsis-pack.github.io/cmsis-toolbox/Experimental-Features/#csv-format).
- Byte-skip annotations are retained in CSV regardless of `--type` or `--stream`: `type` is `info`, `note` describes
  the reason, byte count, and formatter-group offset, and `stream` is the observed formatter ID when known, including
  `0` or `127`. All other fields are empty. `info` is an input annotation, not a new CLI type selector. No annotation
  may be represented as a hardware `SYNC`, assigned to a synthetic route, or given an invented time. CTF ignores
  these annotations, so the accounting is CLI/CSV-only. Missing sync at end of input remains a separate route-bound
  Error that always reaches CLI and contributes to command failure, while its CSV row follows ordinary output
  filters; it does not repeat the counted bytes as another loss record.
- Formatted CTF stream files are created lazily as `stream_<id>` only for routes with selected semantic output. Every
  emitted stream class references an explicit clock domain. When selected, the legacy unformatted path retains eager
  `stream_0`, its UUID-optional `swo_clock` metadata form, and companion XML compatibility.
- Generalized CTF metadata records a bound processor name in the corresponding stream-scoped environment entry.
  Its event context preserves the CMSIS-profile `uint8_t cmsis_trace_bus_id` field and adds the ctrace-private
  `ctrace_route` enum used by generated Trace Compass XML. The enum label is the processor name when bound and the
  decimal CTF stream-class ID otherwise. Generalized XML prefixes every state path with that label and then the
  architectural `cmsis_trace_bus_id`; it exposes a separate graphical provider per emitted route and topic, named
  with the resolved processor label when available but never with its numeric ID. The exact legacy CTF event context
  remains unchanged.
- XML declares only graphical outputs: DWT values and addresses as XY series; trace-origin exceptions, DWT matches,
  DWT/PMU overflow events, and processor sleep state as time graphs. Each block is emitted only if the completed stream
  contains matching trace data; synthetic exception bootstrap records alone do not enable an exception block, and
  `Processor State` specifically requires a sleep indication. ITM payloads, ordinary sampled
  PCs, and trace-status records stay available through the CTF event table. Trace Compass XML has no data-driven
  table-view type, so ctrace does not model these point records as artificial timelines.
- `timestamps.clock` has no ctrace fallback. For every route selected for CTF, missing, null, invalid, zero, or
  conflicting frequency is accepted for validation-only and CSV operation but prevents CTF generation with an Error.
  A filter selecting no configured route requires no clock because it can emit no CTF stream. With `--all`, valid CSV
  still completes while the invocation returns non-zero.
- Each formatted route has an independent CTF clock domain, even when processor labels or frequencies match.
  A multi-clock CTF bundle remains valid, but ctrace emits one Warning, removes any stale companion XML, and creates
  no new Trace Compass XML because the supported reader cannot establish a correct combined order. Cross-domain time correlation
  is not inferred.
- CLI decoder warnings and errors remain observable regardless of output filtering. Ordinary route-bound CSV
  diagnostics follow the stream filter and use the `error` type selector, including warning-severity issues. The
  published CSV schema defines no `warning` type or severity column. A recoverable protocol error may be published
  with selected route-bound error/data-loss events even though its Error diagnostic makes the invocation fail.
- A fatal OpenCSD decode abort preserves already committed CSV rows and appends one input-wide `type=error` record
  with `note` set to `decode aborted after processing N input bytes; trace is incomplete: reason`. All other fields
  are empty. This global marker bypasses both type and stream filters; ordinary route-bound errors do not. Partial
  CTF and XML artifacts are removed. This retention and global-marker policy is an explicit ctrace contract, not a
  requirement of the published CSV specification. Failures before CSV startup create no CSV; CSV write or close
  failures still remove the unreliable file.
- Structured diagnostic impact determines command failure; formatted stderr text does not.
- CTF timestamps never regress, and a global timestamp does not by itself establish local timestamp quality.
- Validation-only mode creates no output. Unsupported trace channels are diagnosed and skipped.
- Apart from the explicitly retained CSV after a fatal decode abort, cleanup of incomplete output artifacts is
  attempted after failure, and cleanup failures are reported. Incompatible existing output filesystem types and
  overlapping CTF/XML paths are rejected before replacement.

## Build and CI constraints

- Keep the ctrace workflow's `push.paths` and `pull_request.paths` filters identical. They are limited to the ctrace
  workflow and matrix, the root and ctrace CMake configuration, and ctrace source and tests. Changes made only to
  `.gitmodules` or paths below `external/` must not trigger this workflow.
- Changes to the input, routing, or output-profile contracts require the supported-platform CI, portable unit and
  integration suite, native-Linux Babeltrace consumer gate, source-line coverage gate, and branch-report review
  described in the [build and CI architecture](architecture.md#build-and-ci-structure). XML-shape changes additionally
  follow the golden-update and external-acceptance requirements in the [CTF profile](ctf-format.md#maintaining-the-profile).

Changes to these contracts require corresponding unit or integration coverage. Update the architecture document only
when the implementation structure or data flow changes.
