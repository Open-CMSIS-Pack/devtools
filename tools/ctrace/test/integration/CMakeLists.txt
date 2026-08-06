# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# Generated with AI

function(ctrace_add_integration_test)
    cmake_parse_arguments(PARSE_ARGV 0 TEST "" "NAME;SCRIPT;TIMEOUT" "ARGUMENTS;LABELS")
    if(NOT TEST_NAME OR NOT TEST_SCRIPT OR TEST_UNPARSED_ARGUMENTS OR
       TEST_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR
            "ctrace_add_integration_test requires NAME and SCRIPT; "
            "unexpected arguments: ${TEST_UNPARSED_ARGUMENTS}; "
            "missing values: ${TEST_KEYWORDS_MISSING_VALUES}")
    endif()

    if(DEFINED TEST_TIMEOUT)
        set(test_timeout "${TEST_TIMEOUT}")
    else()
        set(test_timeout 30)
    endif()
    set(test_labels integration)
    list(APPEND test_labels ${TEST_LABELS})

    add_test(
        NAME "${TEST_NAME}"
        COMMAND "${CMAKE_COMMAND}"
            "-DCTRACE_EXECUTABLE=$<TARGET_FILE:ctrace>"
            ${TEST_ARGUMENTS}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/src/${TEST_SCRIPT}"
    )
    set_tests_properties("${TEST_NAME}" PROPERTIES
        LABELS "${test_labels}"
        TIMEOUT "${test_timeout}"
    )
endfunction()

add_test(
    NAME ctrace-version
    COMMAND ctrace --version
)

ctrace_add_integration_test(
    NAME ctrace-version-rejects-invalid-type
    SCRIPT RunExpectedFailure.cmake
    ARGUMENTS
        "-DCTRACE_ARGS=--version;--type;DWT"
        -DEXPECTED_RESULT=1
        "-DEXPECTED_STDERR=\\[error\\] Invalid --type value: DWT"
)

ctrace_add_integration_test(
    NAME ctrace-missing-trace-directory
    SCRIPT RunExpectedFailure.cmake
    ARGUMENTS
        "-DCTRACE_ARGS=${CMAKE_CURRENT_BINARY_DIR}/__missing_trace_directory_4b66b86f__"
        -DEXPECTED_RESULT=1
        "-DEXPECTED_STDERR=\\[error\\] trace directory not found:"
)

set(CTRACE_VERSION_REGEX "${PROJECT_VERSION_FULL}")
string(REPLACE "." "\\." CTRACE_VERSION_REGEX "${CTRACE_VERSION_REGEX}")
string(REPLACE "+" "\\+" CTRACE_VERSION_REGEX "${CTRACE_VERSION_REGEX}")

add_test(
    NAME ctrace-no-args-shows-help
    COMMAND ctrace
)
set_tests_properties(ctrace-no-args-shows-help PROPERTIES
    PASS_REGULAR_EXPRESSION "ctrace <trace-dir> \\[options\\]"
)

add_test(
    NAME ctrace-help
    COMMAND ctrace --help
)
set_tests_properties(ctrace-help PROPERTIES
    PASS_REGULAR_EXPRESSION "ctrace <trace-dir> \\[options\\]"
)

set_tests_properties(ctrace-version PROPERTIES
    PASS_REGULAR_EXPRESSION "^ctrace ${CTRACE_VERSION_REGEX}[\r\n]+$"
)

set_tests_properties(
    ctrace-version
    ctrace-no-args-shows-help
    ctrace-help
    PROPERTIES
        LABELS "integration"
        TIMEOUT 30
)

ctrace_add_integration_test(
    NAME ctrace-trace-run-yaml
    SCRIPT RunTraceRunYaml.cmake
    ARGUMENTS
        -DYAML_FILE=${PROJECT_SOURCE_DIR}/test/data/trace-run/Board.ctrace-run.yml
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/trace-run-yaml
        -DTARGET=Board
        -DEXPECTED_RESULT=0
        "-DEXPECTED_STDERR=aligned DWT range"
        "-DEXPECTED_STDERR_2=configured DWT data trace"
        "-DEXPECTED_STDERR_3=applied ctrace-run meta"
        "-DUNEXPECTED_STDERR=configured ITM channel"
)

ctrace_add_integration_test(
    NAME ctrace-decoder-error-all-is-generated
    SCRIPT RunTraceRunYaml.cmake
    ARGUMENTS
        -DYAML_FILE=${PROJECT_SOURCE_DIR}/test/data/trace-run/Minimal.ctrace-run.yml
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/decoder-error-all
        -DTARGET=Minimal
        -DOUTPUT_OPTION=--all
        -DRAW_DECODER_ERROR=TRUE
        -DEXPECTED_RESULT=1
        -DEXPECT_CSV=TRUE
        -DEXPECT_CTF=TRUE
        -DEXPECT_CSV_ERROR_ROW=TRUE
        "-DEXPECTED_STDERR=\\[info\\] decoded 1 events from 2 bytes"
)

ctrace_add_integration_test(
    NAME ctrace-windows-manifest
    SCRIPT ValidateWindowsManifest.cmake
    ARGUMENTS
        -DMANIFEST_FILE=${PROJECT_SOURCE_DIR}/src/ctrace.manifest
        -DMT_EXECUTABLE=${CMAKE_MT}
        -DEMBEDDED_MANIFEST_FILE=${CMAKE_CURRENT_BINARY_DIR}/ctrace-embedded.manifest
)
set(CTRACE_BLINKY_FIXTURE_DIR
    "${PROJECT_SOURCE_DIR}/test/data/Blinky+Arm")

ctrace_add_integration_test(
    NAME ctrace-fixture-blinky-swo-and-unimplemented-tb
    SCRIPT RunTraceFixture.cmake
    LABELS fixture
    ARGUMENTS
        -DFIXTURE_DIR=${CTRACE_BLINKY_FIXTURE_DIR}
        -DTARGET=Blinky+Arm
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/fixture-blinky
        -DEXPECTED_RESULT=1
)

set(CTRACE_RESET_FIXTURE_DIR
    "${PROJECT_SOURCE_DIR}/test/data/Arm-reset")

ctrace_add_integration_test(
    NAME ctrace-fixture-reset-recovery
    SCRIPT RunResetRecoveryFixture.cmake
    TIMEOUT 60
    LABELS fixture
    ARGUMENTS
        -DFIXTURE_DIR=${CTRACE_RESET_FIXTURE_DIR}
        -DTARGET=Arm
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/fixture-reset-recovery
        -DEXPECTED_RAW_SHA256=8c7ba2b90e42188517c7b793e8b7dd4030fa5455b7a38a2de15d8ca2b47995c9
)
# The 128-KiB capture excerpt preserves the reset discontinuity, recovery sync,
# and resumed DWT events while keeping the MSVC Debug test bounded.
