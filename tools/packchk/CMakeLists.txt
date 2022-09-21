include(DumpCMakeProjectVersion)
include(ProjectVersionFromGitTag)
get_version_from_git_tag("tools/packchk/")

project(packchk VERSION "${PROJECT_VERSION}")
dump_cmake_project_version()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT packchk)

set(PROJECT_COPYRIGHT_NOTICE "(C) 2012-${PROJECT_VERSION_YEAR} Arm Ltd. and Contributors")
set(PROJECT_TARGET_FILE_NAME "$<TARGET_FILE_NAME:packchk>")

configure_file(src/ProductInfo.h.in ProductInfo.h.out)
file(GENERATE
  OUTPUT "$<CONFIG>/ProductInfo.h"
  INPUT ${CMAKE_CURRENT_BINARY_DIR}/ProductInfo.h.out
)
if(MSVC)
    configure_file(src/PackChk.rc.in PackChk.rc)
endif()

# Note that the main() entry point is excluded from the list of sources. This allows
# it to be excluded for test purposes.
set(LIB_SOURCE_FILES PackChk.cpp PackOptions.cpp ParseOptions.cpp RteModelReader.cpp
  Validate.cpp ValidateSemantic.cpp ValidateSyntax.cpp CheckComponents.cpp
  CheckConditions.cpp CheckFiles.cpp CreateModel.cpp
  PackChk_Msgs.cpp GatherCompilers.cpp)

set(LIB_HEADER_FILES PackChk.h PackOptions.h ParseOptions.h Resource.h RteModelReader.h Validate.h
  ValidateSemantic.h ValidateSyntax.h CheckComponents.h CheckConditions.h
  CheckFiles.h CreateModel.h GatherCompilers.h)

list(TRANSFORM LIB_SOURCE_FILES PREPEND src/)
list(TRANSFORM LIB_HEADER_FILES PREPEND include/)

# Create the 'library' containing all of the pack check code except main()
add_library(packchklib OBJECT ${LIB_SOURCE_FILES} ${LIB_HEADER_FILES})
target_include_directories(packchklib
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/
  PRIVATE ${PROJECT_BINARY_DIR}/$<CONFIG>
)

set_property(TARGET packchklib PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_link_libraries(packchklib CrossPlatform ErrLog RteModel RteFsUtils RteUtils XmlTree XmlTreeSlim cxxopts)

# Create the packchk target
add_executable(packchk src/PackChkMain.cpp)
if(MSVC)
	target_sources(packchk PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/PackChk.rc)
endif()

set_property(TARGET packchk PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_link_libraries(packchk PUBLIC packchklib)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ARCH "64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(ARCH "32")
endif()

string(TOLOWER ${CMAKE_SYSTEM_NAME} SYSTEM)

add_custom_target(packchkdist COMMAND
    ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_CURRENT_BINARY_DIR}/packchk-${PROJECT_VERSION_FULL}-${SYSTEM}${ARCH}.zip" --format=zip
       "$<TARGET_FILE:packchk>"
       DEPENDS packchk
       WORKING_DIRECTORY "$<TARGET_FILE_DIR:packchk>")

enable_testing()
add_subdirectory(test/unittests)
add_subdirectory(test/integtests)
