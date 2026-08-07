<!--
Copyright (c) 2026 Arm Limited. All rights reserved.
SPDX-License-Identifier: Apache-2.0
-->

# ctrace Integration Tests

`CtraceIntegTests` calls the `CtraceMain` application entry point linked from the
same object library as the `ctrace` executable. It exercises file-oriented
workflows with fixtures from `test/data` and writes generated output only under
the CMake build directory. Small CTest smoke tests separately cover the platform
executable and Windows manifest.

The complete Blinky fixture covers exact CSV output and discovery of
unsupported Trace Bus input. The versioned Arm target capture covers recovery
across a real MCU-reset trace discontinuity and verifies that DWT decoding
continues after the next hardware ITM sync.
