# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# Generated with AI

include("${CMAKE_CURRENT_LIST_DIR}/CtraceTestCommand.cmake")
ctrace_require_variables(CTRACE_EXECUTABLE CTRACE_ARGS EXPECTED_STDERR)

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
ctrace_require_stderr_match(stderr EXPECTED_STDERR)
