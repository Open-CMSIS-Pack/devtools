SET(TEST_SOURCE_FILES YmlSchemaChkTestEnv.cpp
  YmlSchemaChkTestEnv.h YmlSchemaChkTests.cpp)

list(TRANSFORM TEST_SOURCE_FILES PREPEND src/)

add_definitions(-DTEST_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/")
add_executable(YmlSchemaChkTests ${TEST_SOURCE_FILES})

set_property(TARGET YmlSchemaChkTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET YmlSchemaChkTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(YmlSchemaChkTests
  PUBLIC YmlSchemaChecker YmlTree RteFsUtils gtest_main)

add_test(NAME YmlSchemaChkTests
         COMMAND YmlSchemaChkTests --gtest_output=xml:test_reports/ymlschemachktests-report-${SYSTEM}-${CPU_ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
