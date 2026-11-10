<!--
Copyright (c) 2026 Arm Limited. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# Known OpenCSD issues

## Empty ITM unsynchronized-data buffer

The OpenCSD 1.8.3 revision pinned by devtools evaluates
`&m_packet_data[0]` in `TrcPktProcItm::flushUnsyncedBytes()`. The function can
be called with an empty `std::vector`, for example when the sync search flushes
an eight-byte unsynchronized block and then flushes again at the input-block
boundary. Indexing element zero of an empty vector has undefined behavior,
even though `outputRawPacketToMonitor()` returns immediately for a zero-length
packet and does not dereference the resulting pointer.

ctrace reaches this OpenCSD path during the initial hardware-sync search and
when recovering after an invalid ITM packet sequence. Common release builds
may show no failure, but checked standard-library implementations, sanitizers,
or compiler optimizations can expose the undefined behavior as an assertion,
crash, or other incorrect behavior.

The pinned revision still contains this issue. The ctrace build applies
[`OpenCSD.patch`](../cmake/dependencies/OpenCSD.patch) to use the vector data
pointer, which is valid to pass with a zero size:

```cpp
outputRawPacketToMonitor(
    m_packet_index,
    &m_curr_packet,
    m_dump_unsynced_bytes,
    m_packet_data.data());
```

The regression test
`testOpenCsdItmSessionRecoversAfterUnsynchronizedBlockEndsAtFlushBoundary`
exercises eight- and sixteen-byte unsynchronized input blocks followed by
hardware sync and a software packet. With checked standard-library access,
both this test and the malformed formatted-input integration test abort before
the patch and pass afterward.

The patch is applied idempotently without resetting the submodule or discarding
unrelated local changes. Once OpenCSD releases the fix, update the pinned
submodule revision and remove the local patch.

## Complete Thumb-16 instruction rejected after a short memory read

OpenCSD **1.8.3**, pinned at `59f15320637ce6e510604e7bed550d7ad109d15d`,
requests four bytes before decoding an instruction and treats a shorter read as
inaccessible code, even when the returned two bytes form a complete Thumb-16
instruction. This affects the ETMv3 code follower, the ETMv4 instruction-fetch
paths (including Q and shared ETE source-address handling), and PTM.

A synthetic Cortex-M4/ETMv3.5 trace reproduced the defect on 2026-11-09:
`00 bf fd e7` at `0x1000` contains `NOP; B 0x1000`. Six E atoms should produce
`0x1000, 0x1002` three times. The decoder instead emits only `0x1000`, then
`ADDR_NACC(0x1002)`, and still returns success. Adding two unexecuted bytes after
the branch restores all six PCs. Padding images is not a valid remedy: the
branch is already complete, and the image provider must preserve genuine gaps.

The ctrace build applies
[`OpenCSD-thumb-fetch.patch`](../cmake/dependencies/OpenCSD-thumb-fetch.patch)
independently of the ITM patch. Each affected fetch accepts a short read only
when at least two bytes are available, the ISA is Thumb, and the existing
`is_wide_thumb` predicate identifies a complete 16-bit instruction. Partial
Thumb-32, A32 and A64 instructions remain inaccessible. The opcode buffer is
cleared before each read; instruction decoding and its state changes occur only
after this completeness check.

The dependency regressions in
[`OpenCsdInstructionBoundaryTests.cpp`](../test/unit/src/decode/OpenCsdInstructionBoundaryTests.cpp)
exercise full ETMv3, ETMv4 atom, ETMv4 Q and PTM decoders with an exact memory
callback. They cover two- and three-byte Thumb-16 reads, zero/one-byte reads,
all three incomplete Thumb-32 prefix classes, complete Thumb-32 instructions,
and partial/complete A32 and A64 controls. On macOS arm64 Release, eight valid
Thumb-16 cases fail before the patch; all 21 cases pass afterward, together
with all seven ctrace CTest entries. Reconfiguration leaves the patched source
unchanged. Shared ETE source-address paths were source-reviewed, not runtime-qualified.

This dependency correction does not implement ctrace ETM output or qualify ETE
support. Cancellation, packet truncation and the IRQ8 field issue below remain
separate defects.

## ETMv3 canceling exceptions after packed atoms

OpenCSD **1.8.3**, pinned at `59f15320637ce6e510604e7bed550d7ad109d15d`,
can emit the final instruction of a P-header even when the following exception
cancels it. A synthetic Cortex-M4 instruction-only trace reproduced the defect
with a freshly built host adapter on 2026-11-06:

| Atoms before the same canceling SysTick exception | Expected PCs | Observed PCs |
| --- | --- | --- |
| `88`: EE | `0x1000` | `0x1000`, **`0x1002`** |
| `84 66 84`: E, IGNORE, E | `0x1000` | `0x1000` |

Both decoder runs return success without diagnostics. The packet processor
correctly reports `Cancel prev instr` in both cases. [Arm IHI0014Q][etmv3-spec], section 7.3.1,
p.303 permits equivalent P-header encodings; pp.323 and 338 specify cancellation
for interrupted or continued M-profile instructions during instruction-only trace.

The [pending-list guard][etmv3-pending] uses `numPend >= getNumElem()`.
When multiple ranges are queued, `pendLastNElem(1)` therefore retains none.
The [ETMv3 P-header decoder][etmv3-retain] calls this method to protect the last
range, but the later [exception cancellation][etmv3-cancel] can remove only
pending elements. The canceled PC has already been exported.

