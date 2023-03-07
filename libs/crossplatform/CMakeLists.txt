project(CrossPlatform VERSION 1.0.0)

message(STATUS "Building for: " ${CMAKE_SYSTEM_NAME})

add_subdirectory("test")

SET(SOURCE_FILES  src/CrossPlatformUtils.cpp)

list(APPEND SOURCE_FILES src/${CMAKE_SYSTEM_NAME}/Utils.cpp
                         src/${CMAKE_SYSTEM_NAME}/constants.h)

SET(HEADER_FILES    include/CrossPlatform.h
                    include/CrossPlatformUtils.h)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  add_definitions(-DDEFAULT_PACKROOTDEF=\"XDG_CACHE_HOME\")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  add_definitions(-DDEFAULT_PACKROOTDEF=\"XDG_CACHE_HOME\")
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

# Explicitly link with stdc++fs for GCC 8
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND
   CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
  target_link_libraries(CrossPlatform PUBLIC stdc++fs)
endif()

if(UNIX)
  target_include_directories(CrossPlatform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/win)
elseif(WIN32)
  target_link_libraries(CrossPlatform PUBLIC urlmon shlwapi version dbghelp)
endif()
