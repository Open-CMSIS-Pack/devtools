# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# Generated with AI

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(CTRACE_EMULATOR_ARGUMENT
    "-DCTRACE_EMULATOR=$<JOIN:$<TARGET_PROPERTY:ctrace,CROSSCOMPILING_EMULATOR>,|>")

add_test(
    NAME ctrace-version
    COMMAND ctrace --version
)

add_test(
    NAME ctrace-version-rejects-invalid-type
    COMMAND ${CMAKE_COMMAND}
        -DCTRACE_EXECUTABLE=$<TARGET_FILE:ctrace>
        "${CTRACE_EMULATOR_ARGUMENT}"
        "-DCTRACE_ARGS=--version;--type;DWT"
        "-DEXPECTED_STDERR=\\[fatal\\] cli/invalid-arguments:"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/src/RunExpectedFailure.cmake
)

add_test(
    NAME ctrace-missing-trace-directory
    COMMAND ${CMAKE_COMMAND}
        -DCTRACE_EXECUTABLE=$<TARGET_FILE:ctrace>
        "${CTRACE_EMULATOR_ARGUMENT}"
        -DCTRACE_ARGS=${CMAKE_CURRENT_BINARY_DIR}/__missing_trace_directory_4b66b86f__
        "-DEXPECTED_STDERR=\\[fatal\\] input/trace-directory-failed:"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/src/RunExpectedFailure.cmake
)

string(REPLACE "+" "\\+" CTRACE_VERSION_REGEX "${CTRACE_VERSION_FULL}")

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
    PASS_REGULAR_EXPRESSION "ctrace ${CTRACE_VERSION_REGEX}"
)

add_test(
    NAME ctrace-trace-run-yaml
    COMMAND ${CMAKE_COMMAND}
        -DCTRACE_EXECUTABLE=$<TARGET_FILE:ctrace>
        "${CTRACE_EMULATOR_ARGUMENT}"
        -DYAML_FILE=${CTRACE_SOURCE_DIR}/test/data/trace-run/Board.ctrace-run.yml
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/trace-run-yaml
        -DTARGET=Board
        -DEXPECTED_RESULT=0
        "-DEXPECTED_STDERR=aligned DWT range"
        "-DEXPECTED_STDERR_2=configured DWT data trace"
        "-DEXPECTED_STDERR_3=input/ctrace-run-meta: applied ctrace-run meta"
        "-DUNEXPECTED_STDERR=configured ITM channel|decode/ctrace-run-meta"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/src/RunTraceRunYaml.cmake
)

add_test(
    NAME ctrace-decoder-error-all-is-generated
    COMMAND ${CMAKE_COMMAND}
        -DCTRACE_EXECUTABLE=$<TARGET_FILE:ctrace>
        "${CTRACE_EMULATOR_ARGUMENT}"
        -DYAML_FILE=${CTRACE_SOURCE_DIR}/test/data/trace-run/Minimal.ctrace-run.yml
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/decoder-error-all
        -DTARGET=Minimal
        -DOUTPUT_OPTION=--all
        -DRAW_DECODER_ERROR=TRUE
        -DEXPECTED_RESULT=1
        -DEXPECT_CSV=TRUE
        -DEXPECT_CTF=TRUE
        -DEXPECT_CSV_ERROR_ROW=TRUE
        "-DEXPECTED_STDERR=summary"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/src/RunTraceRunYaml.cmake
)

add_test(
    NAME ctrace-valid-swo-generates-all-outputs
    COMMAND ${CMAKE_COMMAND}
        -DCTRACE_EXECUTABLE=$<TARGET_FILE:ctrace>
        "${CTRACE_EMULATOR_ARGUMENT}"
        -DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}
        -DYAML_FILE=${CTRACE_SOURCE_DIR}/test/data/trace-run/Board.ctrace-run.yml
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/valid-swo-all
        -DTARGET=Valid
        -P ${CMAKE_CURRENT_SOURCE_DIR}/src/RunValidTraceFixture.cmake
)
set_tests_properties(
    ctrace-valid-swo-generates-all-outputs
    PROPERTIES
        LABELS "integration;fixture"
        TIMEOUT 30
)

set(CTRACE_BLINKY_FIXTURE_DIR
    "${CTRACE_SOURCE_DIR}/test/data/Blinky+STM32H747I-EVAL")

add_test(
    NAME ctrace-fixture-blinky-swo-and-unimplemented-tb
    COMMAND ${CMAKE_COMMAND}
        -DCTRACE_EXECUTABLE=$<TARGET_FILE:ctrace>
        "${CTRACE_EMULATOR_ARGUMENT}"
        -DFIXTURE_DIR=${CTRACE_BLINKY_FIXTURE_DIR}
        -DTARGET=Blinky+STM32H747I-EVAL
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/fixture-blinky
        -DEXPECTED_RESULT=1
        -P ${CMAKE_CURRENT_SOURCE_DIR}/src/RunTraceFixture.cmake
)
set_tests_properties(
    ctrace-fixture-blinky-swo-and-unimplemented-tb
    PROPERTIES
        LABELS "integration;fixture"
        TIMEOUT 30
)
