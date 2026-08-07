# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

add_executable(CtraceIntegTests
  src/CtraceIntegTests.cpp
)

set_property(TARGET CtraceIntegTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
set_property(TARGET CtraceIntegTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_compile_definitions(CtraceIntegTests PRIVATE
  CTRACE_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/test/data"
  CTRACE_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/testoutput"
)
target_link_libraries(CtraceIntegTests PRIVATE
  ctracelib
  gtest_main
)

# The executable-level smoke tests below exercise the platform trampoline.
add_dependencies(CtraceIntegTests ctrace)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test_reports")
add_test(
  NAME CtraceIntegTests
  COMMAND CtraceIntegTests
    --gtest_output=xml:test_reports/ctraceintegtests-report-${SYSTEM}-${CPU_ARCH}.xml
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)
set_tests_properties(CtraceIntegTests PROPERTIES
  LABELS "integration"
  TIMEOUT 120
)

add_test(
  NAME ctrace-version
  COMMAND ctrace --version
)

set(CTRACE_VERSION_REGEX "${PROJECT_VERSION_FULL}")
string(REPLACE "." "\\." CTRACE_VERSION_REGEX "${CTRACE_VERSION_REGEX}")
string(REPLACE "+" "\\+" CTRACE_VERSION_REGEX "${CTRACE_VERSION_REGEX}")
set_tests_properties(ctrace-version PROPERTIES
  PASS_REGULAR_EXPRESSION "^ctrace ${CTRACE_VERSION_REGEX}[\r\n]+$"
)

add_test(
  NAME ctrace-no-args-shows-help
  COMMAND ctrace
)
set_tests_properties(ctrace-no-args-shows-help PROPERTIES
  PASS_REGULAR_EXPRESSION "ctrace <trace-dir> \\[options\\]"
)

add_test(
  NAME ctrace-help
  COMMAND ctrace --help
)
set_tests_properties(ctrace-help PROPERTIES
  PASS_REGULAR_EXPRESSION "ctrace <trace-dir> \\[options\\]"
)

set_tests_properties(
  ctrace-version
  ctrace-no-args-shows-help
  ctrace-help
  PROPERTIES
    LABELS "integration"
    TIMEOUT 30
)

add_test(
  NAME ctrace-windows-manifest
  COMMAND "${CMAKE_COMMAND}"
    "-DCTRACE_EXECUTABLE=$<TARGET_FILE:ctrace>"
    "-DMANIFEST_FILE=${PROJECT_SOURCE_DIR}/src/ctrace.manifest"
    "-DMT_EXECUTABLE=${CMAKE_MT}"
    "-DEMBEDDED_MANIFEST_FILE=${CMAKE_CURRENT_BINARY_DIR}/ctrace-embedded.manifest"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/src/ValidateWindowsManifest.cmake"
)
set_tests_properties(ctrace-windows-manifest PROPERTIES
  LABELS "integration"
  TIMEOUT 30
)
