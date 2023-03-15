SET(SOURCE_FILES PackChkIntegTestEnv.cpp PackChkIntegTests.cpp)
SET(HEADER_FILES PackChkIntegTestEnv.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND src/)

add_executable(PackChkIntegTests ${SOURCE_FILES} ${HEADER_FILES})

add_definitions(-DTEST_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/../")
add_definitions(-DBUILD_FOLDER="${CMAKE_BINARY_DIR}/")
add_definitions(-DPROJMGRUNITTESTS_BIN_PATH="$<TARGET_FILE_DIR:PackChkIntegTests>")
add_definitions(-DPACKXSD_FOLDER="${CMAKE_SOURCE_DIR}/external/open-cmsis-pack-spec/schema")

set_property(TARGET PackChkIntegTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set_property(TARGET PackChkIntegTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(PackChkIntegTests PUBLIC RteFsUtils packchklib gtest_main)

add_test(NAME PackChkIntegTests
         COMMAND PackChkIntegTests --gtest_output=xml:test_reports/packchkintegtests-report-${SYSTEM}-${CPU_ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
