# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

function(require_nonempty_file file_path description)
  if(NOT EXISTS "${file_path}" OR IS_DIRECTORY "${file_path}")
    message(FATAL_ERROR "Missing ${description}: ${file_path}")
  endif()

  file(SIZE "${file_path}" file_size)
  if(file_size EQUAL 0)
    message(FATAL_ERROR "Empty ${description}: ${file_path}")
  endif()
endfunction()

function(require_variable_contains variable_name expected description)
  string(FIND "${${variable_name}}" "${expected}" match_position)
  if(match_position EQUAL -1)
    message(FATAL_ERROR
      "Babeltrace output does not contain ${description}:\n"
      "expected: ${expected}\n"
      "output:\n${${variable_name}}")
  endif()
endfunction()

function(run_babeltrace trace_directory output_variable)
  execute_process(
    COMMAND "${BABELTRACE2_EXECUTABLE}"
      --clock-seconds
      --clock-gmt
      --no-delta
      --color=never
      "${trace_directory}"
    RESULT_VARIABLE babeltrace_result
    OUTPUT_VARIABLE babeltrace_stdout
    ERROR_VARIABLE babeltrace_stderr
  )
  if(NOT "${babeltrace_result}" STREQUAL "0")
    message(FATAL_ERROR
      "Babeltrace failed for ${trace_directory} (${babeltrace_result}):\n"
      "${babeltrace_stdout}${babeltrace_stderr}")
  endif()

  set(${output_variable} "${babeltrace_stdout}${babeltrace_stderr}" PARENT_SCOPE)
endfunction()

foreach(required_variable
    CTRACE_EXECUTABLE
    BABELTRACE2_EXECUTABLE
    FIXTURE_DIRECTORY
    BUILD_ROOT
    TEST_WORK_DIRECTORY)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "Missing -D${required_variable}=...")
  endif()
endforeach()

foreach(required_executable CTRACE_EXECUTABLE BABELTRACE2_EXECUTABLE)
  if(NOT EXISTS "${${required_executable}}" OR IS_DIRECTORY "${${required_executable}}")
    message(FATAL_ERROR "Missing executable ${required_executable}: ${${required_executable}}")
  endif()
endforeach()

execute_process(
  COMMAND "${BABELTRACE2_EXECUTABLE}" --version
  RESULT_VARIABLE version_result
  OUTPUT_VARIABLE version_stdout
  ERROR_VARIABLE version_stderr
)
if(NOT "${version_result}" STREQUAL "0")
  message(FATAL_ERROR
    "Failed to query Babeltrace version (${version_result}):\n"
    "${version_stdout}${version_stderr}")
endif()
set(version_output "${version_stdout}${version_stderr}")
string(REGEX MATCH "Babeltrace[ \t]+[^ \t\r\n]+" version_field "${version_output}")
if(NOT version_field STREQUAL "Babeltrace 2.0.5")
  message(FATAL_ERROR
    "Unsupported Babeltrace version; expected exactly 2.0.5:\n${version_output}")
endif()

get_filename_component(build_root "${BUILD_ROOT}" ABSOLUTE)
get_filename_component(test_work_directory "${TEST_WORK_DIRECTORY}" ABSOLUTE)
string(FIND "${test_work_directory}/" "${build_root}/" build_prefix_position)
if(NOT build_prefix_position EQUAL 0 OR test_work_directory STREQUAL build_root)
  message(FATAL_ERROR
    "TEST_WORK_DIRECTORY must be a child of BUILD_ROOT: "
    "work=${test_work_directory}, build=${build_root}")
endif()

set(trace_run_file "${FIXTURE_DIRECTORY}/Blinky+Arm.ctrace-run.yml")
set(raw_trace_file "${FIXTURE_DIRECTORY}/Blinky+Arm.TB.raw")
require_nonempty_file("${trace_run_file}" "TB-Trace configuration fixture")
require_nonempty_file("${raw_trace_file}" "TB-Trace raw fixture")

file(REMOVE_RECURSE "${test_work_directory}")
file(MAKE_DIRECTORY "${test_work_directory}")
file(COPY "${trace_run_file}" "${raw_trace_file}" DESTINATION "${test_work_directory}")

