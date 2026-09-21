<!--
Copyright (c) 2026 Arm Limited. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# PC Sampling Marker Fixture

This synthetic Armv8-M ITM/DWT byte stream is not a hardware capture. It tests
the [PC sampling marker contract][markers]: one-byte payload `0x00` means
`CPU Sleeping`; one-byte payload `0xFF` means `Trace prohibited`. Neither
marker carries a PC address or indicates a damaged packet.

[markers]: https://open-cmsis-pack.github.io/cmsis-toolbox/Experimental-Features/#pc-sampling-markers

The complete 24-byte `trace-pc-sample.raw` consists of:

| Bytes | Meaning | Timestamp after local timestamp |
| --- | --- | --- |
| `00 00 00 00 00 80` | ITM hardware synchronization | 0 |
| `17 34 12 00 08 10` | PC `0x08001234`, local increment 1 | 1 |
| `15 00 20` | CPU sleeping, local increment 2 | 3 |
| `15 FF 30` | Trace prohibited, local increment 3 | 6 |
| `17 78 56 00 08 40` | PC `0x08005678`, local increment 4 | 10 |

The YAML declares a 1 MHz clock and prescaler 1. Integration tests use the
bytes directly as an unformatted SWO capture and wrap two copies in
memory-aligned formatter frames under IDs 1 and 2. They exercise check, CSV,
CTF and combined output, and verify type and stream selection. The marker
does not reset timestamps, increment the overflow count or prevent decoding
the final PC sample.

The native Linux consumer test also reads the unformatted capture through
Babeltrace 2.0.5. It checks the legacy `PC_SAMPLE` layout for PC/sleep records,
the additive `PC_SAMPLE_PROHIBITED` event and the subsequent PC sample at
10 microseconds.
