# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED MANIFEST_FILE)
    message(FATAL_ERROR "Missing -DMANIFEST_FILE=...")
endif()
if(NOT EXISTS "${MANIFEST_FILE}")
    message(FATAL_ERROR "Missing Windows manifest: ${MANIFEST_FILE}")
endif()

file(READ "${MANIFEST_FILE}" manifest)
if(NOT manifest MATCHES
        "<ws2:longPathAware>[ \t\r\n]*true[ \t\r\n]*</ws2:longPathAware>")
    message(FATAL_ERROR
        "Windows manifest does not enable long-path awareness: ${MANIFEST_FILE}")
endif()
if(NOT manifest MATCHES
        "xmlns:ws2=\"http://schemas.microsoft.com/SMI/2016/WindowsSettings\"")
    message(FATAL_ERROR
        "Windows manifest uses no recognized long-path namespace: ${MANIFEST_FILE}")
endif()
