# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

foreach(required
        CTRACE_EXECUTABLE
        FIXTURE_DIR
        TARGET
        WORK_DIR
        EXPECTED_RESULT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/CtraceTestCommand.cmake")

set(swo_raw "${FIXTURE_DIR}/${TARGET}.SWO.raw")
set(tb_raw "${FIXTURE_DIR}/${TARGET}.TB.raw")
set(trace_run "${FIXTURE_DIR}/${TARGET}.ctrace-run.yml")
set(expected_csv "${FIXTURE_DIR}/${TARGET}.SWO.csv")

foreach(fixture_file IN ITEMS "${swo_raw}" "${tb_raw}" "${trace_run}" "${expected_csv}")
    if(NOT EXISTS "${fixture_file}")
        message(FATAL_ERROR "Missing fixture file: ${fixture_file}")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
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
if(NOT result EQUAL EXPECTED_RESULT)
    message(FATAL_ERROR
        "ctrace conversion returned ${result}, expected ${EXPECTED_RESULT}:\n${stderr}")
endif()

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
