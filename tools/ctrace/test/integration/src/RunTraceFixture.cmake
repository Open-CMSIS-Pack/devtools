# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_LIST_DIR}/CtraceTestCommand.cmake")
ctrace_require_variables(
        CTRACE_EXECUTABLE
        FIXTURE_DIR
        TARGET
        WORK_DIR
        EXPECTED_RESULT
)

set(swo_raw "${FIXTURE_DIR}/${TARGET}.SWO.raw")
set(tb_raw "${FIXTURE_DIR}/${TARGET}.TB.raw")
set(trace_run "${FIXTURE_DIR}/${TARGET}.ctrace-run.yml")
set(expected_csv "${FIXTURE_DIR}/${TARGET}.SWO.csv")

ctrace_require_files_exist("${swo_raw}" "${tb_raw}" "${trace_run}" "${expected_csv}")

ctrace_prepare_work_directory("${WORK_DIR}")
file(COPY "${swo_raw}" "${tb_raw}" "${trace_run}" DESTINATION "${WORK_DIR}")

execute_process(
    COMMAND
        ${ctrace_command}
        "${WORK_DIR}"
        --target "${TARGET}"
        --csv
    RESULT_VARIABLE result
    ERROR_VARIABLE stderr
)
ctrace_require_result(result EXPECTED_RESULT stderr)

if(NOT stderr MATCHES "unsupported-trace-channel:.*channel=TB")
    message(FATAL_ERROR
        "ctrace did not report TB as an unimplemented trace channel:\n${stderr}")
endif()

set(actual_csv "${WORK_DIR}/${TARGET}.SWO.csv")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${actual_csv}" "${expected_csv}"
    RESULT_VARIABLE compare_result
)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR
        "Generated SWO CSV differs from ${expected_csv}")
endif()

foreach(unexpected_output IN ITEMS
        "${WORK_DIR}/${TARGET}.TB.csv"
        "${WORK_DIR}/${TARGET}.TB.traceanalysis.xml"
        "${WORK_DIR}/${TARGET}.TB.ctf")
    if(EXISTS "${unexpected_output}")
        message(FATAL_ERROR
            "TB output must not be generated before TB support is implemented: ${unexpected_output}")
    endif()
endforeach()
