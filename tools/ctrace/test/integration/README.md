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

## Multiple input channels

Inline-generated SWO, TB, and named-TB captures share one trace-run configuration
but carry distinct values and timestamps. Tests verify independent CSV/CTF and
one shared target XML in every output mode, target selection, batch processing, and
type/stream filters. A failed channel must not prevent sibling channels or other
solution sets from completing. Preflight failures preserve existing per-input
artifacts, but those old CTF bundles do not contribute to the new target XML.
Legacy target-only CTF directories and per-channel XML files are not deleted.

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
route-local error follows those filters. Incomplete CTF output must be removed
and excluded from target XML. The cases cover unfiltered output, `--type itm`, and `--stream 2` when
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

A second fixture exercises PC sampling at 1 MHz: PC, sleep marker, trace-prohibited
marker, then PC. Babeltrace must read exactly those four records at 1, 3, 6 and
10 microseconds. This independently checks the unchanged `PC_SAMPLE` layout and
the additive `PC_SAMPLE_PROHIBITED` event, including the record following it.

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

The generated CTF and XML from `ConvertsDwtMatchAcrossCsvAndCtf` were registered through the following endpoints.
Their current output names are `trace-match.SWO.ctf` and `trace-match.traceanalysis.xml`:

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

### PC-sampling markers (2026-09-21)

The release build's [synthetic marker fixture](../data/trace-pc-sample/README.md)
was converted with `--all --type pcsample` and imported into an isolated
instance of the same server version, with its own configuration and workspace.
The event table exposed exactly four records: PC at 1,000 ns, sleep at 3,000 ns,
`PC_SAMPLE_PROHIBITED` at 6,000 ns, and the following PC at 10,000 ns. Both PC
addresses, the empty sleep PC array, sample flags `2`, and overflow count `0`
were preserved.

The `arm.cmsis.swo.tg.processor_state.v1` provider exposed a `Sleep` interval
from 3,000 to 6,000 ns and gaps before and after it. The prohibited marker closed
sleep without creating a running or prohibited-duration state. The existing
legacy XML golden remains unchanged because its capture contains no sleep
indications and therefore emits no processor-state handler.

A marker-only capture produced one `PC_SAMPLE_PROHIBITED` table record, no
companion XML, and no `Processor State` graph. An XML file left from an earlier
capture was removed. This also prevents empty analyses for other point-only
or fully filtered captures: Trace Compass rejects an empty `stateProvider`.

### Shared target XML (2026-09-23)

The release build's `CombinesSwoAndTbViewsWithoutMergingReusedStreamIds` fixture
was accepted in an isolated server instance with one XML and two CTF bundles.
Both default formats (unformatted SWO and formatted TB) and formatted SWO/TB
using the same Trace Bus ID `1` and processor name produced exactly one DWT0
series, with value `42` from 10,000 ns, and one sleep lane from 20,000 ns.
The sources remained separate; no timestamp rebasing was applied.

Separate imports, renamed traces, and multiple registered target XML files
also retained source isolation. An XML view for a different capture returned
no data. The reader retained TSDL quotes in its clock-UUID `hostId`; explicit
quoted-or-unquoted UUID path alternatives were verified against real analysis
data. The existing user server was unchanged and the test server was stopped.
