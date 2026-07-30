# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# Generated with AI

foreach(required CTRACE_EXECUTABLE CTRACE_ARGS EXPECTED_STDERR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/CtraceTestCommand.cmake")

execute_process(
    COMMAND ${ctrace_command} ${CTRACE_ARGS}
    RESULT_VARIABLE result
    ERROR_VARIABLE stderr
)

if(result EQUAL 0)
    message(FATAL_ERROR
        "ctrace unexpectedly succeeded\n"
        "stderr:\n${stderr}")
endif()
if(NOT stderr MATCHES "${EXPECTED_STDERR}")
    message(FATAL_ERROR
        "ctrace stderr does not contain '${EXPECTED_STDERR}'\n"
        "stderr:\n${stderr}")
endif()
