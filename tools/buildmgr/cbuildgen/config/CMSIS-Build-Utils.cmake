# Copyright (c) 2023 Arm Limited and Contributors. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# CMake Utility functions for CMSIS-Build

# Get running toolchain root directory and version
# Check whether the running toolchain matches the registered one and fits the required version range
function(cbuild_get_running_toolchain root version lang)
  set(CMAKE_COMPILER_VERSION ${CMAKE_${lang}_COMPILER_VERSION})
  if(DEFINED TOOLCHAIN_VERSION)
    if(NOT TOOLCHAIN_VERSION VERSION_EQUAL CMAKE_COMPILER_VERSION)
      message(STATUS "Registered toolchain version ${TOOLCHAIN_VERSION} does not match running version ${CMAKE_COMPILER_VERSION}")
    endif()
  endif()
  if(CMAKE_COMPILER_VERSION VERSION_LESS TOOLCHAIN_VERSION_MIN)
    message(WARNING "Running toolchain version ${CMAKE_COMPILER_VERSION} is less than minimum required ${TOOLCHAIN_VERSION_MIN}")
  endif()
  if(DEFINED TOOLCHAIN_VERSION_MAX)
    if(CMAKE_COMPILER_VERSION VERSION_GREATER TOOLCHAIN_VERSION_MAX)
      message(WARNING "Running toolchain version ${CMAKE_COMPILER_VERSION} is greater than maximum required ${TOOLCHAIN_VERSION_MAX}")
    endif()
  endif()
  set(CMAKE_COMPILER_ROOT ${CMAKE_${lang}_COMPILER})
  cmake_path(GET CMAKE_COMPILER_ROOT PARENT_PATH CMAKE_COMPILER_ROOT)
  set(${root} ${CMAKE_COMPILER_ROOT} PARENT_SCOPE)
  set(${version} ${CMAKE_COMPILER_VERSION} PARENT_SCOPE)
endfunction()

# Get system includes
# Replace TOOLCHAIN_ROOT and TOOLCHAIN_VERSION variables
function(cbuild_get_system_includes input output)
  string(REGEX MATCH "^[0-9]+" TOOLCHAIN_MAJOR_VERSION ${TOOLCHAIN_VERSION})
  foreach(ENTRY ${${input}})
    string(REGEX REPLACE "\\\${TOOLCHAIN_ROOT}" ${TOOLCHAIN_ROOT} ENTRY ${ENTRY})
    string(REGEX REPLACE "\\\${TOOLCHAIN_VERSION}" ${TOOLCHAIN_VERSION} ENTRY ${ENTRY})
    string(REGEX REPLACE "\\\${TOOLCHAIN_MAJOR_VERSION}" ${TOOLCHAIN_MAJOR_VERSION} ENTRY ${ENTRY})
    cmake_path(NORMAL_PATH ENTRY OUTPUT_VARIABLE NORMAL_ENTRY)
    string(APPEND ${output} "${_ISYS}\"${NORMAL_ENTRY}\" ")
  endforeach()
  set(${output} ${${output}} PARENT_SCOPE)
endfunction()
