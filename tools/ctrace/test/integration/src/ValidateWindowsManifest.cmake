# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

function(validate_manifest manifest_file description)
    if(NOT EXISTS "${manifest_file}")
        message(FATAL_ERROR "Missing ${description}: ${manifest_file}")
    endif()

    file(READ "${manifest_file}" manifest)
    if(NOT manifest MATCHES
            "<ws2:longPathAware>[ \t\r\n]*true[ \t\r\n]*</ws2:longPathAware>")
        message(FATAL_ERROR
            "${description} does not enable long-path awareness: ${manifest_file}")
    endif()
    if(NOT manifest MATCHES
            "xmlns:ws2=\"http://schemas.microsoft.com/SMI/2016/WindowsSettings\"")
        message(FATAL_ERROR
            "${description} uses no recognized long-path namespace: ${manifest_file}")
    endif()
endfunction()

if(NOT DEFINED MANIFEST_FILE)
    message(FATAL_ERROR "Missing -DMANIFEST_FILE=...")
endif()
validate_manifest("${MANIFEST_FILE}" "Windows manifest source")

if(WIN32)
    foreach(required_variable CTRACE_EXECUTABLE MT_EXECUTABLE EMBEDDED_MANIFEST_FILE)
        if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
            message(FATAL_ERROR "Missing -D${required_variable}=...")
        endif()
    endforeach()
    if(NOT EXISTS "${CTRACE_EXECUTABLE}")
        message(FATAL_ERROR "Missing ctrace executable: ${CTRACE_EXECUTABLE}")
    endif()
    if(NOT EXISTS "${MT_EXECUTABLE}")
        message(FATAL_ERROR "Missing Windows manifest tool: ${MT_EXECUTABLE}")
    endif()

    file(REMOVE "${EMBEDDED_MANIFEST_FILE}")
    execute_process(
        COMMAND "${MT_EXECUTABLE}"
            -nologo
            "-inputresource:${CTRACE_EXECUTABLE};#1"
            "-out:${EMBEDDED_MANIFEST_FILE}"
        RESULT_VARIABLE mt_result
        OUTPUT_VARIABLE mt_stdout
        ERROR_VARIABLE mt_stderr
    )
    if(NOT mt_result EQUAL 0)
        message(FATAL_ERROR
            "Failed to extract the embedded ctrace manifest (${mt_result}):\n"
            "${mt_stdout}${mt_stderr}")
    endif()
    validate_manifest("${EMBEDDED_MANIFEST_FILE}" "Embedded ctrace manifest")
endif()
