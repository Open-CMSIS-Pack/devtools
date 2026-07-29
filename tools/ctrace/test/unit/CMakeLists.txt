# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

add_executable(CtraceUnitTests src/CtraceMainTest.cpp)

set_property(TARGET CtraceUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET CtraceUnitTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(CtraceUnitTests PRIVATE ctracelib gtest_main)

add_test(NAME CtraceUnitTests
         COMMAND CtraceUnitTests --gtest_output=xml:test_reports/ctraceunittests-report-${SYSTEM}-${CPU_ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
