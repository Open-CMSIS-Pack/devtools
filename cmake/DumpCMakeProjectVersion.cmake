if(__dump_cmake_project_version)
    return()
endif()
set(__dump_cmake_project_version YES)

# We must run the following at "include" time, not at function call time,
# to find the path to this module rather than the path to a calling list file
get_filename_component(_cmake_project_version_dir ${CMAKE_CURRENT_LIST_FILE} PATH)

function(dump_cmake_project_version)
    configure_file("${_cmake_project_version_dir}/CMAKE_PROJECT_VERSION.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/CMAKE_PROJECT_VERSION" @ONLY)
endfunction()
