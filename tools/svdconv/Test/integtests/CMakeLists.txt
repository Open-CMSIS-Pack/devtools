SET(SOURCE_FILES SvdConvIntegTestEnv.cpp SvdConvIntegTests.cpp SvdConvTestUtils.cpp SvdConvTestCmdLine.cpp)
SET(HEADER_FILES SvdConvIntegTestEnv.h SvdConvTestUtils.h)

list(TRANSFORM SOURCE_FILES PREPEND src/)
list(TRANSFORM HEADER_FILES PREPEND include/)

add_executable(SvdConvIntegTests ${SOURCE_FILES} ${HEADER_FILES})

add_definitions(-DTEST_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/../")
add_definitions(-DBUILD_FOLDER="${CMAKE_BINARY_DIR}/")

set_property(TARGET SvdConvIntegTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set_property(TARGET SvdConvIntegTests PROPERTY
  VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(SvdConvIntegTests PUBLIC RteFsUtils svdconvlib ErrLog gtest_main)

target_include_directories(SvdConvIntegTests
                            PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
                            PRIVATE ../../SVDConv/include)

add_test(NAME SvdConvIntegTests
         COMMAND SvdConvIntegTests --gtest_output=xml:test_reports/svdconvintegtests-${SYSTEM}-${CPU_ARCH}$<$<BOOL:${COVERAGE}>:_cov>.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
