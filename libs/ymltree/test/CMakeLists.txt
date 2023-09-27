SET(TEST_SOURCE_FILES YmlTreeTest.cpp)

add_executable(YmlTreeUnitTests ${TEST_SOURCE_FILES} ${TEST_HEADER_FILES})

set_property(TARGET YmlTreeUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET YmlTreeUnitTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(YmlTreeUnitTests PUBLIC YmlTree RteFsUtils RteUtils gtest_main)

add_test(NAME YmlTreeUnitTests
         COMMAND YmlTreeUnitTests --gtest_output=xml:test_reports/YmlTreeUnitTests-report-${SYSTEM}-${CPU_ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

