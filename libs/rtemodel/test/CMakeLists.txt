SET(TEST_SOURCE_FILES src/RteModelTest.cpp src/RteExampleTest.cpp
 src/RteModelTestConfig.h src/RteModelTestConfig.cpp)

add_definitions(-DGLOBAL_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../test")

add_executable(RteModelUnitTests ${TEST_SOURCE_FILES})

set_property(TARGET RteModelUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET RteModelUnitTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(RteModelUnitTests PUBLIC
  ErrLog RteModel RteFsUtils RteUtils XmlReader XmlTree XmlTreeSlim gtest_main)

add_test(NAME RteModelUnitTests
         COMMAND RteModelUnitTests --gtest_output=xml:test_reports/rtemodelunittests-report-${SYSTEM}${ARCH}-${CPU_ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
