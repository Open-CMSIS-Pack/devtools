# Create the Test target
add_executable(TestProg src/TestProg.cpp)
add_definitions(-DTEST_EXE="$<TARGET_FILE:TestProg>")

if((${CMAKE_SYSTEM_NAME} STREQUAL "Linux") OR (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin"))
  SET(SOURCE_FILES ../src/LinuxDarwin_Retarget.cpp)
else()
  SET(SOURCE_FILES ../src/${CMAKE_SYSTEM_NAME}_Retarget.cpp)
endif()

SET(HEADER_FILES ../include/CrossPlatform.h)
SET(TEST_SOURCE_FILES src/UnitTests.cpp)
SET(TEST_HEADER_FILES)

add_executable(CrossPlatformUnitTests ${TEST_SOURCE_FILES} ${TEST_HEADER_FILES})

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  add_definitions(-DDEFAULT_PACKROOTDEF=\"XDG_CACHE_HOME\")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  add_definitions(-DDEFAULT_PACKROOTDEF=\"HOME\")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  add_definitions(-DDEFAULT_PACKROOTDEF=\"LOCALAPPDATA\")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
  add_definitions(-DDEFAULT_PACKROOTDEF=\"\")
endif()


if(MSVC)
  target_link_options(CrossPlatformUnitTests PUBLIC "$<$<CONFIG:Debug>:/SAFESEH:NO>")
endif(MSVC)

set_property(TARGET CrossPlatformUnitTests PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(CrossPlatformUnitTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include/)

if(UNIX)
  target_include_directories(CrossPlatformUnitTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include/win/)
endif()

add_dependencies(CrossPlatformUnitTests TestProg)

add_definitions(-DTEST_BIN_PATH="$<TARGET_FILE:CrossPlatformUnitTests>")
add_definitions(-DGLOBAL_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../test")

# Explicitly link with stdc++fs for GCC 8
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
  target_link_libraries(CrossPlatformUnitTests PUBLIC stdc++fs)
endif()

target_link_libraries(CrossPlatformUnitTests PUBLIC gtest gtest_main CrossPlatform RteFsUtils)

add_test(NAME CrossPlatformUnitTests
         COMMAND CrossPlatformUnitTests --gtest_output=xml:test_reports/crossplatformunittests-report-${SYSTEM}-${CPU_ARCH}.xml
         WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
