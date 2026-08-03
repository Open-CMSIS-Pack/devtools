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

devtools does not apply a downstream source patch. A possible upstream fix is
to use the vector data pointer, which is valid to pass with a zero size:

```cpp
outputRawPacketToMonitor(
    m_packet_index,
    &m_curr_packet,
    m_dump_unsynced_bytes,
    m_packet_data.data());
```

Alternatively, OpenCSD can skip the call when `m_dump_unsynced_bytes` is zero.
An upstream regression test should exercise sync recovery with an input block
ending immediately after an eight-byte unsynchronized flush. Once OpenCSD
contains and releases the fix, devtools can update the pinned submodule
revision without carrying a local modification.
