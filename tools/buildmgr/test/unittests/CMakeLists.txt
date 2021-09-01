cmake_minimum_required(VERSION 3.14)

set(TEST_SOURCE_FILES CBuildUnitTestEnv.cpp ModelTests.cpp UtilsTests.cpp
    CLayerTests.cpp BuildSystemGeneratorTests.cpp)
set(TEST_HEADER_FILES CBuildUnitTestEnv.h)

list(TRANSFORM TEST_SOURCE_FILES PREPEND src/)
list(TRANSFORM TEST_HEADER_FILES PREPEND src/)

source_group("Source Files" FILES ${TEST_SOURCE_FILES})
source_group("Header Files" FILES ${TEST_HEADER_FILES})

add_executable(CbuildUnitTests ${TEST_SOURCE_FILES} ${TEST_HEADER_FILES})

target_include_directories(CbuildUnitTests PUBLIC ../../cbuild/include ../../cbuildgen/include)

set_property(TARGET CbuildUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET CbuildUnitTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

file(RELATIVE_PATH TEST_FOLDER ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_definitions(-DTEST_FOLDER="${TEST_FOLDER}/")

target_link_libraries(CbuildUnitTests PUBLIC cbuildgenlib gtest_main)

add_test(NAME CbuildUnitTests
         COMMAND CbuildUnitTests --gtest_output=xml:cbuildunit_test_report.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

