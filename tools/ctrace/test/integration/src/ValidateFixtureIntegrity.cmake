# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED FIXTURE_ROOT)
  message(FATAL_ERROR "FIXTURE_ROOT is required")
endif()

function(check_fixture relative_path expected_sha256 expected_size)
  set(path "${FIXTURE_ROOT}/${relative_path}")
  if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
    message(FATAL_ERROR "missing fixture: ${relative_path}")
  endif()

  file(SHA256 "${path}" actual_sha256)
  if(NOT actual_sha256 STREQUAL expected_sha256)
    message(FATAL_ERROR
      "fixture SHA-256 differs for ${relative_path}: expected ${expected_sha256}, got ${actual_sha256}")
  endif()

  file(SIZE "${path}" actual_size)
  if(NOT actual_size EQUAL expected_size)
    message(FATAL_ERROR
      "fixture size differs for ${relative_path}: expected ${expected_size}, got ${actual_size}")
  endif()
endfunction()

# Keep this manifest in source control with the reviewed fixture. It is deliberately
# independent of host checksum utilities and verifies both identity and expected size.
# Markdown documentation does not affect fixture identity and is excluded.
set(fixture_entries
  "Arm-reset/Arm.SWO.raw|8c7ba2b90e42188517c7b793e8b7dd4030fa5455b7a38a2de15d8ca2b47995c9|131071"
  "Arm-reset/Arm.ctrace-run.yml|56ad6c463f4388338305218474bfee3a69201bf1b94a0391ba8ccc06d79889fb|691"
  "Blinky+Arm/Blinky+Arm.SWO.csv|9951a4d21680b51640c32f0ca82abf0d97b09072060883e1db569a2513647a49|21533"
  "Blinky+Arm/Blinky+Arm.SWO.raw|f2de14241242697fa0948f1878850cce81575c404233c5c135aa68fc582dc72c|12288"
  "Blinky+Arm/Blinky+Arm.TB.raw|b0fccabe1a326ffe9fadf12d5c3a205d87628985e5e75a99da23c97d7f33d13b|4096"
  "Blinky+Arm/Blinky+Arm.ctrace-run.yml|f15a4a11c382c66ebc371dd8cd826187daed1995ff41a7430247e065d0790bb2|1149"
  "Blinky+Arm/expected/Blinky+Arm.traceanalysis.xml|905fc1f458f87fe988aff698936d4b39b063770e4e2570ce5643f89996fa8655|6802"
  "Blinky+Arm/expected/Blinky+Arm.ctf/metadata|2769285fcf6cc9731cebc317ace247c29bf32403c7593d877a4c5956ded2cd69|8098"
  "Blinky+Arm/expected/Blinky+Arm.ctf/stream_0|2054d43163cf1ff8e921be92b397469e2eb75fb55f4f81c08c20382f38918ef6|65536"
  "TB-Trace/Blinky+Arm.TB.raw|aab49e56a07783b984fa7c6faeea101a51141423e66ba043dbd8d30702012639|4096"
  "TB-Trace/Blinky+Arm.ctrace-run.yml|74d7177f2d62a36a6079c2576506c3754ce961d1b860fe1f6c675f04a8464111|3009"
  "TB-Trace/regenerate_tb_trace.py|8ce6ca54cedc216c04a03587b8388003a8ab0563e6c79c39ebf436d9bfcd0050|10687"
  "TB-Trace/split_tb_trace.py|0ce65b99a2c51b2978cf0b790c653f5172715a1fa86521da8088fbd851ef9347|5361"
  "formatted-synthetic/Synthetic.ctrace-run.yml|7a4980237b850f471e80b5a30ff7076095514ce6f94e7171b4506d8e6d305047|2029"
  "trace-event/trace-event.ctrace-run.yml|a7b924d89854ac85e2751d1297ec78783fa12cb3fa54f5638691dd48d546a34e|89"
  "trace-event/trace-event.raw|97807dad2f69b1274df8960d3459426d1da4a6892d05e7623f3e16f06c5d85c8|19999"
  "trace-match/trace-match.ctrace-run.yml|6d3dcb3a46fe96a7202e61ab6c8f62895e7a26d42431b63c64dbbaf7adbe6571|1100"
  "trace-match/trace-match.raw|5cffb5803675dc02ecd5ed4939a42c660ad7cabd3542b8ca1506230e20d14a50|18"
  "trace-pc-sample/trace-pc-sample.ctrace-run.yml|e8875a9c5ec034ad4fae9ac823a3d442bbb4246af1ddd8d62093496bb8c52e1b|173"
  "trace-pc-sample/trace-pc-sample.raw|4e75b75e27be0dab3bdfdecb6331d7bfc1d51337d2fcad8ea2adfe5026992ad4|24"
  "trace-run/Board.ctrace-run.yml|bc7d073e12c313dbb259d293ca597d0e0b6e4c41f9ada59eb5263726693bfa00|431"
  "trace-run/Minimal.ctrace-run.yml|12af0fdf3d19abfda197253fc60897f212e13f8c649d1d93ca9798019333b696|89"
)

set(expected_fixture_paths)
foreach(entry IN LISTS fixture_entries)
  string(REPLACE "|" ";" values "${entry}")
  list(GET values 0 relative_path)
  list(GET values 1 expected_sha256)
  list(GET values 2 expected_size)
  check_fixture("${relative_path}" "${expected_sha256}" "${expected_size}")
  list(APPEND expected_fixture_paths "${relative_path}")
endforeach()

file(GLOB_RECURSE actual_fixture_paths RELATIVE "${FIXTURE_ROOT}" LIST_DIRECTORIES false
  "${FIXTURE_ROOT}/*")
list(FILTER actual_fixture_paths EXCLUDE REGEX "(^|/)(\\.DS_Store|\\.gitattributes)$")
list(FILTER actual_fixture_paths EXCLUDE REGEX "\\.md$")
list(SORT actual_fixture_paths)
list(SORT expected_fixture_paths)
if(NOT actual_fixture_paths STREQUAL expected_fixture_paths)
  message(FATAL_ERROR
    "fixture manifest does not match the checked-in fixture set. Expected: ${expected_fixture_paths}; actual: ${actual_fixture_paths}")
endif()

# The reconstructed Trace Bus input is exactly 256 memory-aligned frames. The
# source-ID/payload counters are additionally enforced by its regeneration tool.
file(SIZE "${FIXTURE_ROOT}/TB-Trace/Blinky+Arm.TB.raw" formatted_size)
math(EXPR formatted_remainder "${formatted_size} % 16")
math(EXPR formatted_frames "${formatted_size} / 16")
if(NOT formatted_remainder EQUAL 0 OR NOT formatted_frames EQUAL 256)
  message(FATAL_ERROR
    "TB-Trace capture must contain exactly 256 complete 16-byte formatter frames, got ${formatted_size} bytes")
endif()
