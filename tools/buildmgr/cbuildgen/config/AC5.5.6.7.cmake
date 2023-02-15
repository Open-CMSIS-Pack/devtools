# This file maps the CMSIS project options to toolchain settings.
#
#   - Applies to toolchain: ARM Compiler 5.06 update 7 (build 960) and greater

############### EDIT BELOW ###############
# Set base directory of toolchain
set(TOOLCHAIN_ROOT)
set(TOOLCHAIN_VERSION "5.6.7")

############ DO NOT EDIT BELOW ###########

set(AS "armasm")
set(CC "armcc")
set(CXX "armcc")
set(OC "fromelf")

if(DEFINED REGISTERED_TOOLCHAIN_ROOT)
  set(TOOLCHAIN_ROOT "${REGISTERED_TOOLCHAIN_ROOT}")
endif()
if(DEFINED REGISTERED_TOOLCHAIN_VERSION)
  set(TOOLCHAIN_VERSION "${REGISTERED_TOOLCHAIN_VERSION}")
endif()

if(DEFINED TOOLCHAIN_ROOT AND NOT TOOLCHAIN_ROOT STREQUAL "")
  set(EXT ".exe")
  set(AS ${TOOLCHAIN_ROOT}/${AS}${EXT})
  set(CC ${TOOLCHAIN_ROOT}/${CC}${EXT})
  set(CXX ${TOOLCHAIN_ROOT}/${CXX}${EXT})
  set(OC ${TOOLCHAIN_ROOT}/${OC}${EXT})
endif()

# Helpers

function(cbuild_set_defines lang defines)
  set(TMP_DEFINES)
  foreach(DEFINE ${${defines}})
    string(REPLACE "\"" "\\\"" ENTRY ${DEFINE})
    string(REGEX REPLACE "=.*" "" KEY ${ENTRY})
    if (KEY STREQUAL ENTRY)
      set(VALUE "1")
    else()
      string(REGEX REPLACE ".*=" "" VALUE ${ENTRY})
    endif()
    if(${lang} STREQUAL "ASM")
      if (VALUE MATCHES "^0x[0-9a-fA-F]+|^[0-9]+$")
        set(SETX "SETA")
      else()
        set(SETX "SETS")
      endif()
      string(APPEND TMP_DEFINES "--pd \"${KEY} ${SETX} ${VALUE}\" ")
    else()
      string(APPEND TMP_DEFINES "-D${ENTRY} ")
    endif()
  endforeach()
  set(${defines} ${TMP_DEFINES} PARENT_SCOPE)
endfunction()

set(OPTIMIZE_VALUES    "none" "balanced" "size"        "speed")
set(OPTIMIZE_CC_FLAGS  "-O0"  "-O2"      "-O3 -Ospace" "-O3 -Otime")
set(OPTIMIZE_CXX_FLAGS ${OPTIMIZE_CC_FLAGS})

set(DEBUG_VALUES       "on"      "off")
set(DEBUG_ASM_FLAGS    "--debug" "")
set(DEBUG_CC_FLAGS     "--debug" "--no_debug")
set(DEBUG_CXX_FLAGS    ${DEBUG_CC_FLAGS})
set(DEBUG_LD_FLAGS     ${DEBUG_CC_FLAGS})

set(WARNINGS_VALUES    "on" "off")
set(WARNINGS_ASM_FLAGS ""   "--no_warn")
set(WARNINGS_CC_FLAGS  ""   "-W")
set(WARNINGS_CXX_FLAGS ""   "-W")
set(WARNINGS_LD_FLAGS  ""   "--diag_suppress=warning")

function(cbuild_set_option_flags lang option value flags)
  if(NOT DEFINED ${option}_${lang}_FLAGS)
    return()
  endif()
  list(FIND ${option}_VALUES "${value}" _index)
  if (${_index} GREATER -1)
    list(GET ${option}_${lang}_FLAGS ${_index} flag)
    set(${flags} "${flag} ${${flags}}" PARENT_SCOPE)
  elseif(NOT value STREQUAL "")
    string(TOLOWER "${option}" _option)
    message(FATAL_ERROR "unkown '${_option}' value '${value}' !")
  endif()
endfunction()

function(cbuild_set_options_flags lang optimize debug warnings flags)
  set(opt_flags)
  cbuild_set_option_flags(${lang} OPTIMIZE "${optimize}" opt_flags)
  cbuild_set_option_flags(${lang} DEBUG    "${debug}"    opt_flags)
  cbuild_set_option_flags(${lang} WARNINGS "${warnings}" opt_flags)
  set(${flags} "${opt_flags} ${${flags}}" PARENT_SCOPE)
