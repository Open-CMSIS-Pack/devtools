project(CrossPlatform VERSION 1.0.0)

message(STATUS "Building for: " ${CMAKE_SYSTEM_NAME})

add_subdirectory("test")
if((${CMAKE_SYSTEM_NAME} STREQUAL "Linux") OR (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin"))
  SET(SOURCE_FILES  src/CrossPlatformUtils.cpp
                    src/ProcessRunner.cpp
                    src/StreamReader.cpp
                    src/LinuxDarwin_Process.cpp
                    src/LinuxDarwin_StreamReader.cpp)
ELSE()
  SET(SOURCE_FILES  src/CrossPlatformUtils.cpp
                    src/ProcessRunner.cpp
                    src/StreamReader.cpp
                    src/${CMAKE_SYSTEM_NAME}_Process.cpp
                    src/${CMAKE_SYSTEM_NAME}_StreamReader.cpp)
ENDIF()

list(APPEND SOURCE_FILES src/${CMAKE_SYSTEM_NAME}/Utils.cpp
                         src/${CMAKE_SYSTEM_NAME}/constants.h)

SET(HEADER_FILES    include/CrossPlatform.h
                    include/CrossPlatformUtils.h
                    include/ProcessRunner.h
                    include/StreamReader.h)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  add_definitions(-DDEFAULT_PACKROOTDEF=\"XDG_CACHE_HOME\")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  add_definitions(-DDEFAULT_PACKROOTDEF=\"HOME\")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  add_definitions(-DDEFAULT_PACKROOTDEF=\"LOCALAPPDATA\")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
  add_definitions(-DDEFAULT_PACKROOTDEF=\"\")
endif()

add_library(CrossPlatform STATIC ${SOURCE_FILES} ${HEADER_FILES})

set_property(TARGET CrossPlatform PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_include_directories(CrossPlatform
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ./src/${CMAKE_SYSTEM_NAME})

if(NOT EMSCRIPTEN AND
   ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR
    "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"))
# Add library to support std::filesystem
  target_link_libraries(CrossPlatform PUBLIC stdc++fs)
endif()

if(UNIX)
  target_include_directories(CrossPlatform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/win)
elseif(WIN32)
  target_link_libraries(CrossPlatform PUBLIC Urlmon Shlwapi Version Dbghelp)
endif()
