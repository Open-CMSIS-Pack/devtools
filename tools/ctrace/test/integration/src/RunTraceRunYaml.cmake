# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# Generated with AI

foreach(required CTRACE_EXECUTABLE YAML_FILE WORK_DIR TARGET EXPECTED_RESULT EXPECTED_STDERR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing -D${required}=...")
    endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/CtraceTestCommand.cmake")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
configure_file("${YAML_FILE}" "${WORK_DIR}/${TARGET}.ctrace-run.yml" COPYONLY)
if(RAW_DECODER_ERROR)
    string(ASCII 1 decoder_error_prefix)
    file(WRITE "${WORK_DIR}/${TARGET}.SWO.raw" "${decoder_error_prefix}A")
else()
    file(WRITE "${WORK_DIR}/${TARGET}.SWO.raw" "")
endif()

set(command ${ctrace_command} "${WORK_DIR}" --target "${TARGET}")
if(DEFINED OUTPUT_OPTION)
    list(APPEND command "${OUTPUT_OPTION}")
endif()
execute_process(
    COMMAND ${command}
    RESULT_VARIABLE result
    ERROR_VARIABLE stderr
)

if(NOT result EQUAL EXPECTED_RESULT)
    message(FATAL_ERROR
        "ctrace returned ${result}, expected ${EXPECTED_RESULT}\n"
        "stderr:\n${stderr}")
endif()
if(NOT stderr MATCHES "${EXPECTED_STDERR}")
    message(FATAL_ERROR
        "ctrace stderr does not contain '${EXPECTED_STDERR}'\n"
        "stderr:\n${stderr}")
endif()
foreach(index RANGE 2 6)
    if(DEFINED EXPECTED_STDERR_${index}
       AND NOT stderr MATCHES "${EXPECTED_STDERR_${index}}")
        message(FATAL_ERROR
            "ctrace stderr does not contain '${EXPECTED_STDERR_${index}}'\n"
            "stderr:\n${stderr}")
    endif()
endforeach()
if(DEFINED UNEXPECTED_STDERR AND stderr MATCHES "${UNEXPECTED_STDERR}")
    message(FATAL_ERROR
        "ctrace stderr unexpectedly contains '${UNEXPECTED_STDERR}'\n"
        "stderr:\n${stderr}")
endif()
if(EXPECT_NO_OUTPUT)
    file(GLOB generated_outputs
        "${WORK_DIR}/*.csv"
        "${WORK_DIR}/*.ctf"
        "${WORK_DIR}/*.traceanalysis.xml")
    if(generated_outputs)
        message(FATAL_ERROR
            "ctrace generated output despite a fatal configuration error: ${generated_outputs}")
    endif()
endif()

set(csv_path "${WORK_DIR}/${TARGET}.SWO.csv")
set(ctf_path "${WORK_DIR}/${TARGET}.ctf")
set(ctf_stream_path "${ctf_path}/stream_0")
set(xml_path "${WORK_DIR}/${TARGET}.SWO.traceanalysis.xml")
if(DEFINED EXPECT_CSV)
    if(EXPECT_CSV AND NOT EXISTS "${csv_path}")
        message(FATAL_ERROR "ctrace did not generate expected CSV output: ${csv_path}")
    elseif(NOT EXPECT_CSV AND EXISTS "${csv_path}")
        message(FATAL_ERROR "ctrace unexpectedly generated CSV output: ${csv_path}")
    endif()
endif()
if(DEFINED EXPECT_CTF)
    if(EXPECT_CTF)
        if(NOT EXISTS "${ctf_path}/metadata"
           OR NOT EXISTS "${ctf_stream_path}"
           OR NOT EXISTS "${xml_path}")
            message(FATAL_ERROR
                "ctrace did not generate the complete expected CTF bundle: ${ctf_path}")
        endif()
    elseif(EXISTS "${ctf_path}" OR EXISTS "${xml_path}")
        message(FATAL_ERROR
            "ctrace unexpectedly generated a CTF bundle: ${ctf_path}")
    endif()
endif()
if(EXPECT_CSV_ERROR_ROW)
    if(NOT EXISTS "${csv_path}")
        message(FATAL_ERROR "cannot inspect missing CSV output: ${csv_path}")
    endif()
    file(READ "${csv_path}" csv_contents)
    if(NOT csv_contents MATCHES ",error,")
        message(FATAL_ERROR
            "CSV output does not contain the expected decoder error row:\n${csv_contents}")
    endif()
endif()
