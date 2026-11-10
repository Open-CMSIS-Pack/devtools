# Copyright (c) 2023-2026 Arm Limited and Contributors. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# CMake Utility functions for CMSIS-Build

# Get running toolchain root directory and version
# Check whether the running toolchain fits the required version range
function(cbuild_get_running_toolchain root version lang)
  set(CMAKE_COMPILER_VERSION ${CMAKE_${lang}_COMPILER_VERSION})
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

# Initialize CMake's language-wide C and C++ flags
# Omit language-selection options as long as CMake also passes these flags
# to compiler-driver invocations that perform linking.
# See https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_FLAGS.html
function(cbuild_set_cmake_initial_flags)
  foreach(LANG C CXX)
    set(FILTERED_OPTIONS)
    set(LANGUAGE_OPTION_PENDING FALSE)

    foreach(OPTION IN LISTS CPP_OPTIONS_${LANG})
      if(LANGUAGE_OPTION_PENDING)
        set(LANGUAGE_OPTION_PENDING FALSE)
      elseif(OPTION STREQUAL "-x")
        set(LANGUAGE_OPTION_PENDING TRUE)
      elseif(NOT OPTION MATCHES "^-x.+")
        list(APPEND FILTERED_OPTIONS "${OPTION}")
      endif()
    endforeach()

    list(JOIN FILTERED_OPTIONS " " OPTIONS_STR)
    if(LANG STREQUAL "C")
      set(CMAKE_C_FLAGS_INIT "${CC_CPU} ${OPTIONS_STR}" PARENT_SCOPE)
    else()
      set(CMAKE_CXX_FLAGS_INIT "${CXX_CPU} ${OPTIONS_STR}" PARENT_SCOPE)
    endif()
  endforeach()
endfunction()
