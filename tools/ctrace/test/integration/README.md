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

Fixture provenance and the scenarios covered by each capture are documented in
the [test-data README](../data/README.md).
