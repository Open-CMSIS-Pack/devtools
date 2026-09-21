<!--
Copyright (c) 2026 Arm Limited. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# ctrace Test Data

This directory contains the versioned YAML inputs and raw trace captures used
by the reader and executable-level tests. Generated CSV/CTF outputs stay in the
build tree and are not versioned, except for reference output used by an exact
comparison test.

## Fixture integrity

The [fixture manifest](../integration/src/ValidateFixtureIntegrity.cmake) is
the canonical SHA-256 and size inventory for checked-in fixtures, including
fixture-local provenance documents. `CtraceFixtureIntegrity` checks that the
inventory is complete and the reconstructed TB capture contains 256 frames.
Update the manifest in the same review as a fixture change. Inputs generated
at test runtime are defined and checked by the integration tests, not listed
in this manifest.

## Blinky reference outputs

The Blinky fixture is stored under the generic `Blinky+Arm` target name. It was
captured from a CMSIS project with CMSIS-Debugger 1.4.0 and pyTS 0.1.0, as
recorded in the accompanying `ctrace-run` file. It contains SWO and TB input.
The integration test isolates the SWO input and compares its generated CSV
byte-for-byte with the reference. Coexisting SWO and TB candidates are
ambiguous and must be rejected, even without a format declaration.

The Blinky YAML, SWO capture, and TB capture are approved ctrace test assets and
may be redistributed as part of Open-CMSIS-Pack/devtools. The reference CSV is
derived from the SWO capture and is covered by the same approval and the
repository-wide Apache-2.0 license terms.

The `Blinky+Arm/expected` directory freezes the legacy SWO CTF and Trace Compass
output. The integration test adds the captured CM7 clock of 480 MHz to its
working copy of the legacy YAML, normalizes platform-dependent generated CRLF
line endings to LF while rejecting bare carriage returns, validates the
generated RFC 4122 UUID, and normalizes only that trace UUID to zero in the
metadata and packet headers before the byte-for-byte comparison.

## Formatted multi-source inputs

[TB-Trace](TB-Trace/README.md) reconstructs a memory-aligned CoreSight formatter
capture from the approved Blinky hardware payload. Its local README documents
all transformations, the manually added ctrace-private `trace-format` field,
deterministic regeneration, and independent deformatting/counterchecks. Its
Python tools validate formatter-ID and payload counters and are test-only.
The reconstruction removes the original unassigned prefix and adds initial
ITM sync packets; prefix-loss and missing-sync behavior therefore have separate
generated regression inputs rather than relying on that reconstructed capture.

[formatted-synthetic](formatted-synthetic/README.md) complements that payload
with deterministic packet-family coverage on two routes: one authoritative
processor-ITM anchor and one constrained current-pyTS fallback. The integration
test generates its 128-byte raw input; only the YAML and documentation are
checked in. The local README records the exact routes, packet sequence,
generated raw hash, and test matrix.

## Generated negative and recovery inputs

The integration test also creates focused formatted inputs as byte literals in
[CtraceIntegTests.cpp](../integration/src/CtraceIntegTests.cpp). They are
hand-authored from the CoreSight memory-aligned formatter and ITM packet
encodings; they are not hardware captures and make no claim about pyTS or
pyOCD producer output:

- `Partial.TB.raw` is 15 arbitrary bytes and exists only to prove alignment
  preflight before output creation.
- `Mixed.TB.raw` is two frames containing clean ID-1 ITM software packets and
  two opaque ID-42 runs; it proves one compatibility warning, skipped-byte
  Info, and no guessed decoder or semantic payload for an unsupported ID.
- `Invalid.TB.raw` is one ID-1 frame containing ITM hardware sync followed by
  reserved header `0x04`; it proves an unresolved route-local loss interval at
  end of input.
- `Recovery.TB.raw` is three frames interleaving IDs 1 and 2. ID 2 contains a
  reserved header, continues into the next frame without a repeated formatter
  ID marker, then resynchronizes; it proves that reset and rollback stay local
  while ID 1 and the deformatter retain state.
- `Malformed.TB.raw` is a synthetic ID-1 stream with hardware sync, malformed
  ASYNC bytes `00 08`, an intervening packet, then a real sync and valid payload.
  It verifies native CLI/CSV error details, the bounded packet preview, recovery
  without replay, retained output, and a failing exit status.
- `Incomplete.TB.raw` has complete formatter frames but ends route 1 with an
  incomplete DWT packet after valid ITM payload and a local timestamp. It
  verifies fatal end-of-input handling: selected CSV rows remain and one final
  input-wide `error` row bypasses type/stream filters, while the route-local
  error obeys them. CTF/XML output is removed and the command fails.
- `Unassigned.TB.raw` is one all-zero frame with payload before any formatter
  source ID; it proves CLI Info and one CSV `info` row for 15 skipped
  payload bytes, with no invented route, CTF stream, or missing-sync error.
- Variants of `Synthetic.TB.raw` prepend two all-zero or all-`0xff` frames to
  valid two-route input. They verify continued decoding, exactly one unassigned
  prefix Info, and CSV retention even under type and stream filters. The
  unassigned counts are 30 and 1 payload bytes respectively, not 32 raw bytes;
  payload attributed to NULL or reserved IDs is accounted separately.
- Another `Synthetic.TB.raw` variant combines an unassigned prefix, one healthy
  route, and eight bytes without a hardware sync on the other route. It
  requires skipped-byte Info, a separate route-bound end-of-input Error, and
  non-zero exit while preserving healthy output and diagnostic rows.
- A routed-prefix variant places three unsynchronized bytes before ID 1's real
  hardware sync and includes valid ID-2 payload. It verifies route-specific
  skipped-byte Info and continued decoding of both routes without a decoder
  Error.

These generated files exist only in each test's build-tree working directory.
Their canonical representation and expected semantics are the reviewed source
literals and assertions, so there are no separate fixture hashes or generators.

## Other decoder fixtures

The `Arm-reset` fixture is an approved excerpt of an Arm target capture. It
starts at the hardware ITM sync immediately before an MCU-reset discontinuity
that OpenCSD reports as an invalid packet sequence. The integration test
verifies that ctrace discards the damaged interval, finds the next hardware ITM
sync, and continues decoding DWT events. The bounded excerpt keeps Debug tests
portable across CI platforms. The trace-run YAML retains only metadata needed
by the test.

The `trace-event` fixture combines two packet-aligned excerpts from an Arm
Cortex-M7 SWO capture. The first excerpt contains mixed architectural DWT
event counters. An explicit overflow and hardware sync separate it from a
second excerpt dominated by `SLEEPCNT`. The integration test verifies CSV
packet preservation and bitwise CTF expansion across the boundary.

The `trace-match` fixture is completely synthetic. It was generated from the
Armv8-M ITM and DWT packet definitions and was not captured from real hardware.
It contains a hardware synchronization packet followed by one Data Trace Match
packet for each comparator 0 through 3 and local timestamps. The integration
test verifies the generated CSV rows, CTF records, labels, and Trace Compass
timeline configuration.

## Reader and entry-point inputs

`trace-run` contains only the small current-schema inputs needed by executable
tests. Reader unit tests cover only the fields consumed by ctrace. A C++
entry-point test creates a reviewable 13-byte ITM stream below the build tree
and verifies all output formats without an external fixture generator.
