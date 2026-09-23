<!--
Copyright (c) 2026 Arm Limited. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# Deterministic synthetic formatted fixture

This fixture is completely synthetic. It was not captured from hardware and
does not represent output produced by pyTS, pyOCD, or another capture producer.
It exists to exercise packet families and routing cases that the reconstructed
`TB-Trace` hardware payload does not cover.

The directory stores the canonical `Synthetic.ctrace-run.yml`. Integration
tests generate `Synthetic.TB.raw` below the build tree by calling
`syntheticFormattedCapture()` in `test/integration/src/CtraceIntegTests.cpp`.
That builder creates architectural ITM packets with
`FormattedTraceTestSupport.h` and packs them into 16-byte memory-aligned
CoreSight frames. Neither helper is part of the ctrace runtime.

The deterministic generated capture has these properties:

- size: 128 bytes in eight memory-aligned frames;
- formatter ID selections: three, consisting of the initial ID and two changes;
- Trace Bus ID 1: 58 payload bytes, beginning with ITM hardware sync;
- Trace Bus ID 2: 57 payload bytes, beginning with ITM hardware sync;
- formatter ID 0: two padding bytes and no semantic output;
- generated raw SHA-256:
  `e8a62ad20f048385fde894ed1b869bdfb402feabf8a5e4d88283334a92674847`;
- trace-run SHA-256:
  `a8370d26cd2f75264fc4f48fcdbf5fa2c9e1404c80a61c898dd30de8a44b91c6`.

## Routes

| Trace Bus ID | Processor | Binding evidence | Clock | Prescaler | Raw local increment | Decoded cycles |
| :--- | :--- | :--- | ---: | ---: | ---: | ---: |
| 1 | `anchored` | authoritative `anchored/itm` reference | 240 MHz | 1 | 240 | 240 |
| 2 | `fallback` | constrained current-pyTS `fallback/data#0` fallback | 480 MHz | 4 | 120 | 480 |

Both routes therefore represent a one-microsecond local increment while
proving that prescaling happens exactly once after routing. They use different
PC, address, and global-timestamp values so incorrect cross-route state is
observable.

## Packet coverage

Each route contains the same packet-family sequence with route-specific values:

- hardware synchronization;
- zero-valued ITM software payloads of widths 1, 2, and 4 on ports 1, 2, and 3;
- zero-valued DWT comparator payloads of widths 1, 2, and 4;
- a comparator-3 PC/address pair;
- an Armv8-M comparator-3 Data Trace Match packet;
- a periodic-PC-sample sleep indication;
- a DWT event-counter mask and a PMU trace-on-overflow mask;
- paired GTS1/GTS2 global timestamp packets;
- a local timestamp and an overflow packet.

The resulting output covers ITM, DWT value/address/match, PC sample, DWT event,
PMU event, global timestamp, and trace-status CTF event families. The
reconstructed `TB-Trace` fixture independently covers real-hardware PC,
exception, and DWT-value traffic. The synthetic fixture also drives check,
CSV, CTF, and `--all`; stream/type filter combinations; absent/null clock
handling; route-normalization failures; absent/null format inference for TB and
TB-suffix filenames; explicit formatted SWO naming; independent backend
failures; and repeated-conversion cleanup. Additional generated variants cover
unassigned and routed synchronization prefixes, skipped-byte annotations under
filters, and a never-synchronized route beside a healthy route. They are
described in the [test-data README](../README.md); their bytes differ from the
canonical generated capture and hash above.
