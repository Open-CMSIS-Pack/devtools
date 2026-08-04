<!--
Copyright (c) 2026 Arm Limited. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# ctrace Integration Tests

Integration tests exercise the `ctrace` executable and file-oriented workflows.
They should use fixtures from `test/data` and write generated output only under
the CMake build directory.

The complete Blinky fixture covers exact CSV output and discovery of
unsupported Trace Bus input. The versioned Arm target capture covers recovery
across a real MCU-reset trace discontinuity and verifies that DWT decoding
continues after the next hardware ITM sync.
