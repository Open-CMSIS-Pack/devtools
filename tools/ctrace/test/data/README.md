# ctrace Test Data

This directory contains the versioned YAML inputs and raw trace captures used
by the reader and executable-level tests. Generated CSV/CTF outputs stay in the
build tree and are not versioned, except for reference output used by an exact
comparison test.

The Blinky fixture contains SWO and TB input. The integration test compares the
generated SWO CSV byte-for-byte with its reference and verifies that TB is
reported as a trace channel that is not implemented yet.

The Blinky YAML, SWO capture, and TB capture are approved ctrace test assets and
may be redistributed as part of Open-CMSIS-Pack/devtools. The reference CSV is
derived from the SWO capture and is covered by the same approval.

`trace-run` contains only the small current-schema inputs needed by executable
tests. Reader unit tests cover the consumed fields and verify that unrelated
YAML content is ignored. The successful-output integration test combines the
Board YAML with an eight-byte synthetic ITM stream generated below the build
tree; no generated binary fixture is versioned.
