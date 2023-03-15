SET(TEST_SOURCE_FILES XmlReaderTest.cpp)

add_executable(XmlReaderUnitTests ${TEST_SOURCE_FILES} ${TEST_HEADER_FILES})

set_property(TARGET XmlReaderUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET XmlReaderUnitTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(XmlReaderUnitTests PUBLIC XmlReader gtest_main)

add_test(NAME XmlReaderUnitTests
         COMMAND XmlReaderUnitTests --gtest_output=xml:test_reports/xmlreaderunittests-report-${SYSTEM}-${CPU_ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

