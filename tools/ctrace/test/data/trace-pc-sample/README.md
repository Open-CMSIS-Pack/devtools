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

The YAML declares a 1 MHz clock and prescaler 1. See the
[test-data overview](../README.md#other-decoder-fixtures) for test coverage and
the [integration-test documentation](../../integration/README.md) for Babeltrace
and Trace Compass validation.
