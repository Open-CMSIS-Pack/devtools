SET(TEST_SOURCE_FILES src/XmlFormatterTest.cpp src/XmlTreeTest.cpp)

add_executable(XmlTreeUnitTests ${TEST_SOURCE_FILES})

set_property(TARGET XmlTreeUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET XmlTreeUnitTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(XmlTreeUnitTests PUBLIC XmlTree gtest_main)

add_test(NAME XmlTreeUnitTests
         COMMAND XmlTreeUnitTests --gtest_output=xml:test_reports/xmltreeunittests-report-${SYSTEM}-${CPU_ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
