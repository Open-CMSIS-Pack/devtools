# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

# CTRACE_EXPECTED_BINARIES is a pipe-separated list of package-relative paths.
# CTRACE_STAGE_BINARY/CTRACE_STAGE_PATH optionally stage one local dry-run input.
foreach(required_variable CTRACE_SOURCE_DIR CTRACE_DISTRIBUTION_DIR CTRACE_EXPECTED_BINARIES)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${required_variable} must be provided")
  endif()
endforeach()

get_filename_component(CTRACE_SOURCE_DIR "${CTRACE_SOURCE_DIR}" ABSOLUTE)
get_filename_component(CTRACE_DISTRIBUTION_DIR "${CTRACE_DISTRIBUTION_DIR}" ABSOLUTE)

function(ctrace_validate_package_path relative_path)
  if(NOT relative_path MATCHES "^bin/[A-Za-z0-9._+-]+/[A-Za-z0-9._+-]+$")
    message(FATAL_ERROR "Invalid ctrace package binary path: ${relative_path}")
  endif()
endfunction()

file(MAKE_DIRECTORY
  "${CTRACE_DISTRIBUTION_DIR}/bin"
  "${CTRACE_DISTRIBUTION_DIR}/THIRD_PARTY_LICENSES"
)

if(DEFINED CTRACE_STAGE_BINARY AND NOT "${CTRACE_STAGE_BINARY}" STREQUAL "")
  if(NOT DEFINED CTRACE_STAGE_PATH OR "${CTRACE_STAGE_PATH}" STREQUAL "")
    message(FATAL_ERROR "CTRACE_STAGE_PATH must accompany CTRACE_STAGE_BINARY")
  endif()
  if(NOT EXISTS "${CTRACE_STAGE_BINARY}")
    message(FATAL_ERROR "ctrace binary to stage does not exist: ${CTRACE_STAGE_BINARY}")
  endif()
  ctrace_validate_package_path("${CTRACE_STAGE_PATH}")
  get_filename_component(stage_directory
    "${CTRACE_DISTRIBUTION_DIR}/${CTRACE_STAGE_PATH}" DIRECTORY)
  file(MAKE_DIRECTORY "${stage_directory}")
  file(COPY_FILE "${CTRACE_STAGE_BINARY}"
    "${CTRACE_DISTRIBUTION_DIR}/${CTRACE_STAGE_PATH}" ONLY_IF_DIFFERENT)
endif()

set(package_copies
  "LICENSE|LICENSE.txt"
  "tools/ctrace/docs/THIRD_PARTY_NOTICES.md|THIRD_PARTY_NOTICES.md"
  "tools/ctrace/docs/RUNTIME_COMPONENTS.md|RUNTIME_COMPONENTS.md"
  "external/cxxopts/LICENSE|THIRD_PARTY_LICENSES/cxxopts.txt"
  "external/yaml-cpp/LICENSE|THIRD_PARTY_LICENSES/yaml-cpp.txt"
  "external/OpenCSD/LICENSE|THIRD_PARTY_LICENSES/OpenCSD.txt"
  "tools/ctrace/docs/OpenCSD-NOTICE.txt|THIRD_PARTY_LICENSES/OpenCSD-NOTICE.txt"
)

foreach(package_copy IN LISTS package_copies)
  string(REPLACE "|" ";" package_copy_fields "${package_copy}")
  list(GET package_copy_fields 0 source_path)
  list(GET package_copy_fields 1 destination_path)
  if(NOT EXISTS "${CTRACE_SOURCE_DIR}/${source_path}")
    message(FATAL_ERROR "Required package input does not exist: ${source_path}")
  endif()
  file(COPY_FILE
    "${CTRACE_SOURCE_DIR}/${source_path}"
    "${CTRACE_DISTRIBUTION_DIR}/${destination_path}"
    ONLY_IF_DIFFERENT
  )
endforeach()

string(REPLACE "|" ";" expected_binaries "${CTRACE_EXPECTED_BINARIES}")
list(REMOVE_DUPLICATES expected_binaries)
list(SORT expected_binaries)
foreach(expected_binary IN LISTS expected_binaries)
  ctrace_validate_package_path("${expected_binary}")
  if(NOT EXISTS "${CTRACE_DISTRIBUTION_DIR}/${expected_binary}")
    message(FATAL_ERROR "Expected ctrace package binary is missing: ${expected_binary}")
  endif()
  if(NOT expected_binary MATCHES "\\.exe$")
    file(CHMOD "${CTRACE_DISTRIBUTION_DIR}/${expected_binary}"
      PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
    )
  endif()
endforeach()

file(GLOB_RECURSE actual_binaries
  RELATIVE "${CTRACE_DISTRIBUTION_DIR}"
  "${CTRACE_DISTRIBUTION_DIR}/bin/*"
)
list(SORT actual_binaries)
if(NOT actual_binaries STREQUAL expected_binaries)
  message(FATAL_ERROR
    "Unexpected ctrace binary inventory. Expected '${expected_binaries}', found '${actual_binaries}'"
  )
endif()

file(GLOB_RECURSE package_files
  RELATIVE "${CTRACE_DISTRIBUTION_DIR}"
  "${CTRACE_DISTRIBUTION_DIR}/bin/*"
  "${CTRACE_DISTRIBUTION_DIR}/THIRD_PARTY_LICENSES/*"
)
list(APPEND package_files
  LICENSE.txt
  RUNTIME_COMPONENTS.md
  THIRD_PARTY_NOTICES.md
)
list(SORT package_files)

set(checksum_file "${CTRACE_DISTRIBUTION_DIR}/SHA256SUMS")
file(WRITE "${checksum_file}" "")
foreach(package_file IN LISTS package_files)
  file(SHA256 "${CTRACE_DISTRIBUTION_DIR}/${package_file}" package_file_hash)
  file(APPEND "${checksum_file}" "${package_file_hash}  ${package_file}\n")
endforeach()

set(archive_path "${CTRACE_DISTRIBUTION_DIR}/ctrace.zip")
file(REMOVE "${archive_path}" "${archive_path}.sha256")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E tar cf ctrace.zip --format=zip
    bin
    LICENSE.txt
    RUNTIME_COMPONENTS.md
    THIRD_PARTY_LICENSES
    THIRD_PARTY_NOTICES.md
    SHA256SUMS
  WORKING_DIRECTORY "${CTRACE_DISTRIBUTION_DIR}"
  RESULT_VARIABLE archive_result
  ERROR_VARIABLE archive_error
)
if(NOT archive_result EQUAL 0)
  message(FATAL_ERROR "Failed to create ctrace.zip: ${archive_error}")
endif()

file(SHA256 "${archive_path}" archive_hash)
file(WRITE "${archive_path}.sha256" "${archive_hash}  ctrace.zip\n")
message(STATUS "Created verified ctrace package: ${archive_path}")
