include(DumpCMakeProjectVersion)
include(ProjectVersionFromGitTag)
get_version_from_git_tag("tools/projmgr/")

project(ProjMgr VERSION "${PROJECT_VERSION}")
dump_cmake_project_version()

configure_file(src/ProductInfo.h.in ProductInfo.h)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT projmgr)

# projmgr library
SET(PROJMGR_SOURCE_FILES ProjMgr.cpp ProjMgrKernel.cpp ProjMgrCallback.cpp
  ProjMgrParser.cpp ProjMgrWorker.cpp ProjMgrGenerator.cpp ProjMgrXmlParser.cpp
  ProjMgrYamlParser.cpp ProjMgrLogger.cpp ProjMgrYamlSchemaChecker.cpp
  ProjMgrYamlEmitter.cpp ProjMgrUtils.cpp ProjMgrExtGenerator.cpp
)
SET(PROJMGR_HEADER_FILES ProjMgr.h ProjMgrKernel.h ProjMgrCallback.h
  ProjMgrParser.h ProjMgrWorker.h ProjMgrGenerator.h ProjMgrXmlParser.h
  ProjMgrYamlParser.h ProjMgrLogger.h ProjMgrYamlSchemaChecker.h
  ProjMgrYamlEmitter.h ProjMgrUtils.h ProjMgrExtGenerator.h
)

list(TRANSFORM PROJMGR_SOURCE_FILES PREPEND src/)
list(TRANSFORM PROJMGR_HEADER_FILES PREPEND include/)

add_library(projmgrlib OBJECT ${PROJMGR_SOURCE_FILES} ${PROJMGR_HEADER_FILES})
target_link_libraries(projmgrlib
  PUBLIC
  CrossPlatform RteFsUtils RteUtils XmlTree XmlTreeSlim XmlReader
  RteModel cxxopts yaml-cpp YmlSchemaChecker)
target_include_directories(projmgrlib PRIVATE include ${PROJECT_BINARY_DIR})

if(SWIG_LIBS)
  # projmgr swig
  add_subdirectory(swig)
else()
  # projmgr target
  add_executable(projmgr src/ProjMgrMain.cpp)
  set_target_properties(projmgr PROPERTIES OUTPUT_NAME "csolution")
  if(MSVC)
    target_link_options(projmgr PUBLIC "$<$<CONFIG:Debug>:/SAFESEH:NO>")
  endif(MSVC)
  set_property(TARGET projmgr PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR
     "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_link_options(projmgr PUBLIC "-static")
  endif()
  target_link_libraries(projmgr projmgrlib)
  target_include_directories(projmgr PRIVATE include)
  if("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    target_sources(projmgr PRIVATE src/csolution.manifest)
  endif()
endif()

# projmgr test
enable_testing()
add_subdirectory(test)
