<!--
Copyright (c) 2026 Arm Limited. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# ctrace multi-core and multi-source decision record

This is the normative completion record for formatted Trace Bus input and
route-aware output. The former step-by-step implementation plan remains in the
topic-branch history; it is not a second set of release criteria.

## Scope and non-goals

One trace run resolves to exactly one raw input and uses one OpenCSD
`DecodeTree`: `SINGLE` for an unformatted byte stream or `FRAME_FORMATTED` for
memory-aligned CoreSight frames. Ctrace supports ITM-carried software, DWT,
PMU, exception, timestamp, synchronization, overflow, and decoder-status
events on each configured route.

ETM, ETE, PTM, MTB, Event Recorder decoding, multiple simultaneously active
inputs, FSYNC/HSYNC framing, and cross-clock correlation are deferred. The
provisional root `trace-format` field is ctrace-private; it neither changes the
CMSIS-Toolbox schema nor asserts producer behaviour.

## Input and routing contract

- `trace-format` accepts `unformatted` or `formatted`. Missing or null is the
  undeclared legacy case: only `<set>.SWO.raw` is active, while TB/ER side
  inputs are reported as unsupported and do not fail the SWO conversion.
- An explicit format makes `<set>.SWO.raw`, `<set>.TB.raw`, and
  `<set>.TB_<name>.raw` eligible. Exactly one existing regular, readable file
  is required before output or decoder construction. ER remains unsupported.
- Formatted input is complete 16-byte memory-aligned CoreSight frames with no
  FSYNC/HSYNC. It is not inferred from filenames, synchronisation bytes, or
  configured source count.
- `ctrace-refs.stream` is the routing authority. Normal IDs are `1..111`; ID
  `0` is formatter padding and creates no route or output. An unformatted
  input has a synthetic route without an architectural Trace Bus ID; OpenCSD
  channel `0` is not an architectural ID.
- Processor name, timestamps, diagnostics, recovery, pending DWT state, and
  output metadata stay route-local. A route-local recoverable error resets
  only that route; a deformatter/channel-less error is input-fatal.

## Output contract

- CSV is one callback-ordered file. Formatted rows carry their architectural
  stream ID; output filtering never suppresses decoder diagnostics.
- CTF has one bundle metadata model and lazy `stream_<id>` files for selected
  formatted routes. Each emitted stream declares an explicit clock domain.
  Independent domains stay distinct even at the same frequency.
- A multi-clock bundle remains valid CTF, removes stale XML, and reports one
  Warning rather than claiming a global event order. CTF failures clean the
  complete CTF/XML bundle without discarding an independently valid CSV.
- The legacy unformatted CTF representation remains `stream_0` with
  `swo_clock` and its established event context. CSV and CTF golden comparison
  remain exact after their documented UUID normalisation.

## Accepted legacy XML refinement

The legacy Trace Compass XML golden deliberately changed. XML is now derived
from the completed emitted data, not from all representable CTF event types:

- DWT values/addresses become XY views only when observed.
- Trace-origin exceptions, DWT matches, DWT/PMU counters, and processor sleep
  become time graphs only when observed. Synthetic exception bootstrap records
  alone do not enable a view, and ordinary PC samples do not imply `Processor State`.
- ITM payloads, ordinary PC samples, and trace-status records remain in the
  CTF event table. They are not fabricated as duration-based state views.

This is an approved legacy UI behaviour change, not a claim of byte-for-byte
historical XML compatibility. The checked-in XML remains the approved current
golden and the integration test compares generated XML to it byte-for-byte
(after line-ending normalisation). The fixture manifest also pins its SHA-256.
Any subsequent XML-shape change needs an explicit decision here, a golden
update, and focused XML tests.

## Fixture and test evidence

- `CtraceFixtureIntegrity` is a portable CTest that verifies the complete
  22-file versioned fixture set, SHA-256 values, file sizes, and the 256
  formatter-frame count of `TB-Trace`.
- Unit and integration tests cover input selection/preflight, route isolation,
  output lifecycle, deterministic synthetic formatted packets, the approved
  reconstructed TB capture, and the legacy SWO CSV/CTF/XML goldens.
- `CtraceBabeltrace2Consumer` is the native Linux external-consumer check. It
  requires Babeltrace `2.0.5` and validates each stream separately.

## External acceptance and remaining gates

The data-driven XML refinement passed semantic acceptance on 2026-09-11 with
Trace Compass Server `0.17.0`, TSP `0.6.0`, and server commit
`b626f3c61f8d0dac15451c7663aa36d9bf3db33e`. The single-clock checks proved
that only observed graphical topics create views, route labels use `pname`
without a numeric ID, and exception lanes retain the correct route prefix. The
two-clock fixture exposed all 614 events and no generated XML provider. Exact
versions, requests, counts, and timestamps are recorded beside the
[integration tests](../test/integration/README.md#trace-compass-acceptance).

Before merge, also run the full supported-platform CI, portable unit and
integration suite, the Linux Babeltrace gate, and the existing source-line
coverage/branch-report review. The required ctrace workflow path filters stay
identical for push and pull request and exclude `external/` and `.gitmodules`.

## Primary evidence

- [Architecture](architecture.md) describes the implemented data flow.
- [Constraints](constraints.md) contains the invariants implementation must
  preserve.
- [Fixture documentation](../test/data/README.md) records provenance and
  fixture identity.
- [Integration documentation](../test/integration/README.md) records external
  consumer requirements and the completed Trace Compass acceptance.

Historical source references and the phased implementation rationale remain
available in the topic-branch history and pull-request discussion.
