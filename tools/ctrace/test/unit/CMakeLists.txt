# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

add_executable(CtraceUnitTests
  src/cli/CliParserTests.cpp
  src/control/DecodeConsumersTests.cpp
  src/control/TraceDirectoryJobTests.cpp
  src/decode/CortexMPostDecoderTests.cpp
  src/decode/DecodePipelineTests.cpp
  src/decode/DwtPacketDecoderTests.cpp
  src/decode/OpenCsdErrorControllerTests.cpp
  src/decode/OpenCsdPacketCollectorTests.cpp
  src/diagnostics/DiagnosticsTests.cpp
  src/model/TraceSelectionTests.cpp
  src/output/OutputRequirementsTests.cpp
  src/output/TraceOutputLifecycleTests.cpp
  src/output/csv/CsvFileOutputCriteriaTests.cpp
  src/output/csv/CsvFileOutputTests.cpp
  src/output/csv/CsvRowMapperTests.cpp
  src/output/ctf/CtfBundleOutputTests.cpp
  src/output/ctf/CtfEncoderTests.cpp
  src/output/ctf/CtfSchemaTests.cpp
  src/tracerun/CtraceRunMetaTests.cpp
  src/tracerun/TraceRunConfigReaderTests.cpp
  src/tracerun/TraceRunDiscoveryTests.cpp
)

target_link_libraries(CtraceUnitTests PRIVATE
  ctrace::control
  ctrace::decode
  ctrace::opencsd
  gtest_main
)

target_include_directories(CtraceUnitTests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/support"
)

ctrace_configure_target(CtraceUnitTests)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test_reports")
include(GoogleTest)
gtest_discover_tests(CtraceUnitTests
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  PROPERTIES
    TIMEOUT 30
    LABELS "ctrace-unit"
  XML_OUTPUT_DIR "${CMAKE_BINARY_DIR}/test_reports"
  DISCOVERY_MODE PRE_TEST
)
