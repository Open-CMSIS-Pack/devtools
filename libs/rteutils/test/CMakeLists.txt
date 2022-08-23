SET(TEST_SOURCE_FILES src/RteUtilsTest.cpp src/VersionCmpTests.cpp)

add_executable(RteUtilsUnitTests ${TEST_SOURCE_FILES})

set_property(TARGET RteUtilsUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET RteUtilsUnitTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(RteUtilsUnitTests PUBLIC RteUtils gtest_main)

add_test(NAME RteUtilsUnitTests
         COMMAND RteUtilsUnitTests --gtest_output=xml:rteutils_test_report.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

