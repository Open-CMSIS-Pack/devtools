SET(TEST_SOURCE_FILES src/XmlValidatorTest.cpp)

add_definitions(-DTEST_FOLDER="${CMAKE_CURRENT_SOURCE_DIR}/")
add_executable(XmlValidatorUnitTests ${TEST_SOURCE_FILES})

set_property(TARGET XmlValidatorUnitTests PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET XmlValidatorUnitTests PROPERTY
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_definitions(-DPACKXSD_FOLDER="${CMAKE_SOURCE_DIR}/external/open-cmsis-pack-spec/schema")

target_link_libraries(XmlValidatorUnitTests PUBLIC XmlValidator gtest_main)

add_test(NAME XmlValidatorUnitTests
        COMMAND XmlValidatorUnitTests --gtest_output=xml:test_reports/xmlvalidatorunittests-report-${SYSTEM}${ARCH}-${CPU_ARCH}.xml
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR})