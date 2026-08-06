# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

function(ctrace_require_variables)
    foreach(required ${ARGN})
        if(NOT DEFINED ${required})
            message(FATAL_ERROR "Missing -D${required}=...")
        endif()
    endforeach()
endfunction()

function(ctrace_prepare_work_directory work_dir)
    file(REMOVE_RECURSE "${work_dir}")
    file(MAKE_DIRECTORY "${work_dir}")
endfunction()

function(ctrace_require_result actual_var expected_var stderr_var)
    if(NOT "${${actual_var}}" EQUAL "${${expected_var}}")
        message(FATAL_ERROR
            "ctrace returned ${${actual_var}}, expected ${${expected_var}}\n"
            "stderr:\n${${stderr_var}}")
    endif()
endfunction()

function(ctrace_require_stderr_match stderr_var pattern_var)
    if(NOT "${${stderr_var}}" MATCHES "${${pattern_var}}")
        message(FATAL_ERROR
            "ctrace stderr does not contain '${${pattern_var}}'\n"
            "stderr:\n${${stderr_var}}")
    endif()
endfunction()

function(ctrace_require_files_exist)
    foreach(required_file ${ARGN})
        if(NOT EXISTS "${required_file}")
            message(FATAL_ERROR "Missing required file: ${required_file}")
        endif()
    endforeach()
endfunction()

function(ctrace_require_nonempty_files)
    foreach(required_file ${ARGN})
        if(NOT EXISTS "${required_file}" OR IS_DIRECTORY "${required_file}")
            message(FATAL_ERROR "Missing required regular file: ${required_file}")
        endif()
        file(SIZE "${required_file}" file_size)
        if(file_size EQUAL 0)
            message(FATAL_ERROR "Required file is empty: ${required_file}")
        endif()
    endforeach()
endfunction()

set(ctrace_command "${CTRACE_EXECUTABLE}")
