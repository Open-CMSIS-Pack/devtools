SET(TEST_SOURCE_FILES TestValidateSyntax.cpp TestPackOptions.cpp TestCheckFiles.cpp)

list(TRANSFORM TEST_SOURCE_FILES PREPEND src/)

add_executable(PackChkUnitTests ${TEST_SOURCE_FILES})

add_definitions(-DTEST_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/../")
add_definitions(-DBUILD_FOLDER="${CMAKE_CURRENT_BINARY_DIR}/")

set_property(TARGET PackChkUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET PackChkUnitTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(PackChkUnitTests PUBLIC packchklib gmock_main)

add_test(NAME PackChkUnitTests
         COMMAND PackChkUnitTests --gtest_output=xml:test_reports/packchkunittests-report-${SYSTEM}-${CPU_ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
