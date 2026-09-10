# ctrace Multi-Core and Multi-Source Plan

## Goal

Use one OpenCSD `DecodeTree` based input architecture for both the existing unformatted SWO stream and formatted
CoreSight captures containing any number of Trace Bus IDs. One devtools branch and pull request shall contain the
complete change.

The change must preserve all current single-core output and extend it to every currently supported ITM-carried trace
type on every configured processor:

- ITM software trace
- DWT data, address, and match trace
- exception trace
- periodic PC samples and sleep indications
- DWT event counters and PMU trace-on-overflow
- local and global timestamps
- synchronization, overflow, decoder errors, and data-loss events

Non-ITM streams, including instruction trace, may coexist in a formatted input. The current trace-run schema does not
describe their protocol, so ctrace observes their formatter IDs, diagnoses each unsupported ID once, and skips their
payload without guessing a protocol. This is reported as a non-failing Warning so supported routes still complete,
matching today's behavior for unsupported side channels. ETM, ETE, PTM, and MTB instruction decoding and explicit
protocol routing are deferred.

## Restrictions

- Valid normal CoreSight trace source IDs are `0x01..0x6F` (`1..111`). Although `ITM_TCR.TraceBusID` is a 7-bit
  field, not every representable value is a configurable trace source ID. ID `0x00` denotes the NULL source or that
  multi-source trace is not in use. IDs `0x70..0x7F` are reserved or have formatter-control meanings, including
  trigger ID `0x7D`; `0x7F` is prohibited because it conflicts with synchronization encodings. Ctrace already
  enforces the valid normal source-ID range and deterministically rejects configured values `0x70..0x7F`; pyTS
  currently validates only the field width and must be restricted to the same range for processor source assignment.
  That producer correction is tracked outside this devtools PR and does not justify relaxing ctrace's input check.

  | Value | Architectural meaning |
  | :--- | :--- |
  | `0x00` | No multi-source trace in `ITM_TCR`; NULL source in the formatter |
  | `0x01..0x6F` | Valid normal trace source IDs |
  | `0x70..0x7A` | Reserved |
  | `0x7B` | Reserved in CoreSight v2; formatter flush response in CoreSight v3 |
  | `0x7C` | Reserved |
  | `0x7D` | Formatter trigger indication; not a configurable normal source |
  | `0x7E` | Reserved |
  | `0x7F` | Prohibited because it conflicts with formatter synchronization encodings |

  Sources: [Armv7-M Architecture Reference Manual, DDI0403E.e][armv7-m-arm],
  section C1.7.6 and printed pages C1-716 and C1-718;
  [Armv8-M Architecture Reference Manual, DDI0553](https://developer.arm.com/documentation/ddi0553/latest/),
  `ITM_TCR.TraceBusID`;
  [CoreSight Architecture Specification v2.0, IHI0029D][coresight-v2],
  section D4.2.4 on printed page D4-132; and
  [CoreSight Architecture Specification v3.0, IHI0029F][coresight-v3],
  section D4.2.4.

- A Trace Bus ID identifies one ATB trace source, not one processor or one semantic trace event. DWT and PMU packets
  are carried by their processor's ITM stream and share its ID. A separate ETM source on the same processor requires
  another ID.
- An unformatted single-protocol stream contains no Trace Bus ID. OpenCSD deliberately assigns channel ID `0` to the
  decoder in `OCSD_TRC_SRC_SINGLE` mode; this value is internal and must never be interpreted as an architectural
  source ID from the capture.
- The formatter may be enabled for a single source. Neither the number of configured source IDs, the sink type, nor
  the raw filename determines the input format. Multiple interleaved sources require formatting, but one source may
  be captured either formatted or unformatted.
- This PR decodes exactly one active raw input per `ctrace-run.yml`. A legacy file without explicit input metadata
  keeps only its existing `*.SWO.raw` compatibility path active. With explicit input metadata, discovery must resolve
  exactly one matching raw file; zero or multiple candidates are fatal before a decoder or output artifact is
  created. The provisional root-level `trace-format` value applies globally to the trace-run group and defaults to
  `unformatted` when absent or null. The normalized model must retain whether a non-null declaration was supplied:
  an absent or null field uses the legacy SWO-only discovery rule, while an explicit `unformatted` or `formatted`
  value opts SWO/TB candidates into the new input contract. Framing remains an internal ctrace setting and globally
  defaults to memory-aligned CoreSight frames for formatted input; no public `trace-framing` YAML field is introduced
  in this PR. A future specification may refine both assumptions when explicit input identity is added.
- The OpenCSD `DecodeTree` implementation keeps its current error logger and live-tree registry in static process
  state. Ctrace therefore creates and feeds only one tree at a time. The tree-session wrapper installs the ctrace
  logger before tree creation, destroys the tree before that logger, and restores the previously installed OpenCSD
  logger on every exit path. Parallel tree execution is outside this change.

## Evidence from the TB fixture

`tools/ctrace/test/data/TB-Trace/Blinky+Arm.TB.raw` is a 4096-byte reconstructed test fixture containing 256
memory-aligned CoreSight formatter frames. It contains neither FSYNC nor HSYNC framing. An independent Python
deformatter produces:

| Formatter ID | Payload | Observed trace |
| :--- | ---: | :--- |
| `0x01` | 1488 bytes | CM4 address range (`0x0810...`), PC samples, and exceptions |
| `0x02` | 2093 bytes | CM7 address range (`0x0800...`), PC samples, exceptions, and DWT comparator 0 values |
| `0x00` | 7 bytes | Reserved end padding; not a trace source |

Both source payloads begin with an ITM hardware synchronization sequence, and no payload precedes the first formatter
ID. A recorded countercheck with the current single-stream decoder produced 213 semantic rows on ID `0x01` and 312
on ID `0x02` without decoder errors. The fixture preserves the usable real hardware payload and formatter
interleaving from the legacy capture, but swaps the IDs to the current `CM4 = 1`, `CM7 = 2` assignment and adds
synthetic leading synchronization. Its transformations, source/output hashes, and independent deformatter are
documented beside the checked-in canonical fixture in `tools/ctrace/test/data/TB-Trace/README.md`.

The fixture is useful for validating formatter demultiplexing, routing, and semantic output on both processors. It is
not a contract for how pyOCD exports trace-buffer captures. pyOCD owns trace-buffer readout, wrap handling, valid-data
boundaries, and chronological linearization and must provide ctrace with a complete, decoder-ready raw trace file.

The original pyTS 0.1.0 `ctrace-run` file neither records the two stream IDs nor matches the captured setup. The test
copy follows the current per-processor and generated-reference structure and is manually aligned with the observed
capture. Missing capture provenance cannot be reconstructed from the trace bytes.

## Specification gaps confirmed by the fixture

- `stream` associates a generated reference with an ATB ID, but no node explicitly lists the raw input files and
  channels belonging to the trace-run group. The provisional root `trace-format` field supplies their common format;
  ctrace uses an internal memory-aligned framing default for formatted input.
- The valid architectural source-ID range `0x01..0x6F` and the distinction from special formatter IDs are not stated
  in the trace-run schema.
- The standard pyTS workflow preserves disabled and otherwise unmodified user entries in `ctrace-setup`; effective
  generated stream assignments are carried by `ctrace-refs`. `ctrace-refs.stream` is therefore the only routing
  authority. Ctrace may tolerate an `itm.atbid` enrichment produced by a non-standard/direct pyTS generator call, but
  ignores it for routing and does not require it to agree with the generated references. Group-level input format and
  generated route metadata follow the same separation between copied user intent and generated effective
  configuration.
- The trace-run specification defines timestamp references as `type: itm`, while current pyTS emits `type: dwt`.
  Ctrace needs the transitional normalization rule below until pyTS emits the normative form.
- Per-processor timestamp clocks are representable, but the required behavior for independent clock domains and the
  absence of global time synchronization in combined CTF output is not defined.

## Input contract

The raw bytes do not reliably identify whether they are an unformatted protocol stream or CoreSight frames. Ctrace
must not guess from synchronization patterns or the number of configured Trace Bus IDs.

One trace-run configuration defines a group of associated raw inputs. For this PR, the existing filename heuristic
selects exactly one active input from the group. The following assumptions are deliberately provisional until the
CMSIS-Toolbox trace specification defines an explicit input model:

- optional root-level `trace-format: unformatted | formatted` applies globally to every trace file in the group;
- missing or null `trace-format` silently selects the global default `unformatted` and remains undeclared for legacy
  candidate selection;
- framing is not exposed as a `ctrace-run.yml` field; ctrace stores one internal global framing value and defaults it
  to memory-aligned CoreSight frames whenever `trace-format` is `formatted`;
- FSYNC and HSYNC input are outside this PR and may later require a specified public framing field;
- one valid Trace Bus ID identifies each supported formatted ITM source; a processor `[<pname>/]itm` reference is the
  preferred authoritative route anchor, with the narrowly constrained current-pyTS feature-reference fallback below.

The absent-or-null default preserves the existing legacy `*.SWO.raw` path; it does not make a coexisting legacy TB or
ER side file a second active input. An explicit non-null `trace-format` opts the eligible SWO/TB candidates into the
new global input contract. Discovery must then resolve exactly one matching raw file; zero or multiple candidates are
fatal before decoder or output creation.

An explicit channel/file list is future discovery metadata. It may replace the filename heuristic and refine the
current global assumptions. Route identity then expands from ATB ID to `(input identity, ATB ID)`.

### Provisional `ctrace-run.yml` extension

Decision record for this devtools PR: the root `trace-format` field, its absent/null/explicit behavior, and the internal
memory-aligned framing default are ctrace-private working assumptions. They are not normative CMSIS-Toolbox schema,
and this PR neither changes a capture producer nor claims that pyTS or pyOCD emits them. The reconstructed TB fixture
is explicitly and manually annotated consumer test input. Phase 0 freezes this boundary and records separate
specification/producer follow-ups; those external changes are not gates for the devtools phases. A future normative
input model may require a deliberate reader migration while the legacy absent/null behavior remains compatible.

The external work is recorded here with an explicit owner boundary; no external issue or pull request is created by
this devtools change:

| Owner repository | Follow-up | Relationship to this PR |
| :--- | :--- | :--- |
| [CMSIS-Toolbox](https://github.com/Open-CMSIS-Pack/cmsis-toolbox) | Specify input identity, format, framing, and the ownership of effective capture metadata in `ctrace-run.yml`. | May replace the private field through a deliberate reader migration; not a devtools gate. |
| [pyTS](https://github.com/Open-CMSIS-Pack/pyTS) | Restrict allocated `ITM_TCR.TraceBusID` values to `1..111`; after the toolbox contract exists, emit its normative effective-format metadata. | Ctrace remains strict now; producer alignment is tracked outside this PR. |
| [pyOCD](https://github.com/pyocd/pyOCD) or the invoking capture integration | Export a decoder-ready, chronologically linearized trace-buffer artifact and communicate the effective formatter state through the future toolbox contract. | The manually annotated TB fixture supplies only consumer-side evidence in this PR. |

Within that boundary, this PR introduces one provisional decoder-control field directly below the `ctrace-run` root:

```yml
ctrace-run:
  trace-format: formatted
```

- `trace-format`
  - Values: `unformatted`, `formatted`.
  - Default: `unformatted` when the field is absent or null; no diagnostic is emitted for this default, and null does
    not count as an explicit format declaration during discovery.
  - Effect: selects an OpenCSD `SINGLE` input or `FRAME_FORMATTED` deformatter.

The internal framing value is `memory-aligned` and maps to `OCSD_DFRMTR_FRAME_MEM_ALIGN`. A formatted file therefore
begins at a complete 16-byte CoreSight formatter-frame boundary, contains no formatter FSYNC/HSYNC words, and has a
size that is a multiple of 16 bytes. This internal default is deliberately not parsed from or emitted to YAML. If a
future input requires another framing mode, that mode must first be added to the public trace contract.

`trace-format` describes the effective bytes in the capture artifact, not target capability or copied user intent.
It must not be copied into processor setup entries or repeated on feature references. The reader retains the optional
declaration; the normalized descriptor stores both its resolved value and whether the declaration was explicit, plus
the internal framing default for decoder construction. These are one configuration source, not competing settings.
The capture producer that knows the effective formatter state must eventually emit equivalent format information;
ctrace cannot reconstruct it from copied target configuration. Producer emission and schema standardization are
deliberately outside this devtools PR.

Concretely, discovery considers `<solution-set>.SWO.raw`, `<solution-set>.TB.raw`, and the specification-defined
`<solution-set>.TB_<name>.raw` form as decoder candidates. This PR still accepts exactly one active file, including
at most one named Trace Buffer; simultaneous inputs need the future explicit file-association model. Without an
explicit format field, only the SWO name is eligible for the legacy compatibility path. A coexisting legacy TB or
`TB_<name>` file retains today's non-failing "unsupported channel" Warning and does not
invalidate the selected SWO conversion or its completed output. With explicit metadata, either filename is eligible
and zero or more than one existing candidate is fatal before output creation. `<solution-set>.ER.raw` remains a
discovered but unsupported side input in this PR: emit the same non-failing Warning and exclude it from the active
decoder-candidate count. If an eligible SWO/TB input exists, its conversion still completes; an ER-only group reports
the Warning and then fails because no eligible input remains. This rule removes ambiguity when SWO and TB artifacts
coexist and is replaced, not extended, by the future explicit channel/file list.

The reconstructed TB fixture deliberately retains the existing generated `ctrace-setup` and `ctrace-refs` content
needed by this PR:

- `ctrace-setup[].pname` plus `ctrace-refs[].pname` and `stream` bind Trace Bus IDs 1 and 2 to CM4 and CM7;
- `timestamps.clock` and `timestamps.itm-prescaler` supply the independent time configuration for each processor;
- `type`, `source`, `address`, `data-type`, and `size` retain the source metadata needed by CSV filtering and
  per-stream CTF metadata;
- reference `info`, `warning`, and `error` diagnostics retain their existing handling. A diagnostic annotation does
  not by itself decide whether the readable fields are usable; the normalized ctrace model validates those fields
  independently before it creates a route.

Other generated setup and register fields remain in the realistic fixture but do not gain new semantics in this PR.
`unformatted` selects an OpenCSD `SINGLE` input whose bytes carry no Trace Bus ID; `formatted` selects the CoreSight
frame deformatter and preserves the IDs carried by the frames.

Trace-buffer implementation details such as RAM bounds, sink write position, wrap state, and chronological
linearization remain internal to pyOCD. Ctrace consumes the resulting decoder-ready file and does not reconstruct a
trace-buffer image.

A `single` input always creates exactly one synthetic logical ITM route with no ATB ID because its raw bytes carry no
ID. Legacy configurations therefore remain valid even when `ctrace-refs` is empty or no reference contains
`stream`. With no matching processor metadata, documented time/label defaults apply. One unambiguous binding is
attached directly; multiple candidates may be merged only when every processor-specific value consumed by the
requested operation is equivalent. Ambiguous processor identity or prescaler metadata prevents decoding because the
bytes cannot choose the required interpretation. Clock, source-description, and label conflicts affect only an
output backend that consumes them; they do not invalidate an otherwise viable independent operation. A configured
architectural ID may help resolve metadata but is not copied onto the synthetic route. OpenCSD reports its transport
channel as ID `0`. A formatted input instead retains the architectural IDs found in its frames, also when it contains
only one source.

The current `*.cbuild-run.yml` processor list is sufficient for pyTS to select processor implementations but does not
describe the actual formatter state or raw capture path. Ctrace must not read it as a second and potentially
conflicting source of capture metadata. The component that knows the effective debugger and sink configuration must
record that resolved information in `*.ctrace-run.yml` before decoding.

`trace-format` is a ctrace-private provisional global ctrace-run extension used by this implementation and manually
annotated fixture. It requires agreement in the CMSIS-Toolbox trace specification before any producer can emit it as
a normative field. Internal framing remains memory-aligned and is not a YAML extension. Producer changes and the
exact future node for explicit channel/file association remain outside this PR and may refine these working
assumptions. Existing `*.SWO.raw` inputs without the new metadata remain compatible and default to the current
unformatted single-ITM interpretation. Formatted input without sufficient routing metadata is rejected with a clear
diagnostic instead of being decoded heuristically.

## End-to-end contract

Ctrace resolves the raw input and `ctrace-run` together. File discovery identifies the capture artifact; normalized
trace-run metadata identifies how it must be decoded and how every resulting stream is described to the outputs.
For correctly generated pyTS input, disabled setup entries have no effective references and therefore create no
routes. Stale references without a matching effective processor route are rejected by normalization rather than
silently treated as valid input.

The `info`, `warning`, and `error` lists attached to reference nodes describe producer diagnostics. The current
schema does not define these fields on `ctrace-setup` entries. Ctrace forwards every diagnostic found on a reference
it already reads, preserving its severity, but does not assume that an `error` annotation makes every scalar on that
reference false. It then validates the readable data through its own normalized model. A complete, consistent model
may continue despite the reported producer error. Missing, contradictory, out-of-range, or ambiguous
routing-critical information without a documented fallback is a fatal ctrace configuration error. Output-specific
requirements are validated separately after reading and normalization. Missing, null, malformed, zero, or ambiguous
clock metadata prevents only CTF generation and emits an Error; no frequency is invented. Validation-only and CSV
decoding remain available. With `--all`, CSV still completes while CTF is not created and the Error keeps the command
status non-zero. The decoder lifecycle starts after the input descriptor passes input-level validation and at least
one requested operation remains viable.

- A legacy `*.SWO.raw` preserves the compatibility rule and creates a `SINGLE` ITM input. Differing
  processor-specific settings require one unambiguous effective setup. OpenCSD reports internal ID `0`; CSV leaves
  `stream` empty and CTF writes `stream_0`.
- An eligible `*.SWO.raw`, `*.TB.raw`, or `*.TB_<name>.raw` with explicit `trace-format: unformatted` binds one synthetic
  protocol/processor route and creates a `SINGLE` decoder. The filename and physical sink do not override the
  declaration. Transport ID remains `0`; explicit metadata supplies processor clock, prescaler, and labels.
- An eligible `*.SWO.raw`, `*.TB.raw`, or `*.TB_<name>.raw` with explicit `trace-format: formatted` creates a
  `FRAME_FORMATTED` input
  with the internal memory-aligned framing default and one ITM protocol decoder per supported normalized route. The
  filename and source count do not override the declaration. Supported formatter IDs survive semantic decoding and
  both outputs; other IDs are diagnosed and skipped.
- A `*.TB.raw` or `*.TB_<name>.raw` without input metadata is rejected as ambiguous before decoder or output
  creation. The heuristic may discover it, but the bytes and existing references do not determine formatting in the
  general case.

Formatted CoreSight ID `0` is NULL/padding/control data and is consumed by the deformatter without creating a
protocol route or semantic event. It must not create a CSV stream value, a CTF stream class or file of its own, or a
Trace Compass lane.

The normalized model contains one input descriptor and explicit protocol routes, conceptually:

```cpp
struct TraceInputRoute {
  RouteId id;
  std::optional<std::uint8_t> traceBusId; // present only for formatted CoreSight input
  TraceProtocol protocol;
  std::optional<std::string> processorName;
};
```

A configured ATB value may participate in `SINGLE` metadata normalization, but it is discarded before this route is
created and never becomes transport, CSV, or CTF identity.

For formatted input, normalization builds exactly one ITM decoder route per unique valid `stream`. An effective
`[<pname>/]itm` reference with `type: itm` is the preferred authoritative processor-ITM anchor; `source` on it denotes
an ITM stimulus channel, not another route. If an anchor is present, malformed fields or a conflict with another
reference are fatal and cannot be hidden by the fallback below.

Current pyTS can emit a stream-bearing feature reference without an implicit processor-ITM anchor for an unnamed
single-processor setup. When no anchor exists for a stream, ctrace may therefore establish the same ITM route from a
unique, internally consistent group of generated feature references, but only for supported ITM-carried setting
pairs: `data#*` with `type: dwt`, `timestamps` with `type: itm` or the transitional `type: dwt`, `exceptions` with
`type: exception`, `events#*` with `type: event` or `type: pmu`, `pcsampling` with `type: pcsample`, and
`synchronization` with `type: dwt`. Streamless references, instruction/trace-halt conditions, `timesync`, unsupported
types, and unknown path/type pairs never create a fallback route. This compatibility rule is sufficient to associate
the decoder consumer with pyTS' declared stream; it does not prove that a capture producer programmed
`ITM_TCR.TraceBusID` correctly. Observed formatter IDs still have to match a configured route or follow the
unsupported-ID policy.

All references for a stream must normalize to one compatible `(ITM, pname)` binding, and one bound processor ITM
binding must not map to multiple streams. Distinct explicitly unbound routes do not acquire a shared processor binding
merely because both omit `pname`. With multiple active processor setups, `pname` must identify the matching setup;
with one setup, an omitted `pname` may use the existing unique-setup inference. An absent setup does not prevent
decoding an otherwise unambiguous route, but processor-specific metadata then uses documented defaults. Compatible
DWT, exception, event, PMU, timestamp, PC-sampling, overflow, and global-timestamp references describe content on the
route and do not create additional routes. The reader retains common binding metadata and diagnostics for all
supported reference types (`dwt`, `event`, `exception`, `itm`, `pmu`, `overflow`, `pcsample`, and `global_ts`).
Repeated consistent stream IDs are valid; route uniqueness does not mean that a stream number may occur only once in
`ctrace-refs`. `ctrace-refs.stream` is authoritative. An optional copied or enriched `ctrace-setup.itm.atbid` is
ignored for routing and cannot repair or invalidate the generated binding.

The normative timestamp reference uses `type: itm`; current pyTS emits `type: dwt`. Ctrace temporarily accepts the
second spelling only when the `ctrace-ref` leaf is `timestamps`, then normalizes both to the same local timestamp
configuration of the ITM route. A timestamp reference never creates a second route or a DWT comparator source, but
may establish the constrained no-anchor fallback above. Any other spelling or a conflicting `(pname, stream)` binding
is fatal. Tests retain both accepted forms until pyTS matches the normative spelling.

CoreSight routing identity, CTF identity, and filesystem naming are separate concepts:

- the Trace Bus ID comes from the formatted CoreSight transport and is resolved through `ctrace-refs`;
- the CTF stream-class ID is a backend-assigned numeric identifier referenced by the packet header, the matching
  `stream { id = ...; }` declaration, and each event's `stream_id`;
- a data-stream filename is not referenced by CTF metadata. `stream_<n>` is only a ctrace naming convention.

[CTF 1.8][ctf-spec], section 5,
requires neither stream-class ID `0` nor contiguous IDs. Its
[filesystem representation][ctf-spec], section 2,
does not define data-stream filenames. For this PR, ctrace deliberately uses the formatted Trace Bus ID as the CTF
stream-class ID and assigns CTF ID `0` to an unformatted `SINGLE` input. This direct mapping keeps diagnostics and
artifacts simple, but it is an explicit backend choice rather than a CTF requirement. The file is named
`stream_<ctf-stream-class-id>`; no logic may infer a source identity from that filename alone.

Ctrace reserves the following CTF-local stream-class ID namespace:

| CTF stream-class ID | ctrace assignment |
| :--- | :--- |
| `0x00000000` | Unformatted `SINGLE` input |
| `0x00000001..0x0000006F` | Formatted CoreSight source; equal to its Trace Bus ID |
| `0x00000070..0x0000007F` | Unused, matching the reserved/special CoreSight 7-bit values |
| `0x00000080..0x000000FF` | CMSIS Event Recorder instances `0..127`, using `0x80 + instance` |
| `0x00000100..0xFFFFFFFF` | Other backend-only streams, allocated deterministically |

CMSIS Event Recorder has no CoreSight Trace Bus ID. `Event Recorder<n>` maps explicitly to CTF stream-class ID
`0x80 + n` for `n = 0..127`; filenames render that numeric ID in decimal, so `Event Recorder<0>` is written to
`stream_128`, `Event Recorder<1>` to `stream_129`, and so on. The assigned ID is local to the CTF bundle and is not
written back into `ctrace-run.yml` or exposed as a Trace Bus ID. Metadata and the source descriptor remain
authoritative for the source kind. Event Recorder decoding itself remains outside this PR. If a later feature needs
more than 128 Event Recorder instances, or multiple raw inputs introduce overlapping CoreSight ID namespaces, its
collision-free CTF allocation must be specified before extending this namespace.

The one-input constraint means an ATB ID uniquely identifies a formatted route in this PR. Future multiple-input
support must key routes as `(input identity, ATB ID)` because different formatter domains may reuse the same ATB ID,
and multiple unformatted inputs all use OpenCSD transport channel `0`.

The CTF model therefore stores stream identity and time-domain identity explicitly instead of treating IDs or equal
frequencies as aliases:

```cpp
using CtfClockDomainId = std::uint32_t;

struct CtfClockDomainDescriptor {
  CtfClockDomainId id;
  std::string name;
  std::optional<Uuid> uuid;
  std::uint64_t frequencyHz;
  bool absolute = false;
};

struct CtfStreamDescriptor {
  std::uint32_t streamClassId;
  TraceSourceKind sourceKind;
  std::optional<std::uint8_t> traceBusId;
  std::optional<std::string> processorName;
  CtfClockDomainId clockDomainId;
};
```

`cmsis_trace_bus_id` is meaningful only when `traceBusId` is present. An Event Recorder stream must not put its CTF
ID `0x80` into that field as though it were a CoreSight ID.

The decoder produces one common semantic `TraceEvent` flow. Each event carries a normalized source-route identity,
local cycle timestamp, source information, payload, and quality state. A formatted CoreSight route contains its ATB
Trace Bus ID; an unformatted or non-CoreSight route does not. Processor name, clock, prescaler, configured source
metadata, and output labels remain on the normalized route and are resolved from that identity instead of being
copied into every event.

The existing `TraceSelection` remains one shared output contract for CSV and CTF. All supported routes are decoded
so that synchronization, recovery, and diagnostics stay correct; `--stream` and `--type` are applied to normalized
semantic events immediately before backend output. Both backends reuse `traceEventSelectedForOutput` instead of
implementing separate filter rules. Several values after one option, for example `--type dwt itm`, form a union of
types; when both stream and type filters are present, both predicates must match. Diagnostic reporting observes the
complete decoded flow independently of output filtering. CTF creates lazy stream, clock, metadata, and XML artifacts
only after the first selected event, so a fully filtered route remains artifact-free.

The output behavior is deliberately asymmetric:

- CSV writes one combined file in synchronous semantic callback order. It preserves the stable `stream`, `type`,
  `source`, `value`, `pc`, `address`, and diagnostic fields. Trace-run data controls routing, timestamp-prescaler
  correction, validation, and filtering, but does not add processor names, clock frequencies, configured addresses,
  or other inferred values to the CSV schema.
- CTF writes one bundle containing one shared `metadata` file and one `stream_<ctf-stream-class-id>` file for every
  emitted CTF stream descriptor. It uses trace-run processor names, clocks, source labels, DWT data types/sizes, and
  address ranges as CTF metadata. All stream files share the trace UUID, while each stream class references one
  explicit clock domain and owns its packet sequence.
- CSV retains the synchronous semantic callback order produced by OpenCSD. Pending packets may be released by a
  later timestamp on their own stream, so this is not a promise of byte-exact raw-input order across streams. A CTF
  reader can scale each stream with its assigned clock; it can define a global event order only for clock domains
  with a proven common reference.

## Target architecture

```text
raw file discovery                 ctrace-run
  SWO.raw / TB[_name].raw  trace-format, setup, refs, routes
          \                         /
           +---- normalized input + stream metadata
                              |
                    OpenCSD DecodeTree session
                      /                    \
               SINGLE input           FRAME_FORMATTED input
               one protocol          route by Trace Bus ID
                      \                    /
                   stream-bound protocol decoders
                              |
                    CortexMStreamDecoder
                 independent state per stream
                              |
                    semantic TraceEvent flow
                       /                    \
             one ordered CSV          one CTF bundle
                                      metadata
                                      stream_<ctf-class-id>...
```

One `DecodeTree` is created per raw input. `OCSD_TRC_SRC_SINGLE` connects one configured ITM decoder directly;
`OCSD_TRC_SRC_FRAME_FORMATTED` creates the deformatter and connects one decoder for each configured ITM Trace Bus ID.
DWT and PMU packets share their processor's ITM stream and do not create separate OpenCSD decoder instances.
An ETB or ETR capture containing only one ITM source still uses the formatted path when its formatter was enabled.
The formatted deformatter discards ID-0 NULL/padding data. OpenCSD uses transport channel `0` only for unformatted
`SINGLE` mode; the CTF backend independently assigns that route CTF stream-class ID `0`.
The `DecodeTree` is the mandatory OpenCSD entry point for every raw input, including legacy `*.SWO.raw`. There is no
second direct-ITM raw-input architecture; unformatted SWO is represented by the tree's `SINGLE` input mode.

## Instance and state audit

The existing ctrace implementation contains no mutable process-global trace state. File-local `static` functions,
`constexpr` tables, and immutable default values are stateless and can be shared safely. The OpenCSD decoder registry
is a shared factory/registry, while every decoder component and its protocol state are owned by the session/tree.
Logical multi-stream decoding remains synchronous; this plan does not require concurrent calls from multiple
threads.

The following instance boundaries already support independent streams:

- `CortexMStreamDecoder` owns a lazy `Trace Bus ID -> CortexMPostDecoder` map.
- Each `CortexMPostDecoder` owns its timestamp, overflow, pending-event, and `DwtPacketDecoder` state.
- Each `DwtPacketDecoder` owns its pending comparator correlation state.
- `CtfEncoder` already partitions semantic timestamp, overflow, and exception-lane state by Trace Bus ID.
- `DecodeConsumers` and `TraceOutputLifecycle` deliberately fan one semantic event flow out to the selected outputs.
- `CsvFileOutput` deliberately remains one instance because CSV preserves the combined decoder callback order; the
  `stream` column identifies formatted sources.

The following shared-instance assumptions must be changed or reviewed during implementation:

- `YmlTraceRunConfigReader` currently drops a setup as soon as it sees `disable`. Preserve each disabled setup
  fragment's optional `pname`, original ordinal, and referenceable feature paths, but do not invent producer-
  diagnostic fields that are not part of the `ctrace-setup` schema. A disabled fragment contributes no active decoder
  metadata and never disables another active fragment with the same `pname`. It lets normalization reject a stale
  reference only when that path resolves exclusively to disabled fragments, while keeping disabled and absent setups
  distinguishable.
- `DecodePipeline` currently owns one direct `OpenCsdItmDecoder`; replace it with one tree session that owns one or
  more protocol decoder components.
- `OpenCsdPacketCollector` currently has one tree-wide transaction buffer. Replace it with route-aware buffering so a
  protocol error can discard only the failing route's uncommitted elements at or after the error offset while
  retaining events from other routes. Raw packet-monitor callbacks need a small per-decoder adapter that supplies the
  bound Trace Bus ID. Input-wide framing failures still discard the complete transaction.
- `OpenCsdItmDecoder` currently tracks one input-wide data-loss/resynchronization interval. Replace it with per-route
  recovery state: one route's next synchronization/event must not close another route's data-loss interval.
- `OpenCsdErrorRecord` and resulting diagnostics must retain an optional OpenCSD channel ID. Channel `0` is a valid
  `SINGLE` channel, normal formatted IDs are `1..111`, and `OCSD_BAD_CS_SRC_ID` (`0xFF`) maps to no channel for a
  deformatter/input-wide error; it is unrelated to CTF stream-class ID `255`.
- `OpenCsdErrorController` already collects callbacks during one synchronous OpenCSD operation and decides after that
  operation returns. Preserve this non-reentrant boundary, but extend its single-error decision into a complete batch
  grouped by normalized route. Fatal, non-recoverable, or channel-less errors take precedence; every retained warning
  and recoverable route error is still reported.
- OpenCSD's alternate `DecodeTree` error logger is process-global. Encapsulate its installation and restoration in
  the tree-session lifetime, restore the previously installed logger, and do not let a global OpenCSD callback retain
  a pointer to destroyed ctrace state.
- `TraceIssueReporter` currently aggregates its overflow summary across all streams. Keep overall diagnostics if
  useful, but partition overflow counters and first timestamps by Trace Bus ID and include the stream in messages.
- `TraceOutputLifecycle` deliberately lets one backend fail while other backends continue and publish their output.
  Preserve this contract: a CTF-specific configuration or writer failure must not disable otherwise valid CSV, and
  vice versa. Within the CTF backend, treat metadata, all emitted stream files, and the optional companion XML as one
  bundle for completion and cleanup so a failed or repeated conversion cannot leave stale CTF streams or XML lanes.
- `CtfEncoder` currently owns one binary `CtfStreamWriter`; replace it with one writer instance per emitted CTF
  stream descriptor, indexed by CTF stream-class ID, and move trace UUID ownership to the enclosing bundle/encoder.
- Add one bundle-local `CtfMetadataModel` that owns common trace identity and all per-stream declarations. Populate it
  from normalized trace-run data and augment it with observations during decoding. It is not a process-global
  singleton. Keep `CtfMetadataWriter` as a stateless serializer of the completed model.
- `CtfMetadataWriter` currently keys ITM and DWT symbols only by channel/comparator number. In the new model, key
  source metadata by `(Trace Bus ID, source)` and generate stream-specific type aliases/environment names so equal
  source numbers on different processors cannot overwrite each other's labels, types, sizes, or addresses.
- `TraceCompassXmlWriter` currently builds one global state-system path per event/source. Prefix relevant state paths
  with Trace Bus ID or resolved processor identity so exceptions, sleep, DWT sources, and counters from different
  processors do not merge into the same GUI lane.

## Sequential implementation phases

This section defines the mandatory execution order. The complete change stays on one topic branch and in one
devtools pull request, but every phase is implemented as a focused, reviewable commit series. A phase may add tests
before production code, but its final commit must leave the branch buildable and all established behavior green.
Formatted input is never passed to the unformatted decoder as an intermediate shortcut.

```text
Phase 0 -> Phase 1 -> Phase 2 -> Phase 3 -> Phase 4
        -> Phase 5 -> Phase 6 -> Phase 7 -> Phase 8 -> Phase 9
```

| Phase | Deliverable | Status |
| :--- | :--- | :--- |
| 0 | Baseline, fixtures, goldens, coverage gate | Complete |
| 1 | Trace-run declaration and route normalization | Complete |
| 2 | Raw-input discovery and preflight | Complete |
| 3 | Route-aware semantic state, diagnostics, and CSV | Complete |
| 4 | CTF descriptors and metadata model | Complete |
| 5 | Multi-stream CTF bundle and Trace Compass policy | Complete |
| 6 | DecodeTree `SINGLE` migration | Complete |
| 7 | Clean formatted decoding and TB integration | Next |
| 8 | Route-local recovery and error isolation | Pending |
| 9 | Consumer validation, documentation, and final hardening | Pending |

Update this table only after the corresponding exit criterion and common gate pass.

The implementation baseline includes the merged
[devtools PR #2602](https://github.com/Open-CMSIS-Pack/devtools/pull/2602); this topic branch is based on the resulting
`main` and must not duplicate its reader changes. Its null contract applies throughout every phase: optional null
scalars and null entries are read as absent wherever possible, unrelated nodes and diagnostics remain ignored, and
defaults or mode-specific requirements are evaluated only after reading. Presence-only nodes retain their specified
meaning. In particular, `trace-format: null` behaves like an omitted declaration and selects the legacy global
`unformatted` default. Missing or null clock remains acceptable for validation-only and CSV operation but is an
Error that prevents CTF generation.

### Gate after every phase

Before starting the next phase:

1. Format changed C++ and CMake files and run `git diff --check`.
2. Build ctrace and run the complete portable unit and integration suite, not only the new tests.
3. Run the deterministic 100% ctrace source-line coverage gate and inspect the unfiltered branch report. Do not add
   exclusions to make a phase pass.
4. Re-run the relevant legacy SWO golden-output tests. Any intentional artifact change must be explained and
   approved in the plan before updating a golden file.
5. Review the complete phase diff for ownership, cleanup, error paths, and accidental changes outside ctrace. Fix all
   findings before proceeding.

The supported-platform CI matrix is required whenever a phase is pushed. Local success is not used to dismiss a
platform failure.

### Phase 0: Freeze the baseline and fixtures

Purpose: establish reproducible evidence and quality gates without changing runtime behavior.

1. Freeze the ctrace-private decision record for root `trace-format`, absent/null/explicit declaration behavior, and
   internal memory-aligned framing. State that the manually annotated TB fixture is consumer test input, that this PR
   makes no normative CMSIS or producer-emission claim, and that specification/producer work is tracked separately
   rather than gating the devtools phases. Record the external pyTS follow-ups for the `1..111` assignment range and
   eventual normative capture-format metadata.
2. Add this reviewed plan, the TODO links, and the documented reconstructed `TB-Trace` fixture with its independent
   deformatter/analysis helper.
3. Verify every checked-in SHA-256 value, formatter payload count, source ID, and decoded CM4/CM7 semantic row count.
   Keep the helper outside the ctrace runtime and build.
4. Record the current legacy SWO CSV and CTF/XML artifacts as the compatibility baseline.
5. Add or confirm a deterministic 100% source-line coverage gate before the functional phases begin. Keep the
   ctrace workflow push and pull-request path filters identical and limited to their existing ctrace scope.
6. Run the complete baseline gate. No production source behavior changes in this phase.

Exit criterion: the private-contract boundary is explicit, and the fixture, legacy artifacts, complete test suite,
and coverage result are reproducible from the topic branch.

### Phase 1: Normalize trace-run declarations and routes

Purpose: create one schema-aware model before raw-file discovery or decoding changes.

1. Extend `TraceRunConfig` with the optional global `trace-format` declaration. Missing and null remain absent;
   explicit `unformatted` and `formatted` remain distinguishable from the default so discovery can preserve legacy
   behavior.
2. Until Phase 7 enables the formatted frontend, stop a job with an explicit effective `formatted` declaration
   deterministically before raw-input discovery or selection, before the legacy frontend can receive bytes, and
   before decoder or output construction. This temporary guard is part of Phase 1 even though final discovery moves
   to Phase 2.
3. Preserve disabled setup fragments with optional `pname`, original ordinal, and referenceable feature paths instead
   of active metadata. `disable` remains a presence-only node, independent of its YAML value, and applies only to its
   own fragment.
4. Retain binding metadata and diagnostics for every consumed reference type. Normalize authoritative processor-ITM
   anchors, the constrained current-pyTS feature-reference fallback, and normative/transitional timestamp spellings
   without creating decoder objects.
5. Build a normalized route catalogue with processor association, optional architectural Trace Bus ID, timestamp
   clock/prescaler metadata, ITM enable mask, source descriptions, and producer diagnostics.
6. Validate only routing-critical structure at this layer: reference-authoritative IDs in `1..111`, unique compatible
   bindings, valid anchor-or-fallback evidence, fragment-local disabled-reference resolution, and ambiguous processor
   association. Ignore a copied/enriched `ctrace-setup.itm.atbid` for routing, reject configured stream values
   `112..127`, and keep output-specific clock and DWT metadata requirements deferred.
7. Cover missing/null/invalid format values, the pre-Phase-7 formatted guard, disabled fragments, anchor and fallback
   routes, consistent and conflicting bindings, references without `source`, producer diagnostics, both timestamp
   spellings, tolerated `itm.atbid`, and rejected values `112..127` with focused reader/model/job tests.

Exit criterion: trace-run parsing and normalization are independent of filenames and outputs; existing legacy
decoding still uses the old raw frontend unchanged, while an explicit formatted declaration is safely rejected before
that frontend or any output can be reached.

### Phase 2: Resolve and preflight exactly one raw input

Purpose: establish the final input-selection boundary before any decoder or output is created.

1. Introduce the normalized input descriptor containing selected path/channel, effective format, declaration state,
   internal framing, and normalized routes.
2. Implement the legacy and explicit discovery matrix: absent/null format activates only SWO; an explicit non-null
   format makes SWO, TB, and specification-defined `TB_<name>` channels eligible; ER remains a diagnosed but
   unsupported side input.
3. Resolve exactly one eligible file and reject zero or multiple candidates. Preflight regular-file status,
   readability, and memory-aligned formatted size before constructing outputs or OpenCSD state.
4. Reorder job construction so trace-run read, normalization, discovery, input preflight, and output-requirement
   evaluation happen before output creation. Preserve backend independence for later CTF-only requirement failures.
5. Continue decoding legacy SWO and explicitly unformatted SWO/TB/`TB_<name>` with the existing direct frontend.
   Retain the
   Phase-1 formatted-input guard at the normalized descriptor boundary until Phase 7; after discovery and preflight,
   reject formatted input before output creation and never feed it to the unformatted decoder.
6. Test the complete SWO/TB/`TB_<name>`/ER matrix, empty inputs, unreadable/non-regular files, ambiguous candidates,
   partial formatted frames, and proof that failed preflight creates no artifact or decoder.

Exit criterion: every job owns one validated descriptor and existing unformatted output remains compatible.

### Phase 3: Make semantic state, diagnostics, and CSV route-aware

Purpose: remove single-stream assumptions below the raw frontend before enabling formatted bytes.

1. Introduce a stable normalized route identity. Preserve the optional architectural Trace Bus ID separately from
   OpenCSD's transport channel `0`, so a synthetic unformatted route is never mistaken for CoreSight source ID `0`.
2. Carry the route identity through `OpenCsdTraceElement`, `CortexMStreamDecoder`, `TraceEvent`, diagnostics, selection,
   and outputs. Keep processor metadata on the route catalogue rather than copying it into every event.
3. Verify or complete independent `CortexMPostDecoder`, DWT correlation, timestamp, overflow, synchronization,
   quality, and pending-event state per route.
4. Apply each route's timestamp prescaler exactly once in `CortexMStreamDecoder`; OpenCSD, CSV, and CTF do not repeat
   the scaling.
5. Partition overflow summaries and decoder/data-loss diagnostics by route. Preserve a combined overall summary only
   if it cannot hide per-route information.
6. Keep one CSV writer and the existing column set. Preserve synchronous callback order, optional formatted stream
   IDs, payload width, common type/stream filtering, and independent diagnostic reporting.
7. Test interleaved elements on at least two synthetic routes, duplicate source numbers, different prescalers,
   independent errors, filtering semantics, and exact 1/2/4-byte zero payload rendering.

Exit criterion: direct semantic tests prove multi-route behavior while all raw input still uses the unchanged
unformatted frontend.

### Phase 4: Introduce the CTF stream/clock and metadata model

Purpose: replace global CTF assumptions with explicit descriptors without enabling multiple binary writers yet.

1. Replace `coreClockHz` configuration with normalized stream-class and clock-domain descriptors. Keep transport,
   processor, CTF stream-class, and clock-domain identity separate.
2. Implement the bundle-local `CtfMetadataModel` and make `CtfMetadataWriter` a stateless serializer of that model.
   Key ITM/DWT source metadata by route and source, not by source number alone.
3. Move trace UUID ownership to the bundle and pass it explicitly to the existing writer and metadata model. Clock
   UUIDs remain separate identities.
4. Resolve CTF requirements after trace-run/input normalization. Missing, null, invalid, zero, or ambiguous clocks
   disable only CTF with a targeted Error; CSV/check remain viable and `--all` remains non-zero while completing CSV.
5. Preserve the existing one-stream runtime path, `stream_0`, `swo_clock`, UUID optionality, event layouts, and XML
   shape. Exercise multi-stream/multi-clock metadata models directly in unit tests but do not publish multiple stream
   files until Phase 5.
6. Test descriptor identity, boundary/non-contiguous stream-class IDs, source-name collisions, equal-frequency but
   independent domains, shared-domain consistency, and absence of every clock fallback.

Exit criterion: the legacy CTF output is produced through the new model and remains equivalent; the model can
represent the final formatted topology without writer-side global state.

### Phase 5: Emit a complete multi-stream CTF bundle

Purpose: make CTF and Trace Compass consume the route-aware model before formatted decoding is enabled.

1. Replace the single binary writer with an owning map of lazy `CtfStreamWriter` instances indexed by CTF
   stream-class ID. Every writer receives the bundle trace UUID and owns independent packet/timestamp state.
2. Route selected semantic events to their descriptor, create the writer on first emission, and emit exactly one
   stream-local `trace_start` and exception bootstrap before the triggering event. Preserve the eager legacy empty
   unformatted `stream_0` exception.
3. Generate one metadata stream class and referenced clock declaration per emitted descriptor. Do not create
   artifacts for configured formatted routes that produce no selected event.
4. Remove the equal-clock restriction. Distinct processor domains get distinct clock UUIDs even at equal frequency;
   sharing requires an explicitly identical counter/timebase domain.
5. Generate Trace Compass XML only when every emitted stream uses the same one clock declaration, and partition its
   state paths by route/processor. Otherwise keep valid CTF, remove stale XML, and report one Warning.
6. Treat all stream files plus metadata and optional XML as one CTF backend lifecycle. Any CTF start/write/finish
   failure cleans the incomplete bundle while an independent CSV backend may still complete.
7. Drive the encoder directly with interleaved semantic events on at least two routes and test lazy creation,
   non-contiguous IDs, shared trace UUID, independent clocks/state, filters, metadata isolation, XML conditions, and
   failure cleanup.

Exit criterion: multi-route semantic input produces a standards-consistent CTF bundle without requiring a formatted
raw frontend; all legacy CTF/XML tests remain green.

### Phase 6: Move legacy unformatted input onto DecodeTree

Purpose: replace the direct ITM session with the final common frontend while changing only the `SINGLE` path.

1. Add a tree-session wrapper that owns `DecodeTree`, its configured components, callback adapters, and error state.
2. Install the alternate OpenCSD logger for exactly the tree lifetime, restore the previously installed logger on
   every exit path, and enforce at most one live tree session.
3. Create one ITM decoder in `OCSD_TRC_SRC_SINGLE` mode, bind OpenCSD channel `0` to the synthetic route, and attach
   callbacks to both the full decoder and associated packet processor as required by OpenCSD.
4. Preserve chunk feeding, bounded `WAIT`/flush behavior, end-of-trace, current recovery semantics, byte/event counts,
   and output lifecycle.
5. Remove the direct-session production path once all injected-session and legacy SWO tests use the tree wrapper.
6. Test normal and exceptional construction/destruction, foreign logger restoration, overlapping-tree rejection,
   associated-component errors, channel `0`, empty input, bounded progress, and byte-for-byte legacy CSV plus
   equivalent CTF/XML output.

Exit criterion: every supported legacy input uses `DecodeTree(SINGLE)` and no formatted behavior is enabled yet.

### Phase 7: Enable clean formatted CoreSight decoding

Purpose: add the memory-aligned formatted path after the semantic and output layers are already multi-route capable.

1. Construct `OCSD_TRC_SRC_FRAME_FORMATTED` with the internal memory-aligned framing value and one ITM decoder for
   every normalized anchor- or feature-fallback route.
2. Attach a route-bound raw-packet adapter to every ITM decoder and a tree-level unpacked-frame monitor for observed
   formatter IDs.
3. Keep ID `0` silent as NULL/padding. Diagnose each unsupported normal source ID once and skip it without guessing a
   protocol; preserve supported routes and outputs.
4. Preserve the architectural Trace Bus ID through semantic events, CSV filtering/stream values, CTF stream-class
   mapping, diagnostics, and Trace Compass when XML is valid.
5. Enable the reconstructed TB fixture end to end and add focused clean formatted fixtures for single-source,
   boundary-ID, unsupported-ID, and empty-input behavior.
6. Treat any formatted protocol or deformatter error as input-fatal in this phase. Route-local recovery is enabled
   only after Phase 8 proves transaction isolation and deformatter-state preservation.

Exit criterion: clean formatted one- and multi-source captures decode correctly; the CM4/CM7 TB fixture produces one
combined CSV and independent CTF streams, with no misleading multi-clock XML.

### Phase 8: Isolate formatted errors and recover one route

Purpose: make recoverable ITM failures local without corrupting other routes or the formatter state.

1. Preserve every OpenCSD callback in a stable operation-local batch, normalize its optional channel, and make fatal,
   channel-less, or deformatter errors take precedence after the OpenCSD call returns.
2. Replace tree-wide packet transactions and data-loss state with route-aware buffering and recovery intervals.
   Retain unaffected-route events while discarding only unsafe events from the failing route.
3. Resolve and reset only the failing decoder chain. Never reset the complete formatted tree for a route-local
   protocol error.
4. Preserve the deformatter's current ID and partial-frame state. Drain already unpacked segments with bounded flush
   operations before feeding the next aligned raw block, using the root processed-byte count as the only file cursor.
5. Keep only the affected route in data loss until its next hardware synchronization; close an unresolved interval
   explicitly at end-of-trace.
6. Abort every active output on input-wide fatal failure, but preserve the established independent-backend behavior
   for writer failures and publish recoverable semantic diagnostics in otherwise complete output.
7. Test failures from both decoder components, several callbacks per operation, partial-frame interruption,
   continuous IDs without repeated markers, unaffected interleaved routes, failed local reset, deformatter errors,
   unresolved end-of-trace, and the existing `SINGLE` recovery path.

Exit criterion: injected and formatted-fixture faults prove that one route can recover without resetting, losing, or
misattributing another route.

### Phase 9: Complete integration, consumers, and documentation

Purpose: close coverage gaps and validate the complete feature as one product change.

1. Add documented synthetic formatted fixtures for every currently supported ITM-carried trace type not covered by
   the reconstructed hardware fixture, including malformed/recovery intervals and zero-width-sensitive payloads.
2. Run the full CLI matrix for check, CSV, CTF, and `--all`; type/stream filters; absent/null clocks; missing routes;
   unsupported IDs; output failures; and repeated conversions with stale artifacts.
3. Add the pinned Linux Babeltrace consumer test and validate each stream's time scaling independently. Validate the
   supported Trace Compass single-clock case and the deliberate multi-clock no-XML case semantically.
4. Update `architecture.md`, `constraints.md`, fixture provenance, and `todo.md`. Mark `trace-format` and internal
   framing as ctrace-private provisional decisions, link their specification/producer follow-ups, and keep
   FSYNC/HSYNC, explicit file identity, ETM/ETE/PTM/MTB, Event Recorder decoding, and cross-domain correlation visibly
   deferred.
5. Run formatting, all portable tests, the Linux consumer gate, deterministic source-line coverage, the unfiltered
   branch report, and the complete supported-platform CI matrix. Review the total branch diff and acceptance
   criteria before declaring the pull request ready.

Exit criterion: every acceptance criterion below is demonstrated by a test, an external-consumer check, or explicit
documentation, with no temporary Phase 2 or Phase 7 restrictions left in production code.

## Detailed implementation requirements

The subsystem sections below are the detailed contract used by the phases above. If a detail appears to conflict
with the phase sequence, the safer behavior applies and the plan must be corrected before implementation continues.

### Trace-run and discovery (Phases 1-2)

1. Add a normalized raw-input descriptor containing global `trace-format`, internal global framing, channel, path,
   and source routes.
2. Parse and validate provisional root-level `trace-format: unformatted | formatted`. Treat an absent or null value
   as `unformatted` without a diagnostic. Keep framing internal and fixed to the global `memory-aligned` default for
   formatted input; do not parse or emit a `trace-framing` YAML field.
3. In Phase 1, before final raw-input discovery exists, reject an explicit effective `formatted` declaration at the
   job boundary before legacy raw-input selection, decoder construction, or output creation. In Phase 2, retain that
   guard after descriptor discovery and preflight. Remove it only when Phase 7 constructs the formatted tree; never
   use the unformatted frontend as a temporary fallback.
4. Preserve declaration state while parsing, then resolve the effective format before candidate selection: an absent
   or null declaration uses the global unformatted default and legacy SWO-only eligibility; a coexisting legacy TB
   remains an unsupported side input rather than becoming a second candidate. An explicit non-null format makes
   SWO/TB eligible, and an explicit formatted input uses the internal memory-aligned framing default without a
   diagnostic.
5. Resolve exactly one active raw input. Reject zero or multiple matching inputs, then preflight its regular-file
   status, readability, and required byte alignment before decoder and output creation; do not partially convert one
   candidate. Empty unformatted and memory-aligned captures are valid. Preserve the legacy unsupported-channel
   behavior separately: an ineligible
   coexisting TB/ER side input reports a non-failing Warning but does not abort a valid selected SWO conversion.
6. For formatted input, derive exactly one route per unique valid `stream`. Prefer an effective `[<pname>/]itm`
   reference with `type: itm` as the authoritative anchor. If no anchor exists, accept only the supported ITM-carried
   path/type pairs listed in the end-to-end contract as a current-pyTS fallback. Associate other compatible feature
   references without creating more routes. Allow consistent repetitions, but reject conflicting
   `ID -> (protocol, pname)` or bound-`pname + protocol -> ID` bindings and reject a malformed or conflicting present
   anchor rather than falling back around it.
7. Create exactly one synthetic no-ATB-ID ITM route for a `SINGLE` input and require at least one reference-
   authoritative ID in `1..111` for formatted input. Bind processor metadata when uniquely available. Explicit stream
   ID `0` and configured values `112..127` are invalid formatted routes. Tolerate a copied or enriched
   `ctrace-setup.itm.atbid`, but ignore it for routing and consistency decisions.
8. Store and forward diagnostics from every consumed reference type, then independently validate all
   routing-critical fields.
   Missing, malformed, contradictory, or ambiguous required data is fatal even when the reader could retain part of
   the node. Retain structurally valid fields from processor-ITM and timestamp references even when they have no
   `source` and carry a producer `error`; formatted routing still requires valid anchor-or-fallback evidence. Preserve
   each setup containing `disable` as a disabled fragment with its optional `pname`, original ordinal, and
   referenceable feature paths. It contributes no active metadata and never disables another active fragment with the
   same `pname`. Reject a reference as stale-disabled only when its path resolves exclusively to disabled fragments;
   use an active match when present, and merge multiple active matches only when every consumed value is compatible.
   Diagnostics are not defined on `ctrace-setup` nodes by the current schema.
9. Validate the selected formatted file against the internal memory-aligned framing contract: an empty capture is
   valid, and every non-empty capture has a length that is a multiple of 16 bytes. Attach a tree-level unpacked raw-
   frame monitor to collect observed formatter IDs. FSYNC/HSYNC acquisition and partial-sync validation remain out
   of scope until a public framing mode is specified.
10. Keep the current unformatted SWO default for older trace-run files.
11. Replace filename-only Trace Buffer decisions and support multiple simultaneous named inputs once the explicit
    channel/file-association declaration is specified.

### OpenCSD integration (Phases 6-8)

1. Replace the direct `OpenCsdItmSession` construction with a `DecodeTree` session used by both input formats.
2. Preserve the existing chunked feeding, flush, end-of-trace, and bounded-progress behavior.
3. Create one `ITMConfig` and ITM decoder per routed Trace Bus ID for formatted input.
   Keep OpenCSD's ITM prescaler at `1`; ctrace applies the configured per-stream prescaler later and exactly once.
4. Do not create a protocol decoder for formatter ID `0`; consume it only as deformatter NULL/padding data.
5. Attach the generic trace-element callback once to the tree. For every ITM decoder, attach the ctrace error logger
   to both the full decoder component and its associated packet processor; `DecodeTree` does not wire the alternate
   logger to the associated component automatically.
6. Keep OpenCSD error callbacks as a synchronous observation boundary, not a recovery execution context. The callback
   copies severity, error code, raw index, optional OpenCSD channel, and message into a session-owned batch and
   returns without resetting decoders, changing outputs, or throwing. After the active OpenCSD data-path call returns,
   ctrace combines the batch with its response and processed-byte count, then decides whether to continue, flush,
   reset one route, or abort. This prevents reentrant mutation of the tree from inside an OpenCSD callback.
7. Attach a packet-monitor adapter with bound Trace Bus ID to every ITM decoder so that ITM sync, overflow, reserved
   packets, and incomplete tails retain their source identity.
8. Attach a tree-level `ITrcRawFrameIn` monitor and enable `OCSD_DFRMTR_UNPACKED_RAW_OUT` for per-element `traceID`
   observation. This makes formatter IDs without an ITM
   route visible even though no protocol decoder is attached. Handle ID `0` as NULL/padding and formatter-special
   IDs according to the restrictions above; do not interpret other IDs as ITM or guess whether they carry
   ETM/ETE/PTM/MTB. Diagnose each unsupported observed normal source ID once as a non-failing Warning and skip its
   payload without flooding the log. Continue and publish the successfully decoded supported routes; ID `0` remains
   silent NULL/padding rather than an unsupported-source warning.

### CSV output (Phase 3)

1. Keep one CSV writer and preserve the deterministic, synchronous OpenCSD decoder callback order across all streams.
   Do not promise raw-byte or chronological ordering between independent streams.
   Do not write concurrently from per-stream chains into the same CSV file; OpenCSD callbacks are consumed
   synchronously by this single fan-in writer. Separate parallel CSV writers would imply separate output files and
   are outside the specified output contract.
2. Continue writing the architectural Trace Bus ID in `stream`; leave it empty for unformatted internal ID `0`.
3. Apply the per-stream ITM timestamp prescaler before CSV mapping, while retaining cycle values rather than
   converting them to wall-clock units. `CortexMStreamDecoder` performs this scaling exactly once; CSV and CTF consume
   the resulting canonical events and never apply the prescaler again.
4. Keep the specified CSV representation close to the trace stream. Do not enrich rows with `pname`, configured
   clocks, comparator base addresses, or other values that did not arrive in the trace packet.
5. Attribute decoder errors, synchronization, overflow, and data loss to the affected stream without changing the
   existing CSV column set.
6. Preserve the common `TraceSelection` output filter after semantic decoding. Multiple requested types are a union;
   type and stream predicates form an intersection. Do not suppress decoder diagnostics or recovery work merely
   because their semantic events are filtered from CSV output.

### Errors and recovery (Phase 8)

1. Normalize the channel supplied by `ocsdError` into `std::optional<uint8_t>` in `OpenCsdErrorRecord`: preserve
   channel `0` and normal formatted IDs, but map `OCSD_BAD_CS_SRC_ID` (`0xFF`) to absence.
2. Preserve every callback in the current data-path-call batch. Group recoverable errors by normalized route after
   the call returns; warnings remain diagnostics, while any non-recoverable/fatal or unassignable protocol error takes
   precedence and aborts the input. Do not perform reset, rollback, output, or exception propagation in the callback.
3. Associate protocol errors, data-loss intervals, synchronization, and overflow with the affected normalized route
   and include its Trace Bus ID when one exists.
4. Treat a framing/deformatter error as fatal for the complete input. Abort every active output instead of attempting
   to preserve partially routed data.
5. For a recoverable ITM protocol error with a normalized route, reset only that route's OpenCSD packet
   processor/full-decoder chain. Resolve the decoder through `DecodeTree::getDecoderElement(traceBusId)`, obtain its
   public `ITrcDataIn` interface through the decoder manager, and send `OCSD_OP_RESET`. Packet-processor reset
   propagates to its associated full decoder without resetting the frame deformatter or any other route. Use the
   synthetic channel `0` to resolve the one `SINGLE` decoder.
6. Do not reset the complete formatted `DecodeTree` for route-local recovery. A tree reset clears the deformatter's
   current source ID, so resuming at an arbitrary later frame can silently lose a continuous source that does not
   repeat an ID marker. If a protocol error cannot be attributed to one configured route, or route-local reset fails,
   treat it as fatal for the input instead of guessing a recovery point.
7. After route-local reset, retain the deformatter's framing, current-ID, and partially delivered-frame state. The
   root input's returned `numBytesProcessed` is the only raw-file cursor; never re-feed bytes it reports as consumed.
   Before supplying the next aligned raw block, issue bounded `OCSD_OP_FLUSH` calls so the deformatter can finish
   delivering any already unpacked frame segments. Handle further callback batches after each flush by the same
   route-local rules. A successful `CONT` response proves the pending frame is drained; `WAIT` remains bounded, and a
   fatal/channel-less result aborts the input.
8. Only the affected ITM decoder searches for its next hardware synchronization and remains in data loss until it
   finds one; other routes continue without reset or a synthetic discontinuity. If the affected route never
   resynchronizes, explicitly close and emit its interval as unresolved through end-of-trace.
9. Retain valid callbacks before the failing raw-file offset. Route-aware transaction buffering discards callbacks
   from the failing route at or after that offset while preserving unaffected routes; an input-wide fatal error
   discards all uncommitted callbacks and aborts every active output. A writer error aborts its own backend according
   to the existing independent-output contract; recoverable protocol diagnostics remain in completed output as
   today.

### Processor and time domains (Phases 1, 3-5)

The existing trace-run normalization already resolves timestamp settings through
`ctrace-setup.pname -> ctrace-ref.stream`. It retains both `timestamps.clock` and
`timestamps.itm-prescaler` per Trace Bus ID. The stream decoder applies the resolved prescaler to local ITM
timestamps before handing events to the independent per-stream post-decoders. For the TB fixture this maps stream
`1` to CM4 at 240 MHz with prescaler 1, and stream `2` to CM7 at 480 MHz with prescaler 1.

The [CMSIS-Toolbox trace specification][cmsis-trace]
defines `timestamps.clock` as optional and gives it no default; only `timestamps.itm-prescaler` has the default `1`.
Ctrace therefore does not make clock mandatory while reading or decoding. It becomes an operational requirement
only for CTF, whose generated clock declaration needs a real frequency. Missing or invalid clock metadata disables
that backend with an Error instead of inventing a processor frequency.
[CTF 1.8](https://github.com/efficios/ctf/blob/master/common-trace-format-specification.md#8-clocks) itself assumes
1 GHz when a clock's `freq` is omitted, but ctrace must not use that format-level default: its timestamp values are
processor-clock cycles, not nanoseconds, so the resulting time scale would be silently wrong.

Every OpenCSD `ITMConfig` uses prescaler `1`, so OpenCSD exposes raw ITM ticks. Only `CortexMStreamDecoder`, which
knows the resolved stream identity, multiplies each local timestamp once by that stream's `itm-prescaler`. CSV and
CTF consume these canonical scaled events and must not scale them again.

This association requires the formatter ID, or an explicit single-input processor route. An unformatted stream is
reported by OpenCSD as internal ID `0`; it cannot be assigned to one of several processors with different timestamp
settings from the trace bytes alone. Missing prescaler metadata keeps the specified default of `1`; missing CTF clock
metadata has no default. It is accepted by the reader and by validation-only/CSV decoding but prevents CTF generation
for the selected route. Ambiguous or invalid prescaler metadata is fatal.

The current CTF writer has one `CtfStreamWriter` instance and one hard-coded `swo_clock`. It therefore accepts
multiple selected streams only when their resolved `timestamps.clock` values are equal. This is an implementation
restriction, not a CTF 1.8 restriction: one metadata file can declare multiple clocks and stream classes. The
multi-source implementation replaces the single writer with one writer instance per CTF stream descriptor and maps
every CTF stream class to an explicit clock-domain descriptor. Equal frequency does not imply a shared origin.
[CTF 1.8 clocks](https://github.com/efficios/ctf/blob/master/common-trace-format-specification.md#8-clocks)
define non-absolute clocks as synchronized only when they have the same UUID.

1. Preserve the validated source-route-to-processor binding in normalized metadata and resolve it for outputs; do
   not duplicate the processor name in every decoded event.
2. Maintain independent DWT correlation, timestamp quality, and local cycle state per Trace Bus ID.
3. Keep CSV rows in deterministic, synchronous decoder callback order. The existing `stream` column remains the
   Trace Bus ID; no non-standard CSV column is added.
4. Use the processor binding for CTF metadata and Trace Compass labels where available.
5. Emit one CTF stream per emitted CTF stream descriptor and one clock declaration per actual counter/timebase,
   including when processor clocks differ. Streams may share a declaration only when they use the same counter,
   frequency, and origin; mere equal frequency or mathematical correlation is insufficient.
6. Treat clock frequency and cross-stream synchronization separately: frequency converts local cycles to elapsed
   time, while global timestamps or another common reference are required to align independent processor time
   origins exactly.

The current `ctrace-run.yml` supplies frequency but no clock-domain identity, origin, or inter-processor offset.
Therefore this PR treats different processor bindings as independent domains even when their frequencies are equal.
The reconstructed CM4/CM7 fixture consequently produces two independent CTF clocks and no companion Trace Compass
XML. A later producer field may prove that streams use the same counter/timebase. Correlated but distinct counters
remain separate clock declarations and need their own specified offsets/correlation model.

A formatted route may be valid without a matching processor setup. Such a route remains explicitly unbound and gets
its own clock domain and UUID; it never shares a domain merely because another route is also unbound. With no valid
clock value, validation-only and CSV decoding may continue with the default prescaler `1`, but CTF generation reports
an Error and does not start. Processor labels are omitted or use the existing generic source fallback.

### CTF streams and clocks (Phases 4-5)

1. Let the single `CtfBundleOutput`/`CtfEncoder` instance own one bundle-local `CtfMetadataModel`, an owning map of
   `CtfStreamWriter` instances indexed by CTF stream-class ID, and an explicit normalized-route-to-CTF-ID lookup. No
   CTF metadata state is process-global.
2. Populate configured metadata before decoding from normalized CTF stream and clock-domain descriptors: CTF
   stream-class ID, source kind, optional Trace Bus ID, processor name, clock-domain ID/frequency, ITM/DWT sources,
   labels, data types, sizes, and address ranges.
3. Augment the model only from selected semantic events with actually emitted streams and dynamic observations such
   as exception numbers. Mark a stream class emitted on its first selected semantic event; unselected observations
   and configured routes that never occur in the raw trace do not create an empty stream file, metadata declaration,
   clock declaration, or XML lane. The
   per-stream writers do not independently generate or own schema metadata. Preserve the existing eager
   `stream_0`/`trace_start` behavior for an unformatted `SINGLE` capture, including an empty capture; this explicit
   compatibility exception does not create empty files for configured formatted routes.
4. Resolve each decoded event's normalized source route to its CTF stream-class ID, route it to the corresponding
   lazily created writer instance, and write it to `stream_<ctf-stream-class-id>`. When creating a writer, emit the
   same selection-dependent `TRACE_STATUS/trace_start` record and exception-lane bootstrap as today's single writer,
   exactly once and before the event that caused creation. A first overflow, synchronization, or issue-only event
   follows the same rule; a route whose events are all filtered remains artifact-free.
   Apply the same `traceEventSelectedForOutput` predicate as CSV before lazy creation; do not duplicate or weaken the
   type/stream filter in CTF-specific code. Synchronization is the narrow compatibility exception: it has no public
   CSV row or `--type` value, but an otherwise unfiltered CTF selection may encode it as `trace_start`/`resync`
   control context and therefore activate the route. Any explicit type filter keeps a sync-only route artifact-free.
5. Allocate the trace UUID once at bundle level and pass the same UUID to every writer and to the metadata model.
   Every packet header UUID must equal this metadata trace UUID. `CtfStreamWriter::open` must no longer generate an
   independent UUID for each file. Clock UUIDs identify time domains and are never reused as the trace UUID.
6. Allocate a CTF stream-class ID independently from filesystem naming. As a deliberate ctrace mapping for this PR,
   use the Trace Bus ID for formatted sources and CTF ID `0` for an unformatted single input. CTF does not require
   zero-based or contiguous IDs. Packet headers and metadata identify the stream class; filenames are descriptive
   and are not referenced for routing. Reserve `0x80..0xFF` specifically for CMSIS Event Recorder instances and IDs
   beginning at `0x100` for other sources without a unique direct ATB-ID mapping.
7. Extend `CtfOutputConfig` and `CtfEncoderConfig` from one `coreClockHz` value to normalized stream descriptors plus
   clock-domain descriptors. A stream references a domain ID; the domain owns name, optional UUID, frequency,
   `absolute`, and future offsets.
8. Generate one uniquely named CTF clock declaration per actual counter/timebase, for example `cmsis_clock_1` and
   `cmsis_clock_2`, and one clock-mapped timestamp type for each declaration. Without producer evidence of a common
   origin, assign every processor domain a distinct clock UUID and `absolute = false`, even when frequencies match.
   Share one declaration/UUID only when the streams use the same counter, frequency, and origin. Correlated but
   distinct clocks remain distinct declarations; their future `absolute`/offset representation must be specified
   separately. Preserve the existing UUID-optional single-stream `swo_clock` representation for compatibility.
9. Generate one CTF stream declaration per CTF stream descriptor whose event-header and packet-context timestamps
   map to that stream's clock. Generate the supported event declarations for every stream class; event IDs remain
   identical because they are scoped by stream ID.
10. Generate ITM channel and DWT comparator metadata in the scope of their stream so identical source numbers on
    different processors retain independent labels and value metadata. Because CTF `env` is trace-global, include
    the CTF stream-class ID in every generated environment/type symbol that would otherwise collide.
11. Keep `cmsis_trace_bus_id` in CoreSight/ITM event contexts for CMSIS CTF profile and consumer compatibility. Write
    the architectural ID for formatted CoreSight routes and retain `0` as the existing no-ATB-ID sentinel for the
    legacy unformatted ITM route. Event Recorder stream classes omit this field; never write a CTF-local ID such as
    `0x80` into it.
12. Remove the output-planning rejection for selected streams with different valid clocks. Resolve frequency once
    per clock-domain descriptor: one valid value is used by every stream on that domain, while conflicting valid
    values are a CTF requirement failure. Diagnose missing, null, invalid, zero, or conflicting declarations with an
    Error naming the configuration, route, and processor where available. Do not invent a frequency and do not start
    or publish the CTF backend when any selected domain lacks one unambiguous positive clock. With `--all`, keep CSV
    generation active and return non-zero because of the CTF Error. CSV generation and validation-only decoding do
    not depend on this clock.
13. Preserve local timestamp, monotonicity, overflow, exception-lane, and packet sequence state independently in each
   writer/stream state.
14. At successful completion, close every stream writer, validate the completed metadata model, and serialize exactly
    one shared `metadata` file through the stateless `CtfMetadataWriter`. Abort the complete bundle if any stream
    cannot be completed.
15. Generate the companion Trace Compass XML only when all emitted stream classes reference exactly the same single
    CTF clock declaration. The current Trace Compass CTF reader does not correctly scale/sort a trace with multiple
    clock declarations, even if those clocks are otherwise correlatable. For such an otherwise valid bundle, keep
    the CTF output, ensure no stale companion XML remains at the target path, and emit one clear Warning. A future
    per-domain-bundle/experiment output may restore a combined GUI without claiming false cross-domain ordering.
    An empty formatted capture produces a metadata-only CTF bundle with no XML and no multiple-clock Warning.
16. When XML is generated, partition Trace Compass state-system paths by normalized route/processor identity before
    the existing event/source hierarchy.
17. Without a common time reference, describe clocks as independent and do not claim wall-clock or cross-core
    synchronization. Their frequencies still provide correct elapsed time within each stream. When usable global
    timestamp correlation is available, specify and test the appropriate separate-clock offset/absolute
    representation in a later extension. Assign streams to one declaration only when the producer proves the same
    counter/timebase; do not normalize independent clocks to nanoseconds merely to force a global order.

## Compatibility

- Existing single-core SWO captures must produce byte-for-byte identical CSV and equivalent CTF/XML output.
- OpenCSD transport channel `0` remains the internal marker for unformatted input and stays empty in the CSV
  `stream` column; the CTF backend separately assigns this route CTF stream-class ID `0`.
- An explicit source route with `stream: 0` is invalid; ID `0` is introduced only by the unformatted decode path.
- Formatter ID `0` is discarded as NULL/padding and never creates a route, event, CTF stream class/file of its own,
  or Trace Compass lane. Under this PR's direct mapping, `stream_0` remains the unformatted-input artifact; this is a
  ctrace convention, not a CTF rule.
- Formatted sources retain their architectural Trace Bus IDs `1..111` through decoding and output filtering.
- Output type names and current CTF event layouts do not change merely because the input uses a `DecodeTree`.
- A formatted capture containing only one source is valid and must not be forced through the unformatted path.

## Tests

The phase sections define when tests are added. The catalogue below is the final required coverage; ownership is:

| Phase | Primary test responsibility |
| :--- | :--- |
| 0 | Fixture reproducibility, legacy goldens, and coverage gate |
| 1 | YAML reading, null/default behavior, disabled fragments, reference and route normalization |
| 2 | Discovery, input ambiguity, preflight, and no-artifact failure ordering |
| 3 | Per-route semantic state, diagnostics, prescaling, CSV, and filters |
| 4 | CTF descriptors, clock requirements, metadata model, and legacy model compatibility |
| 5 | Multiple CTF writers/clocks, lazy artifacts, Trace Compass policy, and bundle cleanup |
| 6 | DecodeTree `SINGLE`, logger/session lifetime, and legacy end-to-end compatibility |
| 7 | Clean formatted routing, formatter IDs, memory alignment, and TB end-to-end output |
| 8 | Error batching, route-local rollback/reset/resynchronization, and fatal framing errors |
| 9 | Complete fixture/CLI matrix, Babeltrace, Trace Compass, documentation, and all-platform CI |

### Unit tests

- Parse every provisional `trace-format` value. Verify that an absent or null value silently selects the global
  `unformatted` default. Reject unknown and non-scalar non-null values. Verify separately that formatted input uses
  the internal global `memory-aligned` framing value without reading a YAML field. Before Phase 7, verify that an
  explicit formatted declaration, including one beside an SWO-named file, stops before raw frontend, decoder, or
  output construction.
- Resolve zero, one, and multiple discovered raw inputs. Verify that only one succeeds and every failure occurs
  before decoder or output creation.
- Preserve legacy discovery with coexisting SWO and TB files: without explicit metadata, only SWO is active and TB or
  `TB_<name>` is diagnosed with a non-failing unsupported-channel Warning, while SWO output is still completed. With
  explicit metadata, verify SWO-only, TB-only, and one named-TB success; SWO+TB, TB+named-TB, and two-named-TB
  ambiguity failures before output; non-failing ER exclusion from the active count while another eligible input
  completes; and ER-only failure because no eligible input remains.
- Reject missing, non-regular, and unreadable selected raw inputs before output creation. Accept empty unformatted
  and memory-aligned captures.
- Reject non-empty formatted input whose size is not a multiple of the internal 16-byte memory-aligned frame size.
- Accept consistent repetitions of one stream ID across feature references. Reject conflicting route bindings,
  malformed route anchors, stale routes into disabled-only setup fragments, and insufficient routing data. Accept
  IDs `1` and `111`, reject configured values `0`, `112`, and `127`, and prove that an optional
  `ctrace-setup.itm.atbid` is tolerated but neither creates nor changes a route.
- Diagnose and skip observed formatter IDs without a supported ITM route, without guessing their protocol or
  creating semantic output. Verify one non-failing Warning per unsupported normal ID and completed output from
  supported ITM routes; ID `0` remains silent.
- Forward `info`, `warning`, and `error` diagnostics while independently retaining a structurally valid route. Cover
  every supported reference type, including processor-ITM and timestamp bindings without `source`; reject them only
  when ctrace's own normalization fails.
- Preserve disabled setup fragments and their original ordinals. Verify active and disabled fragments with the same
  `pname` and different features, an active and disabled match for the same path, compatible and conflicting multiple
  active matches, a disabled-only stale reference, and unnamed single- and multiple-fragment cases. A disabled
  fragment never suppresses an active one; an absent optional setup remains distinguishable and may use the
  documented defaults. Do not interpret unsupported diagnostic fields on setup nodes.
- Verify a formatted route with an authoritative `[<pname>/]itm`, `type: itm`, `stream` anchor and, separately, the
  current-pyTS fallback for an unnamed single-core data-only setup with no anchor and for a uniquely bound named
  feature. Reject conflicting fallback references, a conflict with a present anchor, and streamless/control-only or
  unsupported refs as route evidence. A legacy `SINGLE` input with empty refs receives one synthetic no-ATB-ID ITM
  route. Accept timestamp refs using normative `type: itm` and transitional `type: dwt` only for the `timestamps`
  leaf, including the constrained fallback, and reject conflicting bindings.
- Construct `DecodeTree` sessions for both `SINGLE` and `FRAME_FORMATTED` input.
- Verify the one synthetic `single` route and reject only ambiguous/conflicting processor metadata; verify that
  equivalent candidates may merge and OpenCSD channel ID `0` resolves to the resulting source metadata.
- Verify both formatted and unformatted captures containing exactly one configured ITM source.
- Preinstall a foreign OpenCSD logger and verify that every normal and exceptional exit restores that exact logger
  after destroying the tree session. Reject an overlapping second live tree, and verify guard release when tree
  construction itself fails.
- Feed formatter ID-0 NULL data and verify that it creates no route, callback, event, CTF stream, or XML lane.
- Verify stream-bound raw callbacks and OpenCSD error-channel normalization for valid `SINGLE` channel `0`, a normal
  formatted ID, and `0xFF` as an absent/deformatter-wide channel.
- Inject errors from both the ITM full decoder and its associated packet processor and verify that the ctrace logger,
  stream attribution, and recovery path receive both.
- Verify that error callbacks only capture stable data. Reset/output actions occur after the OpenCSD call returns;
  multiple callbacks in one call are retained and fatal or channel-less errors take precedence over route recovery.
- Verify independent timestamp, DWT correlation, overflow, and recovery state for multiple IDs.
- Verify one CTF file and stream class per emitted CTF stream descriptor, with no artifact for a configured route that
  produces no selected semantic event. Verify clock declarations only for domains referenced by emitted streams.
- Verify that every lazily created formatted writer emits exactly one selection-dependent `trace_start` and
  exception-lane bootstrap before its first selected record, including overflow/sync/issue-only routes. Preserve the
  eager legacy `stream_0` behavior for an empty unformatted capture.
- Verify that all CTF stream files share the trace UUID and carry their own stream ID, packet sequence, and clock-
  mapped timestamps.
- Use non-contiguous boundary IDs such as `1` and `111`; verify exact decimal filenames, packet `stream_id`, metadata
  `stream { id = ...; }`, and event `stream_id` without any array-index or next-ID assumption.
- Verify CTF generation for distinct processor clocks without applying one processor's frequency to another stream.
  Give independent domains distinct clock UUIDs and prove that equal frequencies do not merge them; verify that
  every packet UUID equals the metadata trace UUID and `trace_uuid != clock_uuid` for every declared clock.
- Verify clock-domain frequency normalization: two streams sharing one domain accept one equal valid value and reject
  missing, null, invalid, zero, or conflicting values for CTF without inventing a frequency or creating CTF output.
  Verify that `--all` still completes CSV and returns non-zero for the CTF Error. CSV-only and validation-only decoding
  remain unaffected.
- Verify that two formatted routes without processor setup stay explicitly unbound and are not merged. They remain
  valid for CSV/check but are rejected for CTF because neither has a clock.
- Preserve the legacy CTF representation explicitly: `stream_0`, exactly one `swo_clock` declaration, unchanged
  clock-UUID optionality/metadata shape, and companion XML present.
- Verify that an empty memory-aligned formatted capture produces metadata only, no stream/clock declaration or XML,
  and no multiple-clock Warning.
- Use different ITM prescalers on two streams and prove that OpenCSD keeps prescaler 1 and both CSV and CTF observe
  exactly one ctrace scaling step.
- Configure the same ITM channel and DWT comparator numbers with different labels/types on two streams and verify
  that CTF metadata remains independent. Verify independent Trace Compass lanes separately when both streams
  reference exactly the same single clock declaration.
- Verify per-stream overflow and decoder-error diagnostics when events from multiple IDs are interleaved.
- Reset only one decoder in a continuous formatted stream whose next frame has no repeated formatter-ID marker.
  Make the failure stop delivery partway through an unpacked formatter frame and prove that bounded post-reset flush
  drains the remaining segments before the next raw block. The deformatter must retain its current ID, another
  interleaved route receives neither reset nor data loss, and only the failing route waits for ITM hardware
  synchronization. Cover `SINGLE` channel-0 recovery, internal memory-aligned framing, multiple recoverable callbacks,
  and a route
  that remains data-loss-active through end-of-trace.
- Verify that channel-less/non-local-resettable protocol errors and frame-deformatter errors abort the input without
  attempting a complete-tree recovery.
- Preserve the independent-output lifecycle tests. Fail CSV and CTF start, write, and completion separately and
  verify that the healthy backend still completes. Within CTF, fail a later stream, metadata completion, and XML
  generation in turn; verify cleanup of the entire incomplete CTF bundle, including stale streams and XML. A
  recoverable decoder error remains publishable with its semantic error/data-loss records.

The following namespace contract tests are deferred with Event Recorder/backend allocation, not implemented in this
PR: `Event Recorder<0> -> 128 -> stream_128`, `Event Recorder<127> -> 255 -> stream_255`, rejection of instance 128,
and allocation of other backend-only sources from 256 upward without using `0x70..0xFF`.

### Integration tests

- Retain the existing real SWO fixtures as compatibility tests.
- Verify that a legacy or explicitly unformatted SWO input produces the unchanged deterministic CSV and a
  `stream_0` CTF file. Verify separately that an explicitly formatted SWO-named input follows its declaration and
  produces only the configured nonzero CTF stream IDs.
- Use the documented reconstructed memory-aligned CoreSight frame capture containing two interleaved real-hardware
  ITM streams.
- Use the reconstructed two-stream fixture for real formatter/PC/exception/DWT-value coverage. Add focused documented
  synthetic formatted fixtures for the supported packet/output types it does not contain: ITM software, DWT address
  and match, PC-sample sleep indications, DWT event counters, PMU, timestamps, synchronization, overflow, and
  decoder-error handling. Across the formatted fixture set, exercise every currently supported trace type on at
  least two normalized ITM routes, including anchor and constrained feature-fallback coverage, so routing is tested
  rather than merely packet decoding.
- Verify that one TB or `TB_<name>` input plus trace-run produces one combined CSV with both stream IDs and one
  decimal-named CTF `stream_<ctf-stream-class-id>` file per formatter ID that emits selected semantic events.
- Verify that the memory-aligned fixture's seven ID-0 padding bytes do not produce `stream_0` or semantic output.
- Exercise a partial memory-aligned formatter frame and verify a deterministic preflight diagnostic and no partial
  output bundle. FSYNC and HSYNC fixtures are deferred until a public framing field is specified.
- Include one malformed ITM interval on one ID and verify route-local recovery and stream attribution while another
  ID continues across the same raw interval without a discontinuity.
- Include an unsupported formatter ID with opaque payload and verify that supported ITM streams continue without
  false decoding or an invented instruction-protocol label.
- Verify CSV stream IDs, CTF records, filters, and validation-only mode. Preserve Trace Compass labels and timing on
  the existing single-stream case. Cover one `--type` followed by multiple values, multiple `--stream` values, their
  union/intersection semantics, and a fully filtered formatted route that creates no CTF artifact. The two-stream
  shared-clock positive case is a direct CTF-output test.
- Verify that CSV preserves the raw payload width for ITM and DWT values on every supported 1-, 2-, and 4-byte packet
  after formatted routing: `0x00`, `0x0000`, and `0x00000000` respectively.
- Read each CTF stream with Babeltrace using a source graph or an isolated metadata-plus-one-stream fixture and
  verify frequency scaling independently (for example, 240 ticks at 240 MHz and 480 ticks at 480 MHz both represent
  1 us). Verify that the default whole-bundle mux rejects distinct clock UUIDs rather than inventing a global order.
  For Trace Compass, test semantic timestamp scaling/order rather than merely successful load: one shared clock
  declaration is the positive case; two declarations, including equal-frequency clocks, must produce no XML and
  exactly one Warning, also replacing a pre-existing stale XML target.
- Maintain 100% ctrace source-line coverage and preserve or improve the reviewed branch report without coverage
  exclusions.

### CI and external consumer gates (Phases 0 and 9)

- Keep the portable internal CTF structure/integration tests in `CtraceIntegTests` on every supported platform.
- Register the Babeltrace consumer check as a separately labelled Linux-only CTest. Add a pinned Babeltrace 2.x
  installation to the Linux test and coverage jobs; absence is a CI failure, not a silent skip.
- Keep generating and archiving line/branch LCOV data. Add a deterministic 100% source-line check to the coverage job;
  Codecov's current relative line threshold and archived branch HTML alone are not that gate. Continue publishing
  the branch report for review without filtering new code or adding exclusions.
- Before PR review, repeat the documented single-clock positive and multiple-clock negative acceptance cases with
  the supported Trace Compass/trace-server version; successful import alone is insufficient.

## Documentation updates (Phases 0 and 9)

- Update `architecture.md` from a direct ITM decoder to the shared `DecodeTree` architecture.
- Record format, framing, routing, reset, and compatibility rules in `constraints.md`.
- Document provenance for every generated formatted-trace fixture.
- Remove the completed Multi-Core, formatted Trace Bus, and processor-routing items from `todo.md`.
- Keep instruction-trace decoding in `todo.md` as independent later work.

## Acceptance criteria

- Every selected trace-run resolves to exactly one active raw input, and all routing-critical metadata is validated
  before decoder or output creation.
- The same `DecodeTree` session abstraction processes unformatted SWO and formatted CoreSight input.
- Every configured ITM Trace Bus ID is decoded independently and mapped to the correct processor when that binding is
  configured; otherwise it remains explicitly unbound and uses only documented defaults.
- All currently supported trace types work for every decoded ITM source.
- Observed formatter IDs without a supported ITM route do not corrupt supported streams, are diagnosed once, and
  are not falsely classified as a particular instruction protocol.
- Errors, sync, overflow, timestamps, filters, and CSV/CTF retain the correct stream identity; Trace Compass does so
  whenever its XML is generated for a supported clock topology.
- OpenCSD error callbacks only capture stable error batches. Recoverable ITM errors reset their known route after the
  data-path call returns; they do not reset the formatter or interrupt unaffected routes.
- A combined CTF bundle supports different processor clocks through explicit stream-to-clock-domain mappings;
  independent domains have distinct clock UUIDs and no claimed global event order.
- Trace Compass XML is generated only when every emitted stream references exactly the same one CTF clock
  declaration; any multiple-clock bundle remains valid CTF and emits exactly one Warning instead of misleading XML.
- A selected clock domain without one unambiguous positive configured frequency emits an Error diagnostic and
  prevents CTF creation; no frequency fallback is used. Validation-only and CSV-only decoding remain valid, and
  `--all` still completes CSV while returning non-zero for the CTF Error.
- Memory-aligned formatted input is covered end to end; FSYNC and FSYNC+HSYNC remain deferred until framing is
  specified publicly.
- Formatter ID-0 padding never becomes a decoded or output stream.
- Fatal input/decode errors abort every active output. A backend-local output error aborts only that backend; a CTF
  failure cleans up the entire incomplete CTF/XML bundle while an otherwise valid CSV may still complete.
- Existing single-SWO behavior remains compatible.
- No ETM, ETE, PTM, or MTB instruction decoding is introduced.
- All portable ctrace tests pass on supported CI platforms, the Linux Babeltrace gate passes, source-line coverage is
  100%, and the unfiltered branch report has been reviewed without adding exclusions.

[armv7-m-arm]: https://documentation-service.arm.com/static/606dc36485368c4c2b1bf62f
[cmsis-trace]: https://github.com/Open-CMSIS-Pack/cmsis-toolbox/blob/main/docs/Experimental-Features.md#timestamps
[coresight-v2]: https://documentation-service.arm.com/static/5f9009d5f86e16515cdc0417
[coresight-v3]: https://documentation-service.arm.com/static/63a03a981d698c4dc521ca77
[ctf-spec]: https://github.com/efficios/ctf/blob/master/common-trace-format-specification.md
