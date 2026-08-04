# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

set(CTRACE_TEST_HEADER_FILES
  support/CtfTestSupport.h
  support/OpenCsdSessionTestSupport.h
  support/OpenCsdTestSupport.h
  support/TestPath.h
  support/TestSupport.h
  support/TraceOutputTestSupport.h
  support/TraceRunTestSupport.h
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}"
  PREFIX "Header Files"
  FILES ${CTRACE_TEST_HEADER_FILES}
)

add_executable(CtraceUnitTests
  ${CTRACE_SOURCE_DIR}/src/CtraceMain.cpp
  src/CtraceMainTests.cpp
  src/cli/CliParserTests.cpp
  src/control/DecodeConsumersTests.cpp
  src/control/TraceDirectoryJobTests.cpp
  src/decode/CortexMPostDecoderTests.cpp
  src/decode/DecodePipelineTests.cpp
  src/decode/DwtPacketDecoderTests.cpp
  src/decode/OpenCsdErrorControllerTests.cpp
  src/decode/OpenCsdItmDecoderTests.cpp
  src/decode/OpenCsdPacketCollectorTests.cpp
  src/diagnostics/DiagnosticsTests.cpp
  src/model/TraceSelectionTests.cpp
  src/output/OutputRequirementsTests.cpp
  src/output/TraceOutputLifecycleTests.cpp
  src/output/csv/CsvFileOutputTests.cpp
  src/output/csv/CsvRowMapperTests.cpp
  src/output/ctf/CtfBundleOutputTests.cpp
  src/output/ctf/CtfEncoderTests.cpp
  src/output/ctf/CtfMetadataWriterTests.cpp
  src/output/ctf/CtfSchemaTests.cpp
  src/output/ctf/CtfStreamWriterTests.cpp
  src/tracerun/CtraceRunMetaTests.cpp
  src/tracerun/TraceRunConfigReaderTests.cpp
  src/tracerun/TraceRunDiscoveryTests.cpp
  ${CTRACE_TEST_HEADER_FILES}
)

target_link_libraries(CtraceUnitTests PRIVATE
  ctrace::control
  gtest_main
)

target_include_directories(CtraceUnitTests PRIVATE
  "${CTRACE_SOURCE_DIR}/src"
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
