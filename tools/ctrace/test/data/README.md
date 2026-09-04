<!--
Copyright (c) 2026 Arm Limited. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# ctrace Test Data

This directory contains the versioned YAML inputs and raw trace captures used
by the reader and executable-level tests. Generated CSV/CTF outputs stay in the
build tree and are not versioned, except for reference output used by an exact
comparison test.

The Blinky fixture is stored under the generic `Blinky+Arm` target name. It was
captured from a CMSIS project with CMSIS-Debugger 1.4.0 and pyTS 0.1.0, as
recorded in the accompanying `ctrace-run` file. It contains SWO and TB input.
The integration test compares the generated SWO CSV byte-for-byte with its
reference and verifies that TB is reported as a trace channel that is not
implemented yet.

The Blinky YAML, SWO capture, and TB capture are approved ctrace test assets and
may be redistributed as part of Open-CMSIS-Pack/devtools. The reference CSV is
derived from the SWO capture and is covered by the same approval and the
repository-wide Apache-2.0 license terms.

The approved Blinky fixture set is identified by these SHA-256 values:

- SWO capture: `f2de14241242697fa0948f1878850cce81575c404233c5c135aa68fc582dc72c`
- TB capture: `b0fccabe1a326ffe9fadf12d5c3a205d87628985e5e75a99da23c97d7f33d13b`
- Derived CSV: `6138cc60deee8bc16a8a889a6d9156ed76f389c4831afafc5125e4a0d00074cc`
- Trace-run YAML: `c9816183dde98ded93e57afd44312fb3026e3efdd1681f745bc03f7426713563`

The `Arm-reset` fixture is an approved excerpt of an Arm target capture. It
starts at the hardware ITM sync immediately before an MCU-reset discontinuity
that OpenCSD reports as an invalid packet sequence. The integration test
verifies that ctrace discards the damaged interval, finds the next hardware ITM
sync, and continues decoding DWT events. The bounded excerpt keeps Debug tests
portable across CI platforms. The trace-run YAML retains only metadata needed
by the test.

- SWO capture: `8c7ba2b90e42188517c7b793e8b7dd4030fa5455b7a38a2de15d8ca2b47995c9`
- Trace-run YAML: `372e3bf3986fd6860dee5046920cbe129db6fd298c3e22468b3e374c09b8cf52`

The `trace-event` fixture combines two packet-aligned excerpts from an Arm
Cortex-M7 SWO capture. The first excerpt contains mixed architectural DWT
event counters. An explicit overflow and hardware sync separate it from a
second excerpt dominated by `SLEEPCNT`. The integration test verifies CSV
packet preservation and bitwise CTF expansion across the boundary.

- Raw capture excerpt: `97807dad2f69b1274df8960d3459426d1da4a6892d05e7623f3e16f06c5d85c8`
- Trace-run YAML: `a7b924d89854ac85e2751d1297ec78783fa12cb3fa54f5638691dd48d546a34e`

The `trace-match` fixture is completely synthetic. It was generated from the
Armv8-M ITM and DWT packet definitions and was not captured from real hardware.
It contains a hardware synchronization packet followed by one Data Trace Match
packet for each comparator 0 through 3 and local timestamps. The integration
test verifies the generated CSV rows, CTF records, labels, and Trace Compass
timeline configuration.

- Generated raw trace: `5cffb5803675dc02ecd5ed4939a42c660ad7cabd3542b8ca1506230e20d14a50`
- Generated trace-run YAML: `b40c10634b8ba335b14b75f0026758ad84dd68aaf68f0a1bbfd2a5745756c5e8`

`trace-run` contains only the small current-schema inputs needed by executable
tests. Reader unit tests cover only the fields consumed by ctrace. A C++
entry-point test creates a reviewable eight-byte ITM stream below the build tree
and verifies all output formats without an external fixture generator.
