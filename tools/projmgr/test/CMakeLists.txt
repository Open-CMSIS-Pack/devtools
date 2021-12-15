add_executable(ProjMgrUnitTests src/ProjMgrUnitTests.cpp src/ProjMgrTestEnv.cpp
  src/ProjMgrWorkerUnitTests.cpp  src/ProjMgrGeneratorUnitTests.cpp)

set_property(TARGET ProjMgrUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_link_libraries(ProjMgrUnitTests PUBLIC projmgrlib gtest_main)
target_include_directories(ProjMgrUnitTests PUBLIC ../include ./src)

add_definitions(-DTEST_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/")
add_definitions(-DCMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}/")

add_test(NAME ProjMgrUnitTests
         COMMAND ProjMgrUnitTests --gtest_output=xml:test_reports/projmgrunittest-${SYSTEM}${ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
