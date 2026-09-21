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

## Decode errors and retained output

The formatted recovery tests verify that a damaged route can resynchronize
without resetting the formatter or another route. Native decoder error names,
descriptions, packet types, and bounded byte previews appear in CLI and CSV
diagnostics. Recoverable errors leave completed CSV/CTF output available while
the command returns a failing exit status.

`RetainsCsvAndUnfilteredAbortAfterIncompleteFormattedTail` covers a fatal
end-of-input decode failure after a valid payload. It requires selected CSV
rows to remain, followed by exactly one input-wide `error` row containing the
processed-byte count and abort reason, with no cycle timestamp, stream, or
source. That final row bypasses type and stream filters; the preceding
route-local error follows those filters. Incomplete CTF and XML output must be
removed. The cases cover unfiltered output, `--type itm`, and `--stream 2` when
the input uses route 1.

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
instead of inventing a global event order. A separate one-route configuration
without a processor name verifies the numeric stream-class fallback label in
the private `ctrace_route` context.

## Trace Compass acceptance

The data-driven XML output was accepted on 2026-09-11 against Trace Compass
Server `0.17.0`, build `202609101143`, and TSP `0.6.0`. The server bundle
manifest identifies source commit
`b626f3c61f8d0dac15451c7663aa36d9bf3db33e`; the relevant installed bundles
were TMF Core `10.2.0`, CTF Core `5.1.0`, TMF CTF Core `5.0.2`, and XML Core
`4.3.2`. This is a dated acceptance record, not a pinned runtime dependency or
general compatibility matrix. The identity query was:

```sh
curl -sS http://127.0.0.1:8080/tsp/api/identifier
```

Every scenario ran in isolation. Its XML configuration, trace, and experiment
were removed before the next scenario, and the server was restarted to clear
the XML analysis registry that persists beyond the REST `DELETE`. Indexing and
analysis requests were repeated until their response status was `COMPLETED`.

### Single-clock DWT match

The generated `trace-match.ctf` and `trace-match.SWO.traceanalysis.xml` from
`ConvertsDwtMatchAcrossCsvAndCtf` were registered through:

```http
POST /tsp/api/config/types/org.eclipse.tracecompass.tmf.core.config.xmlsourcetype/configs
POST /tsp/api/traces
POST /tsp/api/experiments
```

Experiment `55d09123-c1a6-3736-89b4-d38201b70fcb` exposed exactly seven events
from 0 through 10,000 ns. `GET /tsp/api/experiments/<uuid>/outputs` exposed
exactly one generated graphical provider:

```text
arm.cmsis.swo.tg.dwt_match.v1
SWO Trace Analysis: DWT Match
```

The synthetic exception bootstrap remained available in the event table but
did not create an exception view. The following semantic queries returned four
`DWT_MATCH` children and active states beginning at 1,000, 3,000, 6,000, and
10,000 ns:

```http
POST /tsp/api/experiments/<uuid>/outputs/timeGraph/arm.cmsis.swo.tg.dwt_match.v1/tree
{"parameters":{}}

POST /tsp/api/experiments/<uuid>/outputs/timeGraph/arm.cmsis.swo.tg.dwt_match.v1/states
{"parameters":{"requested_timerange":{"start":0,"end":10999,"nbTimes":100},"requested_items":[1,2,3,4]}}
```

### Single-source CM4

The reconstructed TB fixture was copied to a temporary directory and converted
with the release executable:

```sh
ctrace <temporary-trace-dir> --target Blinky+Arm --all --stream 1
```

Its CTF and XML were loaded as experiment
`39f5a594-7467-31fb-a83e-75004f2d0076`. It exposed exactly 244 events from 0
through 8,844,454 ns and only this generated graphical provider:

```text
arm.cmsis.swo.tg.exception.stream1.v1
SWO Trace Analysis: EXCEPTION - CM4
```

The visible name contained the resolved processor name but no numeric trace ID;
ordinary PC samples remained event-table data. Event-table index 3 was the
25,729-tick sample scaled to 107,204 ns on `stream_1`, with context
`[cmsis_trace_bus_id=1, ctrace_route=CM4]`. The exception tree contained
`Thread Mode`, `Exception Return`, and `SysTick`; querying the three
server-assigned child IDs returned exactly 29 states for each lane:

```http
POST /tsp/api/experiments/<uuid>/outputs/timeGraph/arm.cmsis.swo.tg.exception.stream1.v1/states
{"parameters":{"requested_timerange":{"start":0,"end":8844454,"nbTimes":1000},"requested_items":[6,7,8]}}
```

### Multi-clock TB trace

The complete CTF bundle from `ConvertsReconstructedFormattedTraceBusFixture`
was loaded without an XML configuration. Experiment
`788ec3a1-8d07-375f-8fa2-24f30ea41281` exposed 614 events: 244 on
`stream_1`/CM4 and 370 on `stream_2`/CM7. The event contexts were
`[cmsis_trace_bus_id=1, ctrace_route=CM4]` and
`[cmsis_trace_bus_id=2, ctrace_route=CM7]`.

No companion `Blinky+Arm.TB.traceanalysis.xml` existed, the server's XML
configuration list was empty, and its output list contained no
`arm.cmsis.swo.*` provider. Trace Compass therefore imported all event-table
data without constructing an invalid cross-clock graphical timeline.
