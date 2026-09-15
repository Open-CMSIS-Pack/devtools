# ctrace Constraints

This document records contracts that implementation changes must preserve. Runtime design and the supported feature
profile belong in the [architecture description](architecture.md), working instructions in the [README](../README.md),
and unfinished work in the [TODO list](todo.md). The CMSIS-Toolbox
[trace specification](https://open-cmsis-pack.github.io/cmsis-toolbox/Experimental-Features/) remains authoritative
for standardized `*.ctrace-run.yml` fields. The root `trace-format` field described below is a ctrace-private,
provisional extension, not a normative CMSIS-Toolbox field or a producer-emission requirement. Its specification and
producer follow-ups are recorded in the
[multi-source decision record](multicore-multisource-plan.md#input-and-routing-contract).

## Boundaries

- OpenCSD types remain inside the decode layer. Other modules and output backends consume semantic `TraceEvent`
  values.
- An OpenCSD API migration must retain access to typed ITM configuration and packet data and must preserve every
  structured decoder error from each data-path operation; falling back to only the last error or formatted log text
  would change recovery behavior.
- YAML types remain inside the trace-run reader. The rest of ctrace consumes normalized configuration and metadata.
- The YAML reader validates fields consumed by ctrace; unrelated fields are outside its validation scope. Malformed
  consumed fields remain errors. Optional null scalars and null collection entries are read as absent wherever
  possible; presence-only nodes retain their defined flag semantics. Defaults and operation-specific requirements
  are evaluated after reading. Missing or null `ctrace-setup.itm.enable` is absent; malformed metadata bound to an
  active route is an Error. Conflicting valid masks on one route produce one Warning and disable that route's optional
  received-on-disabled-channel check. An ITM reference without `source` values is valid and contributes no source
  events.
- DWT data metadata comes from reference-level `address`, `size`, and `data-type`. When reference `size` is absent,
  the referenced `ctrace-setup.data.size` supplies it. DWT instruction-control references may bind a processor stream
  but do not create decoded data-source routes.
- Backend-specific requirements and failures remain independent; requesting CTF must not disable otherwise valid CSV
  output, or vice versa.

## Input format, framing, and discovery

- Root-level `trace-format` accepts only `unformatted` or `formatted`. Missing or null selects `unformatted` without
  an Error and remains an undeclared value for discovery compatibility. An explicit non-null declaration opts the
  eligible SWO, TB, and named-TB candidates into the new selection rule.
- A legacy undeclared configuration activates only `<set>.SWO.raw`; coexisting TB files retain their non-failing
  unsupported-channel Warning. With an explicit format, exactly one existing `<set>.SWO.raw`, `<set>.TB.raw`, or
  `<set>.TB_<name>.raw` must be selected. Zero or multiple candidates fail before decoder or output construction.
  Event Recorder input remains diagnosed and excluded from the active candidate count.
- Formatted input globally uses 16-byte memory-aligned CoreSight frames. Its length must be a multiple of 16, and it
  contains neither FSYNC nor HSYNC framing. Ctrace does not parse or emit a `trace-framing` YAML field; supporting
  another framing mode requires a public trace contract first.
- Format describes the effective bytes in the selected capture, not target capability. Ctrace does not infer it from
  filenames, synchronization patterns, configured route count, or target setup.

## Routing invariants

- Generated `ctrace-refs.stream` values are the routing authority. Processor ITM references are the preferred route
  anchors; only the documented constrained current-pyTS feature fallback may establish a route without one. A copied
  or enriched `ctrace-setup.itm.atbid` is tolerated but never creates, changes, or invalidates a route.
- Configured architectural Trace Bus IDs are restricted to `1` through `111` and bind one supported ITM protocol
  route each. DWT and PMU data travel on that processor's ITM route rather than creating separate decoders.
- Unformatted input has one synthetic route with no architectural Trace Bus ID. OpenCSD channel `0`, public stream
  selector `0`, and CTF stream-class ID `0` are compatibility representations of that route, not configured ATB ID 0.
- Formatted ID `0` is NULL/padding and creates no route, semantic event, CTF stream, or Trace Compass lane. A normal
  observed ID without a configured ITM route is diagnosed once and skipped without guessing its protocol.
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
- File-read chunks are not packet boundaries. Decoder state must survive arbitrary read boundaries.
- Error callbacks collect complete stable batches; they do not reset, roll back, emit output, or throw through
  OpenCSD. Classification and recovery happen only after the current synchronous data-path operation returns.
- A recoverable error assigned to a known ITM route discards only that route's failed transaction suffix and resets
  only its packet-processor/full-decoder chain. The frame deformatter, current formatter ID, partially delivered
  frame, and unaffected routes remain intact. A bounded root flush drains pending frame segments before new input.
- The DecodeTree-reported processed-byte count is the only formatted-input cursor. Bytes reported as consumed are
  never re-fed. Only the affected route remains in data loss until a real hardware sync; unresolved loss is closed at
  end of input.
- A channel-less or deformatter error, failed route reset, incomplete formatted framing/input, unrecoverable response,
  exhausted wait, or repeated lack of progress is input-fatal and aborts every active output. An incomplete packet at
  the end of an unformatted ITM stream retains the legacy recoverable behavior: it is published as a decoder issue and
  does not by itself abort otherwise valid output.
- Discontinuities flush or clear pending route-local DWT state and invalidate timestamp quality before decoding
  continues. Timestamp prescalers default to `1`, accept only `1`, `4`, `16`, or `64`, and are applied exactly once
  after OpenCSD exposes raw ITM ticks.

## Observable behavior and output safety

- CSV remains one combined file in semantic callback order. The unformatted route has an empty `stream` field;
  formatted routes expose their architectural IDs. Type and stream filters affect output, not decoding or diagnostic
  reporting.
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
- Different processor bindings are independent CTF clock domains even when their frequencies match. A multi-clock
  CTF bundle remains valid, but ctrace emits one Warning, removes any stale companion XML, and creates no new Trace
  Compass XML because the supported reader cannot establish a correct combined order. Cross-domain time correlation
  is not inferred.
- Decoder warnings and errors remain observable regardless of payload filtering. A recoverable protocol error may be
  published with route-bound error/data-loss events even though its Error diagnostic makes the invocation fail.
- Structured diagnostic impact determines command failure; formatted stderr text does not.
- CTF timestamps never regress, and a global timestamp does not by itself establish local timestamp quality.
- Validation-only mode creates no output. Unsupported trace channels are diagnosed and skipped.
- Cleanup of incomplete output artifacts is attempted after failure, and cleanup failures are reported. Incompatible
  target types and overlapping CTF/XML paths are rejected before replacement.

Changes to these contracts require corresponding unit or integration coverage. Update the architecture document only
when the implementation structure or data flow changes.
