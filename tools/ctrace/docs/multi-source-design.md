<!--
Copyright (c) 2026 Arm Limited. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# ctrace multi-source design

This record explains the September 2026 migration from a single unformatted ITM input to formatted CoreSight input
with multiple Trace Bus IDs. It preserves the design decisions and migration strategy from the original implementation
plan. Current behavior is defined by the [architecture](architecture.md), [constraints](constraints.md), and
[CTF profile](ctf-format.md); later changes to the original assumptions are summarized below.

## Scope and motivation

The existing SWO path decoded one ITM byte stream directly. Trace Buffer captures added a framing layer that can
interleave several sources, each with its own processor metadata, timestamps, and protocol state. Supporting them
required carrying source identity through decoding, diagnostics, filtering, and output generation.

The migration covered every already supported ITM-carried event family: software instrumentation, DWT data and
matches, exceptions, PC samples and sleep, DWT/PMU counters, timestamps, synchronization, overflow, and decoder issues.
DWT and PMU packets share their processor's ITM route; they do not require additional protocol decoders.

The scope remained one decoder-ready raw file per trace-run. Capture readout, trace-buffer wrap handling, and
chronological linearization belong to the producer. Instruction-trace protocols, Event Recorder decoding, simultaneous
raw inputs, FSYNC/HSYNC framing, and cross-processor clock correlation were outside this migration. Their current
status belongs in the [TODO list](todo.md), not in this historical record.

## Ownership before and after

The common `TraceEvent` boundary already separated protocol decoding from output formats. Some semantic state was
already partitioned by source. The migration made route identity explicit throughout that pipeline and removed the
remaining single-input assumptions:

| Area | Before | After |
| --- | --- | --- |
| Raw frontend | Direct ITM session | One `DecodeTree`, with a single or formatted root |
| Protocol state | One ITM decoder | One ITM decoder per configured route |
| Transactions and recovery | Input-wide buffering and loss interval | Rollback, reset, and loss interval per route |
| Semantic state | Existing per-source post-decoders | Explicit normalized routes for timestamps and DWT pairing |
| CTF output | One writer and shared clock configuration | Bundle metadata plus writers and clocks per route |
| CSV output | One ordered writer | Same writer, retaining formatted source IDs |

Both input formats use the same tree ownership boundary. `SINGLE` connects one ITM decoder to a synthetic route;
`FRAME_FORMATTED` owns the deformatter and dispatches payload to configured ITM decoders. Moving SWO onto the tree
first made the established input path exercise the new session lifecycle before formatted decoding was enabled.
It also avoided maintaining two raw-input architectures with different error and lifetime behavior.

The tree session owns the OpenCSD components. Callback adapters, packet collectors, and error state outlive the tree
that refers to them. OpenCSD's alternate logger and live-tree registry use process-global state, so ctrace permits
only one live tree at a time and restores the previous logger after destroying it. Multi-source decoding remains
synchronous; it does not imply parallel tree execution or concurrent writes to CSV.

The resulting pipeline is:

```text
one raw file + normalized trace-run metadata
                  |
       DecodeTree SINGLE / FRAME_FORMATTED
                  |
        route-bound ITM protocol decoders
                  |
       independent Cortex-M semantic state
                  |
            TraceEvent flow
           /              \
   one combined CSV    one CTF bundle
                      metadata + stream_<id> files
```

## Input and route identity

