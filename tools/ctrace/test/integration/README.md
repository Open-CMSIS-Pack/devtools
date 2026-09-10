<!--
Copyright (c) 2026 Arm Limited. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# ctrace Integration Tests

`CtraceIntegTests` calls the `CtraceMain` application entry point linked from the
same object library as the `ctrace` executable. It exercises file-oriented
workflows with fixtures from `test/data` and writes generated output only under
the CMake build directory. Small CTest smoke tests separately cover the platform
executable and Windows manifest.

Fixture provenance and the scenarios covered by each checked-in capture and
inline-generated formatted input are documented in the
[test-data README](../data/README.md).

## Babeltrace consumer gate

`CtraceBabeltrace2Consumer` is a separately labelled native-Linux CTest. The CI
jobs install Ubuntu package revision `2.0.5-3build2`, and the test itself
requires the consumer to report exactly Babeltrace `2.0.5`. CI invokes the
`linux-consumer` label with `--no-tests=error`, so a missing registration is a
failure rather than a silent skip.

The test generates the two-clock `TB-Trace` CTF bundle and reads each stream in
an isolated metadata-plus-one-stream directory. It verifies that Babeltrace
scales a 25,729-tick stream-1 sample at 240 MHz to `0.000107204` seconds and a
7,603-tick stream-2 sample at 480 MHz to `0.000015839` seconds. It then verifies
that Babeltrace's default whole-bundle mux rejects the two distinct clock UUIDs
instead of inventing a global event order.

## Trace Compass acceptance

The Phase-9 consumer acceptance was run against Trace Compass Server `0.17.0`
using TSP `0.6.0`, server commit
`b626f3c61f8d0dac15451c7663aa36d9bf3db33e`, TMF Core `10.2.0`, CTF `5.0.2`,
and XML Core `4.3.2`. It checked semantic query results rather than only a
successful trace import:

- The single-clock `trace-match` output exposed seven events. Its generated XML
  analysis produced the `DWT_MATCH` time-graph states at 1,000, 3,000, 6,000,
  and 10,000 ns.
- A current generalized single-clock TB conversion filtered to CM4/stream 1
  exposed 244 events from 0 through 8,844,454 ns. Its event table placed the
  25,729-tick PC sample at 107,204 ns with stream context
  `[cmsis_trace_bus_id=1, ctrace_route=CM4]`. The XML exception time graph
  returned 29 states each for Thread Mode, Exception Return, and SysTick; a
  literal-path query returned the same states below `CM4/1/...`, proving the
  two-component route prefix.
- The two-clock `TB-Trace` output exposed 614 events, emitted exactly one
  multi-clock warning, and intentionally had no companion XML, avoiding an
  unsupported combined time graph.
- A two-route equal-frequency variant retained two separate clock domains;
  equal frequency did not imply a common origin. Re-conversion removed a stale
  XML target when the emitted topology was multi-clock.

This is an acceptance record for the stated Trace Compass Server/TSP stack.
`cmsis-trace-server` was not part of this acceptance and is not represented as
an acceptance consumer.
