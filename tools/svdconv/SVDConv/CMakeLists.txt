include(DumpCMakeProjectVersion)
include(ProjectVersionFromGitTag)
get_version_from_git_tag("tools/svdconv/")

project(svdconv VERSION "${PROJECT_VERSION}")
dump_cmake_project_version()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT svdconv)

set(PROJECT_COPYRIGHT_NOTICE "Copyright (C) 2010-${PROJECT_VERSION_YEAR} ARM Ltd and ARM Germany GmbH. All rights reserved.")
set(PROJECT_TARGET_FILE_NAME "$<TARGET_FILE_NAME:svdconv>")
configure_file(src/ProductInfo.h.in ProductInfo.h.out)
file(GENERATE
  OUTPUT "$<CONFIG>/ProductInfo.h"
  INPUT ${CMAKE_CURRENT_BINARY_DIR}/ProductInfo.h.out
)

if(MSVC)
    configure_file(src/SVDConv.rc.in SVDConv.rc)
    configure_file(cmdFile.cf.in cmdFile.cf)
endif()

# Note that the main() entry point is excluded from the list of sources. This allows
# it to be excluded for test purposes.
set(LIB_SOURCE_FILES SVDConv.cpp ParseOptions.cpp SvdOptions.cpp SVDConv_Msgs.cpp)

set(LIB_HEADER_FILES SVDConv.h ParseOptions.h SvdOptions.h Resource.h)

list(TRANSFORM LIB_SOURCE_FILES PREPEND src/)
list(TRANSFORM LIB_HEADER_FILES PREPEND include/)

# Create the 'library' containing all of the pack check code except main()
add_library(svdconvlib OBJECT ${LIB_SOURCE_FILES} ${LIB_HEADER_FILES})
target_include_directories(svdconvlib
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/
  PRIVATE ${PROJECT_BINARY_DIR}/$<CONFIG>
)

set_property(TARGET svdconvlib PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_link_libraries(svdconvlib SVDGenerator SVDModel CrossPlatform ErrLog RteModel RteFsUtils RteUtils XmlTree XmlTreeSlim cxxopts)

# Create the svdconv target
add_executable(svdconv src/SVDConvMain.cpp)
if(MSVC)
	target_sources(svdconv PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/SVDConv.rc)
endif()

set_property(TARGET svdconv PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR
   "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  target_link_options(svdconv PUBLIC "-static")
endif()

target_link_libraries(svdconv PUBLIC svdconvlib)

if(("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64") OR ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "AMD64"))
  set(SYSTEM_PROCESSOR amd64)
else()
  set(SYSTEM_PROCESSOR arm64)
endif()

add_custom_target(svdconvdist COMMAND
    ${CMAKE_COMMAND} -E tar "cvf${CMAKE_TAR_FLAGS}"
       "${CMAKE_CURRENT_BINARY_DIR}/svdconv-${PROJECT_VERSION_FULL}-${SYSTEM}${ARCH}-${SYSTEM_PROCESSOR}.${CMAKE_TAR_EXTENSION}"
       "--format=${CMAKE_TAR_FORMAT}"
       "$<TARGET_FILE_NAME:svdconv>"
       DEPENDS svdconv
       WORKING_DIRECTORY "$<TARGET_FILE_DIR:svdconv>")

# enable_testing()
# add_subdirectory(test/unittests)
# add_subdirectory(test/integtests)
