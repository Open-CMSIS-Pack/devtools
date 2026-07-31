# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

include("${CMAKE_CURRENT_LIST_DIR}/CtraceTestCommand.cmake")
ctrace_require_variables(CTRACE_EXECUTABLE PYTHON_EXECUTABLE YAML_FILE WORK_DIR TARGET)

ctrace_prepare_work_directory("${WORK_DIR}")
configure_file("${YAML_FILE}" "${WORK_DIR}/${TARGET}.ctrace-run.yml" COPYONLY)

# Hardware ITM synchronization followed by one channel-1 software packet with
# the value 0x41. Keep the fixture textual here and materialize it only below
# the build tree so the byte sequence stays reviewable.
set(raw_path "${WORK_DIR}/${TARGET}.SWO.raw")
execute_process(
    COMMAND
        "${PYTHON_EXECUTABLE}"
        -c
        "__import__('pathlib').Path(__import__('sys').argv[1]).write_bytes(bytes.fromhex(__import__('sys').argv[2]))"
        "${raw_path}"
        "0000000000800941"
    RESULT_VARIABLE write_result
    ERROR_VARIABLE write_stderr
)
if(NOT write_result EQUAL 0)
    message(FATAL_ERROR "Failed to create valid SWO fixture: ${write_stderr}")
endif()

execute_process(
    COMMAND ${ctrace_command} "${WORK_DIR}" --target "${TARGET}" --all
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR
        "ctrace returned ${result}, expected 0\n"
        "stdout:\n${stdout}\n"
        "stderr:\n${stderr}")
endif()
if(stderr MATCHES "\\[fatal\\]")
    message(FATAL_ERROR "Valid SWO input produced a fatal diagnostic:\n${stderr}")
endif()
if(NOT stderr MATCHES "decode/summary: decoded 2 packets from 8 bytes")
    message(FATAL_ERROR "Valid SWO input produced an unexpected summary:\n${stderr}")
endif()

set(csv_path "${WORK_DIR}/${TARGET}.SWO.csv")
set(ctf_path "${WORK_DIR}/${TARGET}.ctf")
set(xml_path "${WORK_DIR}/${TARGET}.SWO.traceanalysis.xml")
ctrace_require_files_exist(
    "${csv_path}"
    "${ctf_path}/metadata"
    "${ctf_path}/stream_0"
    "${xml_path}"
)

file(READ "${csv_path}" csv_contents)
string(CONCAT expected_csv
    "cycles,stream,type,source,value,pc,offset,note\n"
    "0,0,itm,1,0x41,,,\n")
if(NOT csv_contents STREQUAL expected_csv)
    message(FATAL_ERROR
        "Valid SWO CSV differs from the expected output:\n${csv_contents}")
endif()

foreach(non_empty_file IN ITEMS "${ctf_path}/metadata" "${ctf_path}/stream_0" "${xml_path}")
    file(SIZE "${non_empty_file}" output_size)
    if(output_size EQUAL 0)
        message(FATAL_ERROR "ctrace generated an empty output: ${non_empty_file}")
    endif()
endforeach()
