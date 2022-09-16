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

add_subdirectory(Test/data)
add_subdirectory(Test/UnitTests)
add_subdirectory(Test/integtests)
