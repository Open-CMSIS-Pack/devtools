SET(TEST_SOURCE_FILES src/ErrLogTest.cpp)

add_executable(ErrLogUnitTests ${TEST_SOURCE_FILES})

set_property(TARGET ErrLogUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET ErrLogUnitTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(ErrLogUnitTests PUBLIC ErrLog RteFsUtils RteUtils gtest_main)

add_test(NAME ErrLogUnitTests
         COMMAND ErrLogUnitTests --gtest_output=xml:errlog_test_report.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

