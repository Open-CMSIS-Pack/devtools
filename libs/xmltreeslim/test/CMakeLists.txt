SET(TEST_SOURCE_FILES XmlTreeSlimTest.cpp)

add_executable(XmlTreeSlimUnitTests ${TEST_SOURCE_FILES} ${TEST_HEADER_FILES})

set_property(TARGET XmlTreeSlimUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET XmlTreeSlimUnitTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(XmlTreeSlimUnitTests PUBLIC XmlTreeSlim RteFsUtils RteUtils gtest_main)

add_test(NAME XmlTreeSlimUnitTests
         COMMAND XmlTreeSlimUnitTests --gtest_output=xml:test_reports/xmltreeslimunittests-report-${SYSTEM}-${CPU_ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

