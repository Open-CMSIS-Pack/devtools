# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

set(CTRACE_TEST_HEADER_FILES
  support/CtfTestSupport.h
  support/OpenCsdSessionTestSupport.h
  support/OpenCsdTestSupport.h
  support/TestPath.h
  support/TestPlatform.h
  support/TestSupport.h
  support/TraceOutputTestSupport.h
  support/TraceRunTestSupport.h
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}"
  PREFIX "Header Files"
  FILES ${CTRACE_TEST_HEADER_FILES}
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(CTRACE_TEST_RETARGET_SOURCE_FILE support/retarget/windows/TestPlatform.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CTRACE_TEST_RETARGET_SOURCE_FILE support/retarget/linux/TestPlatform.cpp)
else()
  set(CTRACE_TEST_RETARGET_SOURCE_FILE support/retarget/posix/TestPlatform.cpp)
endif()

add_executable(CtraceUnitTests
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
  ${CTRACE_TEST_RETARGET_SOURCE_FILE}
  ${CTRACE_TEST_HEADER_FILES}
)

target_link_libraries(CtraceUnitTests PRIVATE
  ctracelib
  gtest_main
)

target_include_directories(CtraceUnitTests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/support"
)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test_reports")
add_test(
  NAME CtraceUnitTests
  COMMAND CtraceUnitTests
    --gtest_output=xml:test_reports/ctraceunittests-report-${SYSTEM}-${CPU_ARCH}.xml
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)
