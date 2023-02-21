SET(TEST_SOURCE_FILES src/RteFsUtilsTest.cpp)

add_executable(RteFsUtilsUnitTests ${TEST_SOURCE_FILES} ${TEST_HEADER_FILES})

set_property(TARGET RteFsUtilsUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET RteFsUtilsUnitTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(RteFsUtilsUnitTests PUBLIC RteFsUtils RteUtils gtest_main)

add_test(NAME RteFsUtilsUnitTests
         COMMAND RteFsUtilsUnitTests --gtest_output=xml:test_reports/rtefsutilsunittests-report-${SYSTEM}-${CPU_ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