Durable [evidence and reproduction][cancel-evidence] are in the separate TraceBufferExamples
workspace: README, reproduction script,
text fixtures, packet/element output, results and provenance. No executable or
build directory is archived. The existing CM4 hardware reference contains no
canceling packets, so its successful decode does not validate this case.

No cancellation fix is applied. Correct cancellation and packing-invariant output are
required before qualifying the default ETMv3 instruction output. This host test
covers the OpenCSD full decoder, not the future production ctrace classifier.

## ETMv3 IRQ8 structured packet field

In the same OpenCSD 1.8.3 pin, an M-profile exception with raw code `0x18`
sets `exception.bits.cm_irq_n` to **24**, although the correct IRQ number is 8.
The [parser conversion][etmv3-irq] subtracts `0x10` only when the raw value is
strictly greater than `0x18`. IRQ7 and IRQ9 controls produce their correct values.
The raw exception code remains correct, and the [independent text formatter][etmv3-irq-text]
correctly prints `Exception=IRQ8`; this is a structured-field defect, not a text
formatting error. IHI0014Q Table 7-11, pp.319–320 defines the raw-code mapping.

The minimal packet `95 49 90 01`, following A-Sync and I-Sync, reproduces the
error through `OCSD_C_API_CB_PKT_MON`. Consumers must normalize the raw exception
code using its protocol-specific mapping rather than trust this derived field
until it is corrected and qualified.

## ETMv3 unfinished packet emitted without EOT error status

The same pin's [ETMv3 EOT handler][etmv3-eot] emits pending bytes without changing
the packet type to `ETM3_PKT_INCOMPLETE_EOT`. A trace ending with only `95` after
A-Sync and I-Sync at `0x1000` produces a normal `BRANCH_ADDRESS` packet with
`err_type=NOERROR`; its display retains the previous address `0x1000`.
IHI0014Q Table 7-8, p.314 requires another address byte when bit 7 is set, so
this packet is incomplete. The probe nevertheless returns success.

The captured byte is preserved in the monitor callback; the defect is missing
truncation status and apparently valid stale packet fields, not lost input bytes.
Packet and mixed-fallback output must retain the byte as incomplete and suppress
unverified address fields. The design already identified this qualification risk;
the synthetic case now confirms it without a source patch.

Both packet-field cases, including IRQ7/IRQ9 controls, have [evidence and reproduction][field-evidence]
in the separate TraceBufferExamples workspace, with the C++ probe, reproduction script, fixtures, callback output,
results and provenance. No compiled products are archived.

## ETMv3 cancellation metadata in generic output

This is an API information limitation, separate from the incorrect PC above.
Even when cancellation correctly removes the last range,
[ETMv3 generic EXCEPTION construction][etmv3-exception] preserves the exception
number but omits the cancellation reason. An instruction-output adapter that
promises to explain incomplete paths must retain this information through its
packet monitor, including when all code bytes are available.

A nonzero `Resume` field is not required for the initial instruction-only profile:
IHI0014Q p.323 specifies that exception information byte 2 is not emitted in this
mode. Preserve the actual cancellation indication without inventing complete
execution of a paused instruction.

## ETMv4 reset exception address validity

The pinned [ETMv4 exception decoder][etmv4-exception] sets `excep_ret_addr=1`
for non-tail-chained exception elements, including PE reset. This flag alone is
insufficient to establish an architectural return address: [IHI0064H.a][etmv4-spec],
p.270 defines the address accompanying a PE Reset exception element as UNKNOWN.
The transmitted address still updates protocol address registers; that does not
make it a valid execution or return address.

Consumers must suppress the generic reset return-address claim and retain any
transmitted address only with its protocol role and unknown exception-address
semantics. This is an interface-validity limitation identified from the specification
and decoder source, not a reproduced wrong-PC claim.

[etmv3-pending]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/include/common/ocsd_gen_elem_list.h#L124-L140
[etmv3-retain]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/source/etmv3/trc_pkt_decode_etmv3.cpp#L645-L651
[etmv3-cancel]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/source/etmv3/trc_pkt_decode_etmv3.cpp#L486-L490
[etmv3-exception]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/source/etmv3/trc_pkt_decode_etmv3.cpp#L529-L534
[etmv3-spec]: https://documentation-service.arm.com/static/5f90158b4966cd7c95fd5b5e

[etmv3-irq]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/source/etmv3/trc_pkt_proc_etmv3_impl.cpp#L989-L1013
[etmv3-irq-text]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/source/etmv3/trc_pkt_elem_etmv3.cpp#L669-L674
[etmv3-eot]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/source/etmv3/trc_pkt_proc_etmv3_impl.cpp#L129-L139

[cancel-evidence]: ../../../../TraceBufferExamples/ReferenceTraces/OpenCSD-ETMv3-Cancellation/README.md
[field-evidence]: ../../../../TraceBufferExamples/ReferenceTraces/OpenCSD-ETMv3-PacketFields/README.md

[etmv4-exception]: https://github.com/Linaro/OpenCSD/blob/59f15320637ce6e510604e7bed550d7ad109d15d/decoder/source/etmv4/trc_pkt_decode_etmv4i.cpp#L1690-L1704
[etmv4-spec]: https://documentation-service.arm.com/static/60017fbb3f22832ff1d6872b
