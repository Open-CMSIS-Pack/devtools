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
the canonical SHA-256 and size inventory for checked-in test inputs, reference
outputs and fixture scripts. Markdown documentation is excluded.
`CtraceFixtureIntegrity` checks that the inventory is complete and the
reconstructed TB capture contains 256 frames. Update the manifest in the same
review as a fixture change. Tests work on copies in the build tree; the manifest
guards against unintended changes to the versioned fixtures, not test-time
mutation. Inputs generated at test runtime are defined and checked by the
integration tests, not listed in this manifest.

## Blinky reference outputs

The Blinky fixture is stored under the generic `Blinky+Arm` target name. It was
captured from a CMSIS project with CMSIS-Debugger 1.4.0 and pyTS 0.1.0, as
recorded in the accompanying `ctrace-run` file. It contains SWO and TB input.
The integration test compares the generated SWO CSV byte-for-byte with its
reference and verifies that the coexisting TB input is excluded by the legacy
undeclared-format selection contract.

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
  two opaque ID-42 runs; it proves one warning and no guessed decoder/output
  for an unsupported normal formatter ID.
- `Invalid.TB.raw` is one ID-1 frame containing ITM hardware sync followed by
  reserved header `0x04`; it proves an unresolved route-local loss interval at
  end of input.
- `Recovery.TB.raw` is three frames interleaving IDs 1 and 2. ID 2 contains a
  reserved header, continues into the next frame without a repeated formatter
  ID marker, then resynchronizes; it proves that reset and rollback stay local
  while ID 1 and the deformatter retain state.
- `Unassigned.TB.raw` is one all-zero frame with payload before any formatter
  source ID; it proves that an input-wide deformatter error aborts all outputs.

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

[trace-pc-sample](trace-pc-sample/README.md) is a synthetic PC/sleep/trace-prohibited/PC
sequence with local timestamps. Tests exercise unformatted SWO and two-route
formatted TB input, output modes, filtering and an independent Babeltrace
consumer. The marker is valid status information, not a decoder error.

## Reader and entry-point inputs

`trace-run` contains only the small current-schema inputs needed by executable
tests. Reader unit tests cover only the fields consumed by ctrace. A C++
entry-point test creates a reviewable 13-byte ITM stream below the build tree
and verifies all output formats without an external fixture generator.
