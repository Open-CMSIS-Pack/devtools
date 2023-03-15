set(TEST_SOURCE_FILES CBuildUnitTestEnv.cpp ModelTests.cpp UtilsTests.cpp
    CLayerTests.cpp BuildSystemGeneratorTests.cpp CbuildModelTests.cpp)
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

add_definitions(-DTEST_BUILD_FOLDER="${CMAKE_CURRENT_BINARY_DIR}/")
add_definitions(-DTEST_SRC_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/../")
add_definitions(-DCMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}/")

target_link_libraries(CbuildUnitTests PUBLIC cbuildgenlib gtest_main)

add_test(NAME CbuildUnitTests
         COMMAND CbuildUnitTests --gtest_output=xml:test_reports/cbuildunittests-report-${SYSTEM}-${CPU_ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
