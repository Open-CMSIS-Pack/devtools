# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

set(ctrace_command)
if(DEFINED CTRACE_EMULATOR AND NOT CTRACE_EMULATOR STREQUAL "")
    string(REPLACE "|" ";" ctrace_emulator "${CTRACE_EMULATOR}")
    list(APPEND ctrace_command ${ctrace_emulator})
endif()
list(APPEND ctrace_command "${CTRACE_EXECUTABLE}")
