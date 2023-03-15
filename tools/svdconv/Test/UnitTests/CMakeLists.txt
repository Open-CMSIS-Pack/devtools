set(TEST_SOURCE_FILES SvdUtilsTest.cpp GeneratorTest.cpp)

list(TRANSFORM TEST_SOURCE_FILES PREPEND src/)
list(TRANSFORM TEST_HEADER_FILES PREPEND src/)

source_group("Source Files" FILES ${TEST_SOURCE_FILES})
source_group("Header Files" FILES ${TEST_HEADER_FILES})

add_executable(SVDConvUnitTests ${TEST_SOURCE_FILES} ${TEST_HEADER_FILES})

set_property(TARGET SVDConvUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET SVDConvUnitTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(SVDConvUnitTests PUBLIC SVDModel SVDGenerator gtest_main)

add_test(NAME SVDConvUnitTests
         COMMAND SVDConvUnitTests --gtest_output=xml:test_reports/svdconvunittests-report-${SYSTEM}-${CPU_ARCH}$<$<BOOL:${COVERAGE}>:_cov>.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
