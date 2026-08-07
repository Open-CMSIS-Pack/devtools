# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

include(DumpCMakeProjectVersion)
include(ProjectVersionFromGitTag)
get_version_from_git_tag("tools/ctrace/")

project(ctrace VERSION "${PROJECT_VERSION}")
dump_cmake_project_version()

configure_file(src/cli/ProductInfo.h.in ProductInfo.h)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ctrace)

add_subdirectory(src)

set(CTRACE_HEADER_FILES
  src/CtraceMain.h
)
source_group("Header Files" FILES ${CTRACE_HEADER_FILES})

add_library(ctracelib OBJECT
  src/CtraceMain.cpp
  ${CTRACE_HEADER_FILES}
)
target_link_libraries(ctracelib PUBLIC
  ctrace::control
)
target_include_directories(ctracelib PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

add_executable(ctrace src/main.cpp)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(ctrace PRIVATE src/ctrace.manifest)
endif()
target_link_libraries(ctrace PRIVATE
  ctracelib
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
   (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR
    CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
  target_link_options(ctrace PRIVATE "-static")
endif()

add_subdirectory(test)
