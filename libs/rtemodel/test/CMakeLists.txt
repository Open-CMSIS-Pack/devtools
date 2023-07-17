SET(TEST_SOURCE_FILES src/RteConditionTest.cpp src/RteItemTest.cpp src/RteModelTest.cpp src/RteExampleTest.cpp
 src/RteModelTestConfig.h src/RteModelTestConfig.cpp src/RteChk.cpp src/RteChkTest.cpp )

 SET(RTE_CHK_HEADER_FILES src/RteChk.h)
 SET(RTE_CHK_SOURCE_FILES src/RteChk.cpp src/RteChkMain.cpp)
 add_executable(RteChk ${RTE_CHK_SOURCE_FILES} ${RTE_CHK_HEADER_FILES})
 target_link_libraries(RteChk PUBLIC
  ErrLog RteModel RteFsUtils RteUtils XmlReader XmlTree XmlTreeSlim)

add_definitions(-DGLOBAL_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../test")

add_executable(RteModelUnitTests ${TEST_SOURCE_FILES})

set_property(TARGET RteModelUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET RteModelUnitTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(RteModelUnitTests PUBLIC
  ErrLog RteModel RteFsUtils RteUtils XmlReader XmlTree XmlTreeSlim gtest_main)

add_test(NAME RteModelUnitTests
         COMMAND RteModelUnitTests --gtest_output=xml:test_reports/rtemodelunittests-report-${SYSTEM}-${CPU_ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
