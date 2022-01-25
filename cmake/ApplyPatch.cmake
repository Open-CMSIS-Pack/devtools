# Copyright (c) 2020-2021 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

macro(execute_reset_head_cmd target)
    # git reset --hard
    execute_process(COMMAND ${GIT_EXECUTABLE} reset --hard --quiet
        WORKING_DIRECTORY ${target}
        RESULT_VARIABLE GIT_RESET_RESULT)
    if(NOT GIT_RESET_RESULT EQUAL "0")
        message(FATAL_ERROR "reset failed for ${target}")
    endif()
endmacro()

macro(execute_apply_patch_cmd target)
    # git apply <patch_file>
    execute_process(COMMAND ${GIT_EXECUTABLE} apply "${target}.patch"
        WORKING_DIRECTORY ${target}
        RESULT_VARIABLE GIT_PATCH_RESULT)
    if(NOT GIT_PATCH_RESULT EQUAL "0")
        message(FATAL_ERROR "Apply patch failed for ${target}")
    else()
        message(STATUS "Patch applied successfully on ${target}")
    endif()
endmacro()

function(apply_patch target)
    find_package(Git QUIET)
    if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
        execute_reset_head_cmd(${target})
        execute_apply_patch_cmd(${target})
    endif()
endfunction()
