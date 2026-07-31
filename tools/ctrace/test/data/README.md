# ctrace Test Data

This directory contains the versioned YAML inputs and raw trace captures used
by the reader and executable-level tests. Generated CSV/CTF outputs stay in the
build tree and are not versioned, except for reference output used by an exact
comparison test.

The Blinky fixture was captured from the `Blinky+STM32H747I-EVAL` CMSIS project
with CMSIS-Debugger 1.4.0 and pyTS 0.1.0, as recorded in the accompanying
`ctrace-run` file. It contains SWO and TB input. The integration test compares
the generated SWO CSV byte-for-byte with its reference and verifies that TB is
reported as a trace channel that is not implemented yet.

The Blinky YAML, SWO capture, and TB capture are approved ctrace test assets and
may be redistributed as part of Open-CMSIS-Pack/devtools. The reference CSV is
derived from the SWO capture and is covered by the same approval. Every
versioned input and golden-output file has a sibling `.license` file containing
machine-readable SPDX copyright and license metadata.

The approved Blinky fixture set is identified by these SHA-256 values:

- SWO capture: `f2de14241242697fa0948f1878850cce81575c404233c5c135aa68fc582dc72c`
- TB capture: `b0fccabe1a326ffe9fadf12d5c3a205d87628985e5e75a99da23c97d7f33d13b`
- Derived CSV: `6138cc60deee8bc16a8a889a6d9156ed76f389c4831afafc5125e4a0d00074cc`
- Trace-run YAML: `deef176a7a924a9c24a126ea460994e86afee3d216e6839da515296758797966`

`trace-run` contains only the small current-schema inputs needed by executable
tests. Reader unit tests cover the consumed fields and verify that unrelated
YAML content is ignored. The successful-output integration test combines the
Board YAML with an eight-byte synthetic ITM stream generated below the build
tree; no generated binary fixture is versioned.
