# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

include(DumpCMakeProjectVersion)
include(ProjectVersionFromGitTag)
get_version_from_git_tag("tools/ctrace/")

project(ctrace VERSION "${PROJECT_VERSION}")
dump_cmake_project_version()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ctrace)

# ctrace library
add_library(ctracelib OBJECT src/CtraceMain.cpp include/CtraceMain.h)
target_include_directories(ctracelib PUBLIC include)

set_property(TARGET ctracelib PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ctrace target
add_executable(ctrace src/main.cpp)
target_link_libraries(ctrace PRIVATE ctracelib)

set_property(TARGET ctrace PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR
   "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  target_link_options(ctrace PUBLIC "-static")
endif()

enable_testing()
add_subdirectory(test)
