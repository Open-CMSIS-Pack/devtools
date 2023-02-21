add_executable(PackGenUnitTests src/PackGenUnitTests.cpp src/PackGenTestEnv.cpp)

set_property(TARGET PackGenUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_link_libraries(PackGenUnitTests PUBLIC RteFsUtils packgenlib gtest_main)
target_include_directories(PackGenUnitTests PUBLIC ../include ./src)

add_definitions(-DTEST_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/")

add_test(NAME PackGenUnitTests
         COMMAND PackGenUnitTests --gtest_output=xml:test_reports/packgenunittests-report-${SYSTEM}-${CPU_ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
