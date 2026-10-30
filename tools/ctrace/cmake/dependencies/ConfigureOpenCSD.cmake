# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# Generated with AI

set(CTRACE_OPENCSD_SOURCE_DIR "${CMAKE_SOURCE_DIR}/external/OpenCSD")
if(NOT EXISTS "${CTRACE_OPENCSD_SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR
    "OpenCSD is unavailable. Initialize the external/OpenCSD submodule."
  )
endif()

function(ctrace_apply_opencsd_patch)
  find_package(Git REQUIRED QUIET)
  set(patch_file "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/OpenCSD.patch")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${patch_file}")

  # Reconfiguration must preserve both the patch and unrelated local edits.
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${patch_file}"
    WORKING_DIRECTORY "${CTRACE_OPENCSD_SOURCE_DIR}"
    RESULT_VARIABLE patch_applied
    OUTPUT_QUIET ERROR_QUIET
  )
  if(patch_applied EQUAL 0)
    return()
  endif()

  # git apply validates all hunks before changing any file; never reset the submodule.
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply "${patch_file}"
    WORKING_DIRECTORY "${CTRACE_OPENCSD_SOURCE_DIR}"
    RESULT_VARIABLE patch_result
    ERROR_VARIABLE patch_error
  )
  if(NOT patch_result EQUAL 0)
    message(FATAL_ERROR "Failed to apply the ctrace OpenCSD patch: ${patch_error}")
  endif()
endfunction()

function(ctrace_remove_opencsd_clang_cl_option_for_msvc)
  if(NOT MSVC OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    return()
  endif()

  set_property(TARGET opencsd_options PROPERTY INTERFACE_COMPILE_OPTIONS)
  target_compile_options(opencsd PRIVATE /EHsc /W1)
endfunction()

set(OPENCSD_BUILD_MIN_LIB_STATIC ON CACHE BOOL "" FORCE)
set(OPENCSD_FULL_PROJECT_LAYOUT OFF CACHE BOOL "" FORCE)

# Do not let the embedded project register its own tests or replace the
# devtools install prefix.
get_property(ctrace_had_build_testing CACHE BUILD_TESTING PROPERTY TYPE SET)
if(ctrace_had_build_testing)
  get_property(ctrace_saved_build_testing CACHE BUILD_TESTING PROPERTY VALUE)
endif()
set(BUILD_TESTING OFF CACHE BOOL "Build the testing tree." FORCE)

set(ctrace_saved_install_prefix_default "${CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT}")
set(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT FALSE)

ctrace_apply_opencsd_patch()
add_subdirectory(
  "${CTRACE_OPENCSD_SOURCE_DIR}"
  "${CMAKE_BINARY_DIR}/external/OpenCSD"
  EXCLUDE_FROM_ALL
)

if(ctrace_had_build_testing)
  set(BUILD_TESTING "${ctrace_saved_build_testing}" CACHE BOOL "Build the testing tree." FORCE)
else()
  unset(BUILD_TESTING CACHE)
endif()
set(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT "${ctrace_saved_install_prefix_default}")

ctrace_remove_opencsd_clang_cl_option_for_msvc()
target_compile_definitions(opencsd PRIVATE ENABLE_LARGE_TRACE_SOURCES)

add_library(ctrace-opencsd INTERFACE)
add_library(ctrace::opencsd ALIAS ctrace-opencsd)
target_compile_definitions(ctrace-opencsd INTERFACE
  ENABLE_LARGE_TRACE_SOURCES
)
target_include_directories(ctrace-opencsd SYSTEM INTERFACE
  "${CTRACE_OPENCSD_SOURCE_DIR}/decoder/include"
)
target_link_libraries(ctrace-opencsd INTERFACE
  "$<LINK_ONLY:OpenCSD::opencsd>"
)