execute_process(
  COMMAND "${CTRACE_EXECUTABLE}" "${test_work_directory}" --target Blinky+Arm --ctf
  WORKING_DIRECTORY "${test_work_directory}"
  RESULT_VARIABLE ctrace_result
  OUTPUT_VARIABLE ctrace_stdout
  ERROR_VARIABLE ctrace_stderr
)
if(NOT "${ctrace_result}" STREQUAL "0")
  message(FATAL_ERROR
    "ctrace failed to generate the consumer fixture (${ctrace_result}):\n"
    "${ctrace_stdout}${ctrace_stderr}")
endif()

set(ctf_directory "${test_work_directory}/Blinky+Arm.TB.ctf")
set(metadata_file "${ctf_directory}/metadata")
set(stream_1_file "${ctf_directory}/stream_1")
set(stream_2_file "${ctf_directory}/stream_2")
require_nonempty_file("${metadata_file}" "generated CTF metadata")
require_nonempty_file("${stream_1_file}" "generated CTF stream 1")
require_nonempty_file("${stream_2_file}" "generated CTF stream 2")

foreach(stream_id 1 2)
  set(isolated_directory "${test_work_directory}/isolated-stream-${stream_id}")
  file(MAKE_DIRECTORY "${isolated_directory}")
  file(COPY "${metadata_file}" "${ctf_directory}/stream_${stream_id}"
    DESTINATION "${isolated_directory}")
endforeach()

run_babeltrace("${test_work_directory}/isolated-stream-1" stream_1_output)
require_variable_contains(stream_1_output
  "[0.000107204] PC_SAMPLE: { cmsis_trace_bus_id = 1, ctrace_route = ( \"CM4\" : container = 1 ) }, { cmsis_pc_sample_state = 1, cmsis_pc = [ [0] = 135269288 ]"
  "the 25729-tick PC sample scaled by the 240 MHz stream-1 clock")

run_babeltrace("${test_work_directory}/isolated-stream-2" stream_2_output)
require_variable_contains(stream_2_output
  "[0.000015839] PC_SAMPLE: { cmsis_trace_bus_id = 2, ctrace_route = ( \"CM7\" : container = 2 ) }, { cmsis_pc_sample_state = 1, cmsis_pc = [ [0] = 134261608 ]"
  "the 7603-tick PC sample scaled by the 480 MHz stream-2 clock")

set(unbound_work_directory "${test_work_directory}/unbound-route")
file(MAKE_DIRECTORY "${unbound_work_directory}")
file(COPY "${raw_trace_file}" DESTINATION "${unbound_work_directory}")
file(RENAME
  "${unbound_work_directory}/Blinky+Arm.TB.raw"
  "${unbound_work_directory}/Unbound.TB.raw")
file(WRITE "${unbound_work_directory}/Unbound.ctrace-run.yml" [=[ctrace-run:
  trace-format: formatted
  ctrace-setup:
    - timestamps:
        clock: 240000000
        itm-prescaler: 1
  ctrace-refs:
    - ctrace-ref: itm
      type: itm
      stream: 1
]=])
execute_process(
  COMMAND "${CTRACE_EXECUTABLE}" "${unbound_work_directory}" --target Unbound --ctf
  WORKING_DIRECTORY "${unbound_work_directory}"
  RESULT_VARIABLE unbound_ctrace_result
  OUTPUT_VARIABLE unbound_ctrace_stdout
  ERROR_VARIABLE unbound_ctrace_stderr
)
if(NOT "${unbound_ctrace_result}" STREQUAL "0")
  message(FATAL_ERROR
    "ctrace failed to generate the unbound-route consumer fixture (${unbound_ctrace_result}):\n"
    "${unbound_ctrace_stdout}${unbound_ctrace_stderr}")
endif()
run_babeltrace("${unbound_work_directory}/Unbound.TB.ctf" unbound_output)
require_variable_contains(unbound_output
  "[0.000107204] PC_SAMPLE: { cmsis_trace_bus_id = 1, ctrace_route = ( \"1\" : container = 1 ) }, { cmsis_pc_sample_state = 1, cmsis_pc = [ [0] = 135269288 ]"
  "the numeric CTF stream-class fallback label for an unbound route")

