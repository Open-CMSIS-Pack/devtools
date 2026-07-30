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

function(ctrace_apply_opencsd_itm_empty_packet_fix source_dir)
  set(source_file "${source_dir}/decoder/source/itm/trc_pkt_proc_itm.cpp")
  if(NOT EXISTS "${source_file}")
    message(FATAL_ERROR "OpenCSD ITM packet processor not found: ${source_file}")
  endif()

  file(READ "${source_file}" source_contents)
  set(unsafe_expression "&m_packet_data[0]")
  set(safe_expression "m_packet_data.data()")
  string(FIND "${source_contents}" "${unsafe_expression}" unsafe_position)
  if(unsafe_position EQUAL -1)
    string(FIND "${source_contents}" "${safe_expression}" safe_position)
    if(safe_position EQUAL -1)
      message(FATAL_ERROR
        "Unsupported OpenCSD ITM packet processor; expected empty-packet fix location was not found."
      )
    endif()
    return()
  endif()

  string(REPLACE "${unsafe_expression}" "${safe_expression}" patched_contents "${source_contents}")
  set(patched_dir "${CMAKE_BINARY_DIR}/patched-opencsd/decoder/source/itm")
  set(patched_file "${patched_dir}/trc_pkt_proc_itm.cpp")
  file(MAKE_DIRECTORY "${patched_dir}")
  set(write_patched_file TRUE)
  if(EXISTS "${patched_file}")
    file(READ "${patched_file}" existing_patched_contents)
    if(existing_patched_contents STREQUAL patched_contents)
      set(write_patched_file FALSE)
    endif()
  endif()
  if(write_patched_file)
    file(WRITE "${patched_file}" "${patched_contents}")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source_file}")

  get_target_property(opencsd_sources opencsd SOURCES)
  set(patched_sources)
  set(replaced_source FALSE)
  foreach(opencsd_source IN LISTS opencsd_sources)
    if(opencsd_source MATCHES "(^|/)decoder/source/itm/trc_pkt_proc_itm\\.cpp$")
      list(APPEND patched_sources "${patched_file}")
      set(replaced_source TRUE)
    else()
      list(APPEND patched_sources "${opencsd_source}")
    endif()
  endforeach()
  if(NOT replaced_source)
    message(FATAL_ERROR "OpenCSD target does not contain the ITM packet processor source.")
  endif()
  set_property(TARGET opencsd PROPERTY SOURCES ${patched_sources})
endfunction()

function(ctrace_remove_opencsd_clang_cl_option_for_msvc)
  if(NOT MSVC OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    return()
  endif()

  set_property(TARGET opencsd_options PROPERTY INTERFACE_COMPILE_OPTIONS)
  target_compile_options(opencsd PRIVATE /EHsc /W0)
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

ctrace_apply_opencsd_itm_empty_packet_fix("${CTRACE_OPENCSD_SOURCE_DIR}")
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
