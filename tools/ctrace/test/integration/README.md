# ctrace Integration Tests

Integration tests exercise the `ctrace` executable and file-oriented workflows.
They should use fixtures from `test/data` and write generated output only under
the CMake build directory.

`ctrace-valid-swo-generates-all-outputs` materializes a reviewable eight-byte
ITM stream in the build tree and verifies a successful CSV, CTF, and Trace
Compass conversion. The versioned Blinky capture separately covers damaged
input recovery, exact CSV output, and discovery of unsupported Trace Bus input.