execute_process(
  COMMAND "${BABELTRACE2_EXECUTABLE}"
    --clock-seconds
    --clock-gmt
    --no-delta
    --color=never
    "${ctf_directory}"
  RESULT_VARIABLE bundle_result
  OUTPUT_VARIABLE bundle_stdout
  ERROR_VARIABLE bundle_stderr
)
if("${bundle_result}" STREQUAL "0")
  message(FATAL_ERROR
    "Babeltrace unexpectedly accepted streams with different clock UUIDs:\n"
    "${bundle_stdout}${bundle_stderr}")
endif()

set(bundle_output "${bundle_stdout}${bundle_stderr}")
string(TOLOWER "${bundle_output}" bundle_output_lower)
if(NOT bundle_output_lower MATCHES "different (uuid|identity)")
  message(FATAL_ERROR
    "Babeltrace rejected the multi-clock bundle for an unexpected reason "
    "(${bundle_result}):\n${bundle_output}")
endif()

# The status marker uses an additive event, not a new PC_SAMPLE array-length
# value. A real consumer must decode the following PC record without drift.
get_filename_component(fixture_root "${FIXTURE_DIRECTORY}" DIRECTORY)
set(marker_fixture_directory "${fixture_root}/trace-pc-sample")
set(marker_work_directory "${test_work_directory}/pc-sampling-markers")
require_nonempty_file("${marker_fixture_directory}/trace-pc-sample.ctrace-run.yml"
  "PC sampling marker configuration fixture")
require_nonempty_file("${marker_fixture_directory}/trace-pc-sample.raw"
  "PC sampling marker raw fixture")
file(MAKE_DIRECTORY "${marker_work_directory}")
file(COPY
  "${marker_fixture_directory}/trace-pc-sample.ctrace-run.yml"
  "${marker_fixture_directory}/trace-pc-sample.raw"
  DESTINATION "${marker_work_directory}")
file(RENAME
  "${marker_work_directory}/trace-pc-sample.raw"
  "${marker_work_directory}/trace-pc-sample.SWO.raw")
execute_process(
  COMMAND "${CTRACE_EXECUTABLE}" "${marker_work_directory}" --target trace-pc-sample --ctf --type pcsample
  WORKING_DIRECTORY "${marker_work_directory}"
  RESULT_VARIABLE marker_ctrace_result
  OUTPUT_VARIABLE marker_ctrace_stdout
  ERROR_VARIABLE marker_ctrace_stderr
)
if(NOT "${marker_ctrace_result}" STREQUAL "0")
  message(FATAL_ERROR
    "ctrace failed to generate the PC sampling marker consumer fixture (${marker_ctrace_result}):\n"
    "${marker_ctrace_stdout}${marker_ctrace_stderr}")
endif()

run_babeltrace("${marker_work_directory}/trace-pc-sample.SWO.ctf" marker_output)
string(STRIP "${marker_output}" marker_output)
string(REPLACE "\n" ";" marker_records "${marker_output}")
list(LENGTH marker_records marker_record_count)
if(NOT marker_record_count EQUAL 4)
  message(FATAL_ERROR "Expected exactly four PC sampling records, got ${marker_record_count}:\n${marker_output}")
endif()

set(expected_marker_records
  "[0.000001000] PC_SAMPLE: { cmsis_trace_bus_id = 0 }, { cmsis_pc_sample_state = 1, cmsis_pc = [ [0] = 134222388 ], cmsis_sample_flags = 2, cmsis_overflow_count = 0 }"
  "[0.000003000] PC_SAMPLE: { cmsis_trace_bus_id = 0 }, { cmsis_pc_sample_state = 0, cmsis_pc = [ ], cmsis_sample_flags = 2, cmsis_overflow_count = 0 }"
  "[0.000006000] PC_SAMPLE_PROHIBITED: { cmsis_trace_bus_id = 0 }, { cmsis_sample_flags = 2, cmsis_overflow_count = 0 }"
  "[0.000010000] PC_SAMPLE: { cmsis_trace_bus_id = 0 }, { cmsis_pc_sample_state = 1, cmsis_pc = [ [0] = 134239864 ], cmsis_sample_flags = 2, cmsis_overflow_count = 0 }"
)
foreach(index RANGE 0 3)
  list(GET marker_records ${index} actual_record)
  list(GET expected_marker_records ${index} expected_record)
  if(NOT actual_record STREQUAL expected_record)
    message(FATAL_ERROR
      "Babeltrace PC sampling record ${index} differs:\nexpected: ${expected_record}\nactual: ${actual_record}")
  endif()
endforeach()
