include(DumpCMakeProjectVersion)
include(ProjectVersionFromGitTag)
get_version_from_git_tag("tools/svdconv/")

project(SVDConv VERSION "${PROJECT_VERSION}")
dump_cmake_project_version()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT svdconv)

enable_testing()

add_subdirectory(SVDModel)
add_subdirectory(SVDGenerator)
add_subdirectory(SVDConv)

add_subdirectory(test/data)
add_subdirectory(test/unittests)
add_subdirectory(test/integtests)
