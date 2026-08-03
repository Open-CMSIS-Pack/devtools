# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_LIST_DIR}/CtraceTestCommand.cmake")
ctrace_require_variables(
        CTRACE_EXECUTABLE
        FIXTURE_DIR
        TARGET
        WORK_DIR
        EXPECTED_RAW_SHA256
)

set(raw_input "${FIXTURE_DIR}/${TARGET}.SWO.raw")
set(trace_run "${FIXTURE_DIR}/${TARGET}.ctrace-run.yml")
ctrace_require_files_exist("${raw_input}" "${trace_run}")

file(SHA256 "${raw_input}" raw_sha256)
if(NOT raw_sha256 STREQUAL EXPECTED_RAW_SHA256)
    message(FATAL_ERROR
        "Reset-recovery fixture SHA-256 mismatch: ${raw_sha256}")
endif()
foreach(sync_offset IN ITEMS 407861 407989)
    file(READ "${raw_input}" hardware_sync
        OFFSET ${sync_offset}
        LIMIT 6
        HEX)
    if(NOT hardware_sync STREQUAL "000000000080")
        message(FATAL_ERROR
            "Reset-recovery fixture has no hardware ITM sync at raw offset ${sync_offset}")
    endif()
endforeach()

ctrace_prepare_work_directory("${WORK_DIR}")
file(COPY "${raw_input}" "${trace_run}" DESTINATION "${WORK_DIR}")

execute_process(
    COMMAND
        ${ctrace_command}
        "${WORK_DIR}"
        --target "${TARGET}"
        --csv
    RESULT_VARIABLE result
    ERROR_VARIABLE stderr
)
if(NOT result EQUAL 1)
    message(FATAL_ERROR
        "ctrace returned ${result}, expected 1 for the known reset discontinuity\n"
        "stderr:\n${stderr}")
endif()

foreach(expected_stderr IN ITEMS
        "\\[error\\] decode/opencsd-bad-packet-sequence: invalid ITM packet sequence at raw offset 407871"
        "\\[error\\] decode/data-loss: 116 raw bytes from raw offset 407873 could not be decoded before the next hardware ITM sync"
        "decode/summary: decoded 318443 packets from 816560 bytes")
    if(NOT stderr MATCHES "${expected_stderr}")
        message(FATAL_ERROR
            "ctrace did not report expected reset recovery '${expected_stderr}'\n"
            "stderr:\n${stderr}")
    endif()
endforeach()
if(stderr MATCHES "opencsd-no-progress|trace-directory-failed")
    message(FATAL_ERROR
        "ctrace aborted instead of recovering after the reset discontinuity:\n${stderr}")
endif()

set(csv_output "${WORK_DIR}/${TARGET}.SWO.csv")
ctrace_require_files_exist("${csv_output}")
file(READ "${csv_output}" csv)
if(csv MATCHES ",itm,")
    message(FATAL_ERROR
        "Reset-recovery fixture unexpectedly contains software ITM data")
endif()

set(recovery_error
    "1263538492,0,error,,,,,OpenCSD detected an invalid ITM packet sequence at raw offset 407871.")
set(data_loss
    "1263538492,0,error,,,,,OpenCSD consumed 116 raw bytes while waiting for usable ITM trace packets")
set(first_resumed_event "1535311750,0,dwt,0,0x00,,,")
set(late_resumed_event "2594186112,0,dwt,0,0x5b,,,")

string(FIND "${csv}" "${recovery_error}" recovery_error_position)
string(FIND "${csv}" "${data_loss}" data_loss_position)
string(FIND "${csv}" "${first_resumed_event}" first_resumed_position)
string(FIND "${csv}" "${late_resumed_event}" late_resumed_position)
if(recovery_error_position LESS 0 OR data_loss_position LESS 0 OR
   first_resumed_position LESS 0 OR late_resumed_position LESS 0)
    message(FATAL_ERROR
        "Reset-recovery CSV does not contain all expected error and resumed DWT events")
endif()
if(NOT recovery_error_position LESS data_loss_position OR
   NOT data_loss_position LESS first_resumed_position OR
   NOT first_resumed_position LESS late_resumed_position)
    message(FATAL_ERROR
        "Reset-recovery CSV events are not in the expected decode order")
endif()
