SET(SOURCE_FILES CPackGetTests.cpp InstallerTests.cpp
  CBuildTestFixture.cpp CBuildAC6Tests.cpp CBuildGCCTests.cpp
  CBuildGenTestFixture.cpp CBuildGenTests.cpp SetupTests.cpp CBuildIntegTestEnv.cpp
  MultiTargetAC6Tests.cpp MultiTargetGCCTests.cpp DebPkgTests.cpp AuxCmdTests.cpp)
SET(HEADER_FILES CBuildIntegTestEnv.h CBuildTestFixture.h CBuildGenTestFixture.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND src/)

add_executable(CbuildIntegTests ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET CbuildIntegTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET CbuildIntegTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_definitions(-DTEST_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/../")
add_definitions(-DCBUILDGEN_BIN="$<TARGET_FILE:cbuildgen>")

add_dependencies(CbuildIntegTests cbuildgen)
target_link_libraries(CbuildIntegTests cbuild cbuildgenlib gtest_main)

add_test(NAME CbuildIntegTests
         COMMAND CbuildIntegTests --gtest_output=xml:cbuildinteg_test_report.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
