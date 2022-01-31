include(DumpCMakeProjectVersion)
include(ProjectVersionFromGitTag)
get_version_from_git_tag("tools/buildmgr/")

project(CMSIS_Build VERSION "${PROJECT_VERSION}")
dump_cmake_project_version()

configure_file(cbuildgen/src/ProductInfo.h.in cbuildgen/include/ProductInfo.h)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT cbuildgen)

add_subdirectory(cbuild)
add_subdirectory(cbuildgen)
add_subdirectory(docs)

add_subdirectory(test/unittests)
add_subdirectory(test/integrationtests)