endfunction()

# Assembler

if(CPU STREQUAL "Cortex-M0")
  set(ARMASM_CPU "--cpu=Cortex-M0")
elseif(CPU STREQUAL "Cortex-M0+")
  set(ARMASM_CPU "--cpu=Cortex-M0plus")
elseif(CPU STREQUAL "Cortex-M1")
  set(ARMASM_CPU "--cpu=Cortex-M1")
elseif(CPU STREQUAL "Cortex-M3")
  set(ARMASM_CPU "--cpu=Cortex-M3")
elseif(CPU STREQUAL "Cortex-M4")
  if(FPU STREQUAL "SP_FPU")
    set(ARMASM_CPU "--cpu=Cortex-M4.fp.sp")
  else()
    set(ARMASM_CPU "--cpu=Cortex-M4")
  endif()
elseif(CPU STREQUAL "Cortex-M7")
  if(FPU STREQUAL "DP_FPU")
    set(ARMASM_CPU "--cpu=Cortex-M7.fp.dp")
  elseif(FPU STREQUAL "SP_FPU")
    set(ARMASM_CPU "--cpu=Cortex-M7.fp.sp")
  else()
    set(ARMASM_CPU "--cpu=Cortex-M7")
  endif()
elseif(CPU STREQUAL "SC000")
  set(ARMASM_CPU "--cpu=SC000")
elseif(CPU STREQUAL "SC300")
  set(ARMASM_CPU "--cpu=SC300")
endif()
if(NOT DEFINED ARMASM_CPU)
  message(FATAL_ERROR "Error: CPU is not supported!")
endif()

set(ASM_CPU ${ARMASM_CPU})
set(ASM_FLAGS)
set(ASM_OPTIONS_FLAGS)
cbuild_set_options_flags(ASM "${OPTIMIZE}" "${DEBUG}" "${WARNINGS}" ASM_OPTIONS_FLAGS)

set(ASM_DEFINES ${DEFINES})
cbuild_set_defines(ASM ASM_DEFINES)

if(BYTE_ORDER STREQUAL "Little-endian")
  set(ASM_BYTE_ORDER "--littleend")
elseif(BYTE_ORDER STREQUAL "Big-endian")
  set(ASM_BYTE_ORDER "--bigend")
endif()

# C Compiler

set(CC_CPU "${ARMASM_CPU}")
set(CC_FLAGS)
set(CC_BYTE_ORDER ${ASM_BYTE_ORDER})
set(_PI "--preinclude=")
set(CC_OPTIONS_FLAGS)
cbuild_set_options_flags(CC "${OPTIMIZE}" "${DEBUG}" "${WARNINGS}" CC_OPTIONS_FLAGS)

set(CC_DEFINES ${DEFINES})
cbuild_set_defines(C CC_DEFINES)

# C++ Compiler

set(CXX_CPU "${CC_CPU}")
set(CXX_DEFINES "${CC_DEFINES}")
set(CXX_BYTE_ORDER "${CC_BYTE_ORDER}")
set(CXX_SECURE "${CC_SECURE}")
set(CXX_FLAGS "${CC_FLAGS}")
set(CXX_OPTIONS_FLAGS)
cbuild_set_options_flags(CXX "${OPTIMIZE}" "${DEBUG}" "${WARNINGS}" CXX_OPTIONS_FLAGS)

# Linker

set(LD_CPU ${ARMASM_CPU})
set(_LS "--scatter=")

set(LD_FLAGS "")
set(LD_OPTIONS_FLAGS)
cbuild_set_options_flags(LD "${OPTIMIZE}" "${DEBUG}" "${WARNINGS}" LD_OPTIONS_FLAGS)

# Target Output

set (LIB_PREFIX)
set (LIB_SUFFIX ".lib")
set (EXE_SUFFIX ".axf")

# ELF to HEX conversion
set (ELF2HEX --i32combined --output "${OUT_DIR}/${HEX_FILE}" "${OUT_DIR}/$<TARGET_PROPERTY:${TARGET},OUTPUT_NAME>$<TARGET_PROPERTY:${TARGET},SUFFIX>")

# ELF to BIN conversion
set (ELF2BIN --bin --output "${OUT_DIR}/${BIN_FILE}" "${OUT_DIR}/$<TARGET_PROPERTY:${TARGET},OUTPUT_NAME>$<TARGET_PROPERTY:${TARGET},SUFFIX>")

# Set CMake variables for toolchain initialization
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_ASM_COMPILER "${AS}")
set(CMAKE_C_COMPILER "${CC}")
set(CMAKE_CXX_COMPILER "${CXX}")
set(CMAKE_OBJCOPY "${OC}")
