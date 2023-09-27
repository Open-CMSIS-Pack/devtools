cmake_minimum_required(VERSION 3.22)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GetCMakeTargets)
include(ApplyPatch)

cmake_policy(SET CMP0091 NEW)

option(COVERAGE "Enable code coverage" OFF)
option(LIBS_ONLY "Build only libraries" OFF)
option(SWIG_LIBS "Build SWIG libraries" OFF)

if(LIBS_ONLY)
  message("LIBS_ONLY is active. Build only libraries")
endif(LIBS_ONLY)

# Minimum OSX version 11.7 Big Sur
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.7")

# project must be below above statements because of setting CMAKE_TOOLCHAIN_FILE variable
project(devtools)

enable_testing()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(MSVC)
# disable C4996 warning: of not using *_s functions
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  if("${CMAKE_GENERATOR_PLATFORM}" STREQUAL "x64")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /ZI")
  endif()
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Gy /FC /utf-8")
  message("CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /utf-8")
# disable optimization for RelWithDebug
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Zi /Od /utf-8")
  add_link_options("$<$<CONFIG:Debug>:/SAFESEH:NO>")
  set(CMAKE_CXX_FLAGS_DEFAULT "/w /utf-8")
endif(MSVC)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR
   "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR
   "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -MD -Wall")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall")
  set(CMAKE_CXX_FLAGS_DEFAULT "-w")
endif()

if(SWIG_LIBS)
  message("SWIG_LIBS is active. Build SWIG libraries")
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif(SWIG_LIBS)

if(COVERAGE)
  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} --coverage")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} --coverage")
  endif()
endif(COVERAGE)

enable_testing()

string(TOLOWER ${CMAKE_SYSTEM_NAME} SYSTEM)
if(NOT "${CMAKE_GENERATOR_PLATFORM}" STREQUAL "")
  string(TOLOWER ${CMAKE_GENERATOR_PLATFORM} CPU_ARCH)
elseif(NOT "${CMAKE_OSX_ARCHITECTURES}" STREQUAL "")
  string(TOLOWER ${CMAKE_OSX_ARCHITECTURES} CPU_ARCH)
elseif(NOT "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" STREQUAL "")
  string(TOLOWER ${CMAKE_CXX_COMPILER_ARCHITECTURE_ID} CPU_ARCH)
else()
  string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} CPU_ARCH)
endif()

if("${CPU_ARCH}" STREQUAL "x64" OR
   "${CPU_ARCH}" STREQUAL "x86_64")
  set(CPU_ARCH "amd64")
elseif("${CPU_ARCH}" STREQUAL "win32")
  set(CPU_ARCH "x86")
endif()

message("-- Building for architecture: ${CPU_ARCH}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${SYSTEM}-${CPU_ARCH}")
get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT GENERATOR_IS_MULTI_CONFIG)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_BUILD_TYPE}")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    set(CMAKE_TAR_FLAGS "")
    set(CMAKE_TAR_FORMAT "zip")
    set(CMAKE_TAR_EXTENSION "zip")
else()
    set(CMAKE_TAR_FLAGS "j")
    set(CMAKE_TAR_FORMAT "paxr")
    set(CMAKE_TAR_EXTENSION "tbz2")
endif()

include(CheckCXXSymbolExists)
check_cxx_symbol_exists(std::filesystem::path::preferred_separator filesystem HAS_STD_FILESYSTEM)
if (NOT HAS_STD_FILESYSTEM)
  message(WARNING "Compiler seems to lack std::filesystem support!")
  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    message(WARNING "Try GCC version 8.1 or later ...")
  elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    message(WARNING "Try Clang version 8 or later ...")
  elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    message(WARNING "Try AppleClang version 11 or later ...")
  elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    message(WARNING "Try Visual Studio 2019 or later ...")
  else()
    message(WARNING "Check for a newer version of your compiler implementing C++17 std::filesystem ...")
  endif()
endif()

# Libraries
add_subdirectory(libs/crossplatform)
add_subdirectory(libs/errlog)
add_subdirectory(libs/rtefsutils)
add_subdirectory(libs/rtemodel)
add_subdirectory(libs/rteutils)
add_subdirectory(libs/xmlreader)
add_subdirectory(libs/xmltree)
add_subdirectory(libs/xmltreeslim)
add_subdirectory(libs/ymltree)
add_subdirectory(libs/ymlschemachecker)
add_subdirectory(libs/xmlschemachecker)

# Tools
if(NOT LIBS_ONLY)
  add_subdirectory(tools/buildmgr)
  add_subdirectory(tools/packchk)
  add_subdirectory(tools/packgen)
  add_subdirectory(tools/projmgr)
  add_subdirectory(tools/svdconv)
endif()

# Prepare a list of CMake targets
get_targets()

# Reset global compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEFAULT}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_DEFAULT}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_DEFAULT}")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "")

# Apply patch on submodules
apply_patch(${CMAKE_CURRENT_SOURCE_DIR}/external/json)
apply_patch(${CMAKE_CURRENT_SOURCE_DIR}/external/json-schema-validator)
apply_patch(${CMAKE_CURRENT_SOURCE_DIR}/external/cxxopts)
apply_patch(${CMAKE_CURRENT_SOURCE_DIR}/external/xerces-c)
apply_patch(${CMAKE_CURRENT_SOURCE_DIR}/external/yaml-cpp)

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
# suppress warnings on external/yaml-cpp
set(not-msvc $<NOT:$<CXX_COMPILER_ID:MSVC>>)
target_compile_options(yaml-cpp
  PRIVATE
  $<${not-msvc}: -Wno-effc++ -Wno-shadow>)

# cxxopts
add_subdirectory(external/cxxopts)

# Json
add_subdirectory(external/json)

# json-schema-validator
add_subdirectory(external/json-schema-validator)

#fmt
add_subdirectory(external/fmt)

#xerces-c
set(XERCESC_BUILD_SHARED_LIBS OFF)
add_subdirectory(external/xerces-c)
