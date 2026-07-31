# Copyright (c) 2026 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0

add_library(ctrace-model STATIC
  model/TraceSelection.cpp
)
add_library(ctrace::model ALIAS ctrace-model)
target_include_directories(ctrace-model PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/model"
)
ctrace_configure_target(ctrace-model)

add_library(ctrace-cli STATIC
  cli/CliParser.cpp
)
add_library(ctrace::cli ALIAS ctrace-cli)
target_compile_definitions(ctrace-cli PRIVATE
  CTRACE_VERSION="${CTRACE_VERSION_FULL}"
)
target_include_directories(ctrace-cli PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/cli"
)
target_link_libraries(ctrace-cli
  PUBLIC ctrace::model
  PRIVATE cxxopts
)
ctrace_configure_target(ctrace-cli)

add_library(ctrace-trace-run STATIC
  tracerun/TraceRunDiscovery.cpp
  tracerun/CtraceRunMeta.cpp
  tracerun/YmlTraceRunConfigReader.cpp
)
add_library(ctrace::trace-run ALIAS ctrace-trace-run)
target_include_directories(ctrace-trace-run PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/tracerun"
)
target_include_directories(ctrace-trace-run SYSTEM PRIVATE
  "${CMAKE_SOURCE_DIR}/external/yaml-cpp/include"
)
target_link_libraries(ctrace-trace-run
  PUBLIC ctrace::model
  PRIVATE yaml-cpp
)
ctrace_configure_target(ctrace-trace-run)

add_library(ctrace-diagnostics STATIC
  diagnostics/DiagnosticSink.cpp
  diagnostics/TraceIssueReporter.cpp
)
add_library(ctrace::diagnostics ALIAS ctrace-diagnostics)
target_include_directories(ctrace-diagnostics PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/diagnostics"
)
target_link_libraries(ctrace-diagnostics PUBLIC
  ctrace::model
)
ctrace_configure_target(ctrace-diagnostics)

add_library(ctrace-decode STATIC
  decode/CortexMPostDecoder.cpp
  decode/CortexMStreamDecoder.cpp
  decode/DecodePipeline.cpp
  decode/DwtPacketDecoder.cpp
  decode/OpenCsdErrorController.cpp
  decode/OpenCsdItmDecoder.cpp
  decode/OpenCsdItmSession.cpp
  decode/OpenCsdPacketCollector.cpp
)
add_library(ctrace::decode ALIAS ctrace-decode)
target_include_directories(ctrace-decode PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/decode"
)
target_link_libraries(ctrace-decode PUBLIC
  ctrace::model
)
# Decode headers contain OpenCSD interface types; propagate that build-time
# dependency. ctrace installs no library headers or C++ API.
target_link_libraries(ctrace-decode PUBLIC
  ctrace::opencsd
)
ctrace_configure_target(ctrace-decode)

add_library(ctrace-output STATIC
  output/csv/CsvFileOutput.cpp
  output/csv/CsvRowMapper.cpp
  output/ctf/CtfBundleOutput.cpp
  output/ctf/CtfEncoder.cpp
  output/ctf/CtfExceptionLaneTracker.cpp
  output/ctf/CtfMetadataWriter.cpp
  output/ctf/CtfStreamWriter.cpp
  output/ctf/TraceCompassXmlWriter.cpp
  output/OutputRequirements.cpp
  output/TraceOutputLifecycle.cpp
)
add_library(ctrace::output ALIAS ctrace-output)
target_include_directories(ctrace-output PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/output"
)
target_link_libraries(ctrace-output
  PUBLIC
    ctrace::diagnostics
    ctrace::model
  PRIVATE
    ctrace::trace-run
)
ctrace_configure_target(ctrace-output)

add_library(ctrace-control STATIC
  control/DecodeConsumers.cpp
  control/FileDecodeJob.cpp
  control/TraceDirectoryJob.cpp
)
add_library(ctrace::control ALIAS ctrace-control)
target_include_directories(ctrace-control PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/control"
)
target_link_libraries(ctrace-control PUBLIC
  ctrace::cli
  ctrace::diagnostics
  ctrace::model
  ctrace::output
  ctrace::trace-run
)
# FileDecodeJob.hpp exposes decoder types to build-tree consumers.
target_link_libraries(ctrace-control PUBLIC
  ctrace::decode
)
ctrace_configure_target(ctrace-control)