Input format describes the effective capture bytes. A single source can be formatted, and a sink's name does not
prove how its formatter was configured. The migration introduced the provisional, ctrace-private
`ctrace-run.trace-format` field and an internal 16-byte memory-aligned framing contract. It did not introduce
byte-pattern detection or a public `trace-framing` field. The discovery defaults were subsequently revised, as recorded
below. The format field remains a temporary override, not a field awaiting standardization. The current
[trace proposal](https://github.com/Open-CMSIS-Pack/cmsis-toolbox/pull/699) keeps formatter configuration with trace
communication rather than trace-source setup.

Generated `ctrace-refs.stream` assignments are the routing authority. Copied target setup, including an optional
`itm.atbid`, cannot replace effective generated routing information. Processor-ITM references are the preferred route
anchors; a constrained feature-reference fallback accommodates existing producer output. The full normalization rules
remain in the [routing constraints](constraints.md#routing-invariants).

Four identities have separate purposes:

| Identity | Purpose |
| --- | --- |
| Normalized route | Associates decoder state, semantic events, and configuration |
| Trace Bus ID | Routes payload in formatted CoreSight input |
| Processor binding | Supplies processor-specific metadata and display labels |
| CTF stream-class ID | Identifies records and declarations within an output bundle |

Configured formatted ITM IDs are `1..111`. Formatter ID `0` is NULL/padding, not another decoder route. Unformatted
input carries no architectural ID: its synthetic route uses OpenCSD channel `0`, an empty CSV `stream`, and the
legacy CTF stream-class ID `0`. These representations do not turn it into CoreSight source ID `0`.

An observed formatter ID without a configured ITM route is diagnosed and skipped. Its presence cannot establish
whether its bytes contain ITM or an instruction-trace protocol. This keeps unsupported sources from corrupting
supported routes. The original plan deferred simultaneous raw inputs and anticipated extending route identity with
input identity because different formatter domains can reuse the same Trace Bus ID. The later independent-file
processing described below keeps these domains separate through per-input jobs and outputs.

## Semantic state, output, and clocks

Each route owns its local timestamp, pending events, DWT correlation, overflow state, and timestamp quality.
Processor names, source descriptions, and clock settings stay in normalized route metadata rather than being copied
into every event. The route's ITM prescaler is applied once when raw ticks enter the semantic decoder; outputs consume
the resulting cycle values.

All routes are decoded even when output filters select only some of them. Synchronization, recovery, and CLI
diagnostics therefore remain complete. CSV retains one combined file in synchronous semantic callback order. Pending
events may be released by a later timestamp, so that order does not promise raw-byte order or a common chronology
across independent processors.

CTF uses one bundle-local metadata model, one shared trace UUID, and one writer per emitted formatted route. Each
writer owns its packet sequence and timestamp state. Source labels and DWT metadata are scoped by route, preventing
identical channel or comparator numbers on different processors from overwriting each other. Formatted writers are
created lazily after output selection; the eager legacy `stream_0` remains a compatibility exception.

Using a formatted Trace Bus ID as the CTF stream-class ID is a deliberate ctrace mapping. Neither that mapping nor
the filename `stream_<id>` defines a clock domain. Every stream references an explicit clock descriptor, and
independent domains receive distinct clock UUIDs even at equal frequencies. Frequency scales local cycles; it does
not establish a shared origin. Global Timestamp records alone do not establish cross-route synchronization.

CTF requires a valid configured frequency for each selected route. A missing or invalid clock disables CTF with an
Error while CSV or validation-only decoding can continue. Backend-specific requirements and failures remain
independent. The original implementation treated CTF metadata, stream files, and companion XML as one completion
and cleanup unit. Target-level XML now has an independent lifecycle, as recorded under
[revised decisions](#decisions-revised-after-the-migration).

The supported Trace Compass reader cannot safely combine independent clock declarations. Ctrace therefore emits
views for a bundle only when retained streams use one clock domain. Multi-clock CTF remains valid, but produces a
warning and contributes no XML views. Separate single-clock captures can now share a target XML while retaining
UUID-scoped identities; their clocks are not correlated or rebased.

## Recovery without losing another source

A protocol error can interrupt delivery partway through a formatter frame. Resetting the entire tree would also
clear the deformatter's current source ID. Later frames need not repeat that ID, so a global reset could silently lose
or misattribute otherwise valid payload. Recovery therefore follows the route boundary:

1. Callbacks copy a stable batch of errors and packet observations. They do not reset decoders, write outputs, or
   throw through OpenCSD. Decisions occur after the synchronous data-path operation returns.
2. A recoverable error attributed to a known route discards only that route's transaction suffix at or after the
   failing offset. Earlier events and events from unaffected routes remain in order.
3. Only the affected packet-processor/full-decoder chain is reset. The deformatter, current ID, partially delivered
   frame, and other routes retain their state.
4. Bounded flush operations drain already unpacked frame segments before the next raw block. The root's processed
   byte count is the only raw-input cursor; consumed bytes are never re-fed.
5. The affected route stays in data loss until a real hardware synchronization appears, or closes its unresolved
   interval at end of input. Its semantic state clears incomplete DWT pairing and invalidates timestamp quality.

An unassignable error, deformatter failure, failed local reset, or exhausted progress bound aborts the input. The
migration originally removed every incomplete output after such a failure; the later CSV retention policy is
described below. Backend writer failures continue to affect only their own backend.

## Migration and validation strategy

The implementation was staged so that clean multi-source decoding did not depend on unproven changes in every layer
at once. Each stage retained a buildable implementation and the established single-source output checks.

| Stage | Purpose |
| --- | --- |
| Baseline and input model | Freeze fixtures and goldens; normalize routes; preflight exactly one raw input |
| Semantic and output model | Make route identity explicit; isolate state; add CTF stream and clock descriptors |
| Multi-stream output | Validate independent CTF writers and metadata using direct semantic input |
| Common frontend | Migrate existing unformatted input to `DecodeTree(SINGLE)` |
| Formatted decoding | Enable clean memory-aligned input after consumers support multiple routes |
| Recovery and acceptance | Add route-local faults, then validate the complete CLI and external consumers |

Formatted input was rejected before decoder/output construction until its frontend was ready. Initial formatted
support treated protocol errors as input-fatal; local recovery followed only after transaction isolation and
deformatter-state preservation had dedicated tests.

The validation combined complementary evidence:

- Legacy SWO goldens protected CSV bytes and CTF/XML compatibility during the structural migration.
- The reconstructed [TB fixture](../test/data/TB-Trace/README.md) preserved real interleaved hardware payload but
  documented ID changes and added synchronization. Its independent deformatter checked routing; it was not evidence
  for a producer's capture-export behavior.
- [Synthetic formatted fixtures](../test/data/formatted-synthetic/README.md) covered event families absent from the
  hardware payload, boundary IDs, filtering, malformed packets, and per-route isolation.
- Fault tests exercised multiple callbacks, partial-frame delivery, continuous IDs, failed resets, missing clocks,
  and backend cleanup. Chunk boundaries were varied independently of protocol packets.
- Portable tests, 100% source-line coverage, branch review, and external consumer checks covered different failure modes.
  Babeltrace validated each clock's scaling; Trace Compass acceptance checked timestamps and graphical semantics.

Reproducible commands and supported consumer versions belong to the current
[integration-test documentation](../test/integration/README.md), rather than frozen test counts or execution logs.

## Decisions revised after the migration

The following changes supersede assumptions in the original plan:

- Discovery initially expanded eligibility to SWO, TB, and named-TB while retaining the one-input limit. It now
  processes all matching supported inputs independently and sequentially. Each input is normalized separately:
  absent or null format defaults by channel, with SWO unformatted and TB formatted; an explicit override applies
  to every input in the trace-run. Originally, omission selected only the legacy SWO path.
- Input failures contribute to command failure while remaining inputs and solution sets continue. Each file has
  independent decoder state and outputs, with only one OpenCSD tree live at a time. `--target` still selects a
  solution set, including all its supported inputs.
- CTF bundles now always use `<solution-set>.<channel>.ctf`, also for single-input runs. CSV retains its
  channel-qualified path. Existing `<solution-set>.ctf` bundles are not migrated or removed; the published CTF path
  still needs alignment as recorded in the [CTF profile](ctf-format.md#files-and-common-structure).
- Formatter skips and initial unsynchronized ITM bytes now produce non-failing byte-count Info annotations. They
  carry no invented route or time; a route receiving payload without a committed hardware sync reports an Error.
- A fatal decoder abort retains committed CSV rows with an unfiltered input-wide abort record. Incomplete CTF
  is still removed and excluded from XML. This replaces the original all-output cleanup policy for a healthy CSV writer.
- Trace Compass XML now creates graphical views only for topics observed in completed output, refining the original
  eager legacy XML compatibility rule.
- One `<solution-set>.traceanalysis.xml` now collects eligible, freshly completed CTF bundles for the target. Its
  lifecycle is independent of CSV/CTF, and old per-channel XML files are not migrated or deleted. Each contributing
  bundle has one clock domain; multi-clock bundles are excluded with a warning. XML paths use clock UUID plus public
  Trace Bus ID, not processor labels or private `ctrace_route` context. Clock-derived namespaces prevent view-ID
  collisions across targets. Legacy `swo_clock` also gets a UUID without a binary layout change; independent captures
  are neither synchronized nor rebased.
- CSV uses the published `address` column. The plan's outstanding rename from `offset` is no longer applicable.

These are current contracts, not deferred phases of the migration. Detailed behavior and output-selection exceptions
remain in the linked live documents.

## Source history

The original plan was introduced with the [implementation baseline][baseline] and reconciled in
[commit d2cd3c76][plan]. [Commit 2f0d541d][completion] recorded completion of multi-route support;
[commit 4cc6b054][consolidation] consolidated the runtime documentation. This record distills their design rationale
without reproducing implementation instructions, temporary gates, or validation transcripts.

[baseline]: https://github.com/Open-CMSIS-Pack/devtools/commit/c997790bb76ac6e8a272aafc6b6d22d751760b50
[plan]: https://github.com/Open-CMSIS-Pack/devtools/blob/d2cd3c76e6accb350f211085e0d60b600a74d64b/tools/ctrace/docs/multicore-multisource-plan.md
[completion]: https://github.com/Open-CMSIS-Pack/devtools/commit/2f0d541d8e05e4aaa5910a491828fcd893a50c5c
[consolidation]: https://github.com/Open-CMSIS-Pack/devtools/commit/4cc6b0542ae9cd889882fa04362c34007731641b
