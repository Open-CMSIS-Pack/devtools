# 11.02.2021 : verify merge of feature/cmsis-rte to master
cmake_minimum_required(VERSION 3.14)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

cmake_policy(SET CMP0091 NEW)

option(COVERAGE "Enable code coverage" OFF)
option(LIBS_ONLY "Build only libraries" OFF)

if(LIBS_ONLY)
  message("LIBS_ONLY is active. Build only libraries")
endif(LIBS_ONLY)

# project must be below above statements because of setting CMAKE_TOOLCHAIN_FILE variable
project(devtools)

enable_testing()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(MSVC)
#disable C4996 warning: of not using *_s functions
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
 set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /ZI /Gy /FC")
# disable optimization for RelWithDebug
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Zi /Od")
  add_link_options("$<$<CONFIG:Debug>:/SAFESEH:NO>")
endif(MSVC)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR
   "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR
   "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -MD -Wall")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")
endif()


if(COVERAGE)
  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} --coverage")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} --coverage")
  endif()
endif(COVERAGE)

enable_testing()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH "64")
endif()

# binary output folder
if ("${CMAKE_GENERATOR}" MATCHES "Visual Studio")
  set(OUTDIR "windows${ARCH}")
else()
  string(TOLOWER ${CMAKE_SYSTEM_NAME} SYSTEM)
  set(OUTDIR "${SYSTEM}${ARCH}/${CMAKE_BUILD_TYPE}")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTDIR}")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Libraries
add_subdirectory(libs/crossplatform)
add_subdirectory(libs/errlog)
add_subdirectory(libs/rtefsutils)
add_subdirectory(libs/rtemodel)
add_subdirectory(libs/rteutils)
add_subdirectory(libs/xmlreader)
add_subdirectory(libs/xmltree)
add_subdirectory(libs/xmltreeslim)

# Tools
if(NOT LIBS_ONLY)
  add_subdirectory(tools/buildmgr)
  add_subdirectory(tools/packgen)
endif()

# Google Test Framework
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(external/googletest)

set_property(TARGET gtest PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET gtest_main PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET gmock PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(TARGET gmock_main PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# YAML
add_subdirectory(external/yaml-cpp)

# cxxopts
add_subdirectory(external/cxxopts)
