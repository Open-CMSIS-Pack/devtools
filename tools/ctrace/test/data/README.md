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
- Derived CSV: `80bf99d42cc83691e24af3a1e2c54d94c434402b2a85ff82d9d1c30d55c214c0`
- Trace-run YAML: `c9816183dde98ded93e57afd44312fb3026e3efdd1681f745bc03f7426713563`

The `Blinky+Arm/expected` directory freezes the legacy SWO CTF and Trace Compass
output. The integration test adds the captured CM7 clock of 480 MHz to its
working copy of the legacy YAML, normalizes platform-dependent generated CRLF
line endings to LF while rejecting bare carriage returns, validates the
generated RFC 4122 UUID, and normalizes only that trace UUID to zero in the
metadata and packet headers before the byte-for-byte comparison.

- Normalized CTF metadata: `5179a4768c9faa9c9ce8ffb0639f373748c1d961195fb46af65c9a67c21739c6`
- Normalized CTF stream: `2054d43163cf1ff8e921be92b397469e2eb75fb55f4f81c08c20382f38918ef6`
- Trace Compass XML: `df52c670767351441135e363d9d8f2e7a953f84b6c77efea49bb0e40a9eb4d40`

The `TB-Trace` fixture is a reconstructed, memory-aligned CoreSight formatter
capture derived from the approved Blinky TB capture. It preserves the usable
real-hardware payload and its formatter interleaving, remaps the source IDs to
the current CM4/CM7 configuration, and adds leading ITM synchronization. The
directory README documents deterministic regeneration and independent
deformatting/countercheck commands. Neither Python tool is used by ctrace at
runtime.

- Reconstructed TB capture: `aab49e56a07783b984fa7c6faeea101a51141423e66ba043dbd8d30702012639`
- Trace-run YAML: `19efd6f35a647f1e5fb73f71ffafa867278d693a7114e3861a43309ebf8f8c4a`
- Reconstruction tool: `8ce6ca54cedc216c04a03587b8388003a8ab0563e6c79c39ebf436d9bfcd0050`
- Analysis helper: `0ce65b99a2c51b2978cf0b790c653f5172715a1fa86521da8088fbd851ef9347`
- Fixture README: `8d4bc39ac1bb656fc72b1545863c6c68a6bcf2c22aa30e1e6a3a37c73c328508`

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
