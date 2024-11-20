# This file maps the CMSIS project options to toolchain settings.
#
#   - Applies to toolchain: IAR ARM C/C++ Compiler V9.32.1 and greater
#
#    V1: Initial support
#    V2: Support for 32-bit Cortex M,A and R

set(AS "iasmarm")
set(CC "iccarm")
set(CXX "iccarm")
set(LD "ilinkarm")
set(AR "iarchive")
set(CPP "iccarm")
set(OC "ielftool")

set(TOOLCHAIN_ROOT "${REGISTERED_TOOLCHAIN_ROOT}")
set(TOOLCHAIN_VERSION "${REGISTERED_TOOLCHAIN_VERSION}")

if(DEFINED TOOLCHAIN_ROOT)
  set(EXT)
  set(AS ${TOOLCHAIN_ROOT}/${AS}${EXT})
  set(CC ${TOOLCHAIN_ROOT}/${CC}${EXT})
  set(CXX ${TOOLCHAIN_ROOT}/${CXX}${EXT})
  set(LD ${TOOLCHAIN_ROOT}/${LD}${EXT})
  set(AR ${TOOLCHAIN_ROOT}/${AR}${EXT})
  set(CPP ${TOOLCHAIN_ROOT}/${CPP}${EXT})
  set(OC ${TOOLCHAIN_ROOT}/${OC}${EXT})
endif()

######## Core Options ##########
set(SUPPORTS_BRANCHPROT FALSE) # Branch protection
set(SUPPORTS_MVE FALSE)        # Vector extensions
set(SUPPORTS_DSP FALSE)        # Digital signal processing
set(SUPPORTS_TZ FALSE)         # Trust zones

function(iar_set_option_flag option_values value flags option_to_set default)
  list(FIND ${option_values} "${value}" _index)
    if (${_index} GREATER -1)
      list(GET ${flags} ${_index} flag)
      if("${flag}" STREQUAL "ERROR")
        message(FATAL_ERROR "${value} is not supported")
      endif()
      set(${option_to_set} "${flag}" PARENT_SCOPE)
    else()
      set(${option_to_set} "${default}" PARENT_SCOPE)
    endif()
endfunction()

# The list of generic values from CPRJ.xsd
set(FPU_VALUES "none" "SP_FPU" "DP_FPU" "FPU")

if(CPU STREQUAL "Cortex-M0")
  set(IAR_CPU "Cortex-M0")

elseif(CPU STREQUAL "Cortex-M0+")
  set(IAR_CPU "Cortex-M0+")

elseif(CPU STREQUAL "Cortex-M1")
  set(IAR_CPU "Cortex-M1")

elseif(CPU STREQUAL "Cortex-M3")
  set(IAR_CPU "Cortex-M3")

elseif(CPU STREQUAL "Cortex-M4")
  set(IAR_CPU "Cortex-M4")
  set(FPU_FLAGS     "none" "VFPv4_sp" "ERROR" "VFPv4_sp")

elseif(CPU STREQUAL "Cortex-M7")
  set(IAR_CPU "Cortex-M7")
  set(FPU_FLAGS     "none" "VFPv5_sp" "VFPv5_d16" "VFPv5_sp")

elseif(CPU STREQUAL "Cortex-M23")
  set(IAR_CPU      "Cortex-M23")
  set(SUPPORTS_TZ  TRUE)

elseif(CPU STREQUAL "Cortex-M33")
  set(IAR_CPU      "Cortex-M33")
  set(FPU_FLAGS    "none" "VFPv5_sp" "VFPv5_d16" "VFPv5_sp")
  set(SUPPORTS_TZ  TRUE)
  set(SUPPORTS_DSP TRUE)

elseif(CPU STREQUAL "Star-MC1")
  set(IAR_CPU      "STAR")
  set(FPU_FLAGS    "none" "VFPv5_sp" "VFPv5_d16" "VFPv5_sp")
  set(SUPPORTS_TZ  TRUE)
  set(SUPPORTS_DSP TRUE)

elseif(CPU STREQUAL "Cortex-M35P")
  set(IAR_CPU      "Cortex-M35P")
  set(FPU_FLAGS    "none" "VFPv5_sp" "VFPv5_d16" "VFPv5_sp")
  set(SUPPORTS_TZ  TRUE)
  set(SUPPORTS_DSP TRUE)

elseif(CPU STREQUAL "Cortex-M55")
  set(IAR_CPU      "Cortex-M55")
  set(FPU_FLAGS    "none" "VFPv5_d16" "VFPv5_d16" "VFPv5_d16")
  set(SUPPORTS_MVE TRUE)
  set(SUPPORTS_TZ  TRUE)

elseif(CPU STREQUAL "Cortex-M85")
  set(IAR_CPU             "Cortex-M85")
  set(FPU_FLAGS           "none" "VFPv5_d16" "VFPv5_d16" "VFPv5_d16")
  set(SUPPORTS_BRANCHPROT TRUE)
  set(SUPPORTS_MVE        TRUE)
  set(SUPPORTS_TZ         TRUE)
  set(TZ                  "TZ") # Always on for M85

elseif(CPU STREQUAL "Cortex-A5")
set(IAR_CPU             "Cortex-A5")
set(FPU_FLAGS           "none" "ERROR" "VFPv4_d16" "VFPv4_d16")

elseif(CPU STREQUAL "Cortex-A7")
set(IAR_CPU             "Cortex-A7")
set(FPU_FLAGS           "none" "ERROR" "VFPv4_d16" "VFPv4_d16")

elseif(CPU STREQUAL "Cortex-A8")
set(IAR_CPU             "Cortex-A8")
set(FPU_FLAGS           "none" "ERROR" "VFPv3_d16" "VFPv3_d16")

elseif(CPU STREQUAL "Cortex-A9")
set(IAR_CPU             "Cortex-A9.no_neon")
set(FPU_FLAGS           "none" "ERROR" "VFPv3_d16" "VFPv3_d16")

elseif(CPU STREQUAL "Cortex-A15")
set(IAR_CPU             "Cortex-A15.no_neon")
set(FPU_FLAGS           "none" "ERROR" "VFPv4_d16" "VFPv4_d16")

elseif(CPU STREQUAL "Cortex-A17")
set(IAR_CPU             "Cortex-A17.no_neon")
set(FPU_FLAGS           "none" "ERROR" "VFPv4_d16" "VFPv4_d16")

elseif(CPU STREQUAL "Cortex-R4")
set(IAR_CPU             "Cortex-R4")
set(FPU_FLAGS           "none" "ERROR" "VFPv3_d16" "VFPv3_d16")

elseif(CPU STREQUAL "Cortex-R5")
set(IAR_CPU             "Cortex-R5")
set(FPU_FLAGS           "none" "ERROR" "VFPv3_d16" "VFPv3_d16")

elseif(CPU STREQUAL "Cortex-R7")
set(IAR_CPU             "Cortex-R7")
set(FPU_FLAGS           "none" "ERROR" "VFPv3_d16" "VFPv3_d16")

elseif(CPU STREQUAL "Cortex-R8")
set(IAR_CPU             "Cortex-R8")
set(FPU_FLAGS           "none" "ERROR" "VFPv3_d16" "VFPv3_d16")

elseif(CPU EQUAL "Cortex-R52")
if("${FPU}" STREQUAL "FPU")
  set(IAR_CPU             "Cortex-R52.no_neon")
else()
  set(IAR_CPU             "Cortex-R52")
endif()
# No sp libraries yet
set(FPU_FLAGS           "none" "ERROR" "VFPv5_d16" "VFPv5")

elseif(CPU STREQUAL "Cortex-R52+")
if("${FPU}" STREQUAL "FPU")
  set(IAR_CPU             "Cortex-R52+.no_neon")
else()
  set(IAR_CPU             "Cortex-R52+")
endif()
# No sp libraries yet
set(FPU_FLAGS           "none" "ERROR" "VFPv5_d16" "VFPv5")

endif()

if(NOT DEFINED IAR_CPU)
  message(FATAL_ERROR "Error: CPU is not supported!")
endif()

iar_set_option_flag(FPU_VALUES "${FPU}" FPU_FLAGS IAR_FPU "none")

# Handle any modifiers to the cpu. DO NOT ALTER THE ORDER!
if((DSP STREQUAL "NO_DSP") AND ${SUPPORTS_DSP})
set(IAR_CPU "${IAR_CPU}.no_dsp")
endif()

if((TZ STREQUAL "NO_TZ") AND ${SUPPORTS_TZ})
set(IAR_CPU "${IAR_CPU}.no_se")
endif()

if((BRANCHPROT STREQUAL "NO_BRANCHPROT") AND ${SUPPORTS_BRANCHPROT})
  set(IAR_CPU "${IAR_CPU}.no_pacbti")
endif()

if((MVE STREQUAL "NO_MVE") AND ${SUPPORTS_MVE})
  set(IAR_CPU "${IAR_CPU}.no_mve")
endif()

if(BYTE_ORDER STREQUAL "Little-endian")
  set(IAR_BYTE_ORDER "little")
elseif(BYTE_ORDER STREQUAL "Big-endian")
  set(IAR_BYTE_ORDER "big")
endif()

# Helpers

function(cbuild_set_defines lang defines)
  set(TMP_DEFINES)
  foreach(DEFINE ${${defines}})
    string(REPLACE "\"" "\\\"" ENTRY ${DEFINE})
    string(REGEX REPLACE "=.*" "" KEY ${ENTRY})
    if (KEY STREQUAL ENTRY)
      set(D_OPT ${KEY})
    else()
      string(REGEX REPLACE ".*=" "" VALUE ${ENTRY})
      set(D_OPT "${KEY}=${VALUE}")
    endif()
    if(${lang} STREQUAL "ASM")
      string(APPEND TMP_DEFINES "-D${D_OPT} ")
    else()
      string(APPEND TMP_DEFINES "-D ${D_OPT} ")
    endif()
  endforeach()
  set(${defines} ${TMP_DEFINES} PARENT_SCOPE)
endfunction()

set(OPTIMIZE_VALUES    "debug" "none" "balanced" "size" "speed")
set(OPTIMIZE_CC_FLAGS  "-On"   "-On"  "-Oh"      "-Ohz" "-Ohs")
set(OPTIMIZE_CXX_FLAGS ${OPTIMIZE_CC_FLAGS})

set(DEBUG_VALUES       "on"      "off")
set(DEBUG_ASM_FLAGS    "-r"      "")
set(DEBUG_CC_FLAGS     "--debug" "")
set(DEBUG_CXX_FLAGS    "--debug" "")

set(WARNINGS_VALUES    "on" "off"           "all")
set(WARNINGS_ASM_FLAGS ""   "-w"            "-w+")
set(WARNINGS_CC_FLAGS  ""   "--no_warnings" "--remarks")
set(WARNINGS_CXX_FLAGS ""   "--no_warnings" "--remarks")
set(WARNINGS_LD_FLAGS  ""   "--no_warnings" "--remarks")

# IAR default "Standard C" supports C18/C11
# The option --c89 enables support for ISO 9899:1990 also known as C94, C90, C89, and ANSI C instead of "Standard C" 
set(LANGUAGE_VALUES       "c90"   "gnu90" "c99"   "gnu99" "c11" "gnu11" "c17" "c23" "c++98" "gnu++98" "c++03" "gnu++03" "c++11" "gnu++11" "c++14" "gnu++14" "c++17" "gnu++17" "c++20" "gnu++20")
set(LANGUAGE_CC_FLAGS     "--c89" "--c89" "--c89" "--c89" ""    ""      ""    ""    ""      ""        ""      ""        ""      ""        ""      ""        ""      ""        ""      ""       )
set(LANGUAGE_CXX_FLAGS    ""      ""      ""      ""      ""    ""      ""    ""    "--c89" "--c89"   "--c89" "--c89"   ""      ""        ""      ""        ""      ""        ""      ""       )

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
    message(FATAL_ERROR "unknown '${_option}' value '${value}' !")
  endif()
endfunction()

function(cbuild_set_options_flags lang optimize debug warnings language flags)
  set(opt_flags)
  cbuild_set_option_flags(${lang} OPTIMIZE "${optimize}" opt_flags)
  cbuild_set_option_flags(${lang} DEBUG    "${debug}"    opt_flags)
  cbuild_set_option_flags(${lang} WARNINGS "${warnings}" opt_flags)
  cbuild_set_option_flags(${lang} LANGUAGE "${language}" opt_flags)
  set(${flags} "${opt_flags} ${${flags}}" PARENT_SCOPE)
endfunction()

# Assembler

set(ASM_CPU "--cpu ${IAR_CPU} --fpu=${IAR_FPU}")
set(ASM_BYTE_ORDER "--endian ${IAR_BYTE_ORDER}")
set(ASM_FLAGS)
set(ASM_DEFINES ${DEFINES})
cbuild_set_defines(ASM ASM_DEFINES)
set(ASM_OPTIONS_FLAGS)
cbuild_set_options_flags(ASM "${OPTIMIZE}" "${DEBUG}" "${WARNINGS}" "" ASM_OPTIONS_FLAGS)

# C Pre-Processor

if(SECURE STREQUAL "Secure" OR SECURE STREQUAL "Secure-only")
  set(CC_SECURE "--cmse")
endif()

set(CPP_FLAGS "--cpu=${IAR_CPU} --fpu=${IAR_FPU} ${CC_SECURE}")
set(CPP_DEFINES ${LD_SCRIPT_PP_DEFINES})
cbuild_set_defines(CC CPP_DEFINES)
if(DEFINED LD_REGIONS AND NOT LD_REGIONS STREQUAL "")
  set(CPP_INCLUDES "--preinclude \"${LD_REGIONS}\"")
endif()
set(CPP_ARGS_LD_SCRIPT "${CPP_FLAGS} ${CPP_DEFINES} \"${LD_SCRIPT}\" ${CPP_INCLUDES} --preprocess=ns \"${LD_SCRIPT_PP}\"")
separate_arguments(CPP_ARGS_LD_SCRIPT NATIVE_COMMAND ${CPP_ARGS_LD_SCRIPT})

# C Compiler

set(CC_CPU "--cpu=${IAR_CPU} --fpu=${IAR_FPU}")
set(CC_BYTE_ORDER "--endian=${IAR_BYTE_ORDER}")
set(CC_FLAGS)
set(CC_DEFINES ${DEFINES})
cbuild_set_defines(CC CC_DEFINES)
set(CC_OPTIONS_FLAGS)
cbuild_set_options_flags(CC "${OPTIMIZE}" "${DEBUG}" "${WARNINGS}" "${LANGUAGE_CC}" CC_OPTIONS_FLAGS)
set(_PI "--preinclude ")

# C++ Compiler

set(CXX_CPU "${CC_CPU}")
set(CXX_BYTE_ORDER "${CC_BYTE_ORDER}")
set(CXX_FLAGS "${CC_FLAGS}")
set(CXX_OPTIONS_FLAGS)
cbuild_set_options_flags(CXX "${OPTIMIZE}" "${DEBUG}" "${WARNINGS}" "${LANGUAGE_CXX}" CXX_OPTIONS_FLAGS)

set(CXX_DEFINES "${CC_DEFINES}")
set(CXX_SECURE "${CC_SECURE}")

# Linker

set(LD_CPU "--cpu=${IAR_CPU} --fpu=${IAR_FPU}")
set(_LS "--config ")

if(SECURE STREQUAL "Secure")
  set(LD_SECURE "--import_cmse_lib_out \"${OUT_DIR}/${CMSE_LIB}\"")
endif()

set(LD_FLAGS)
set(LD_OPTIONS_FLAGS)
cbuild_set_options_flags(LD "${OPTIMIZE}" "${DEBUG}" "${WARNINGS}" "" LD_OPTIONS_FLAGS)

# ELF to HEX conversion
set (ELF2HEX --silent --ihex "${OUT_DIR}/$<TARGET_PROPERTY:${TARGET},OUTPUT_NAME>$<TARGET_PROPERTY:${TARGET},SUFFIX>" "${OUT_DIR}/${HEX_FILE}")

# ELF to BIN conversion
set (ELF2BIN --silent --bin "${OUT_DIR}/$<TARGET_PROPERTY:${TARGET},OUTPUT_NAME>$<TARGET_PROPERTY:${TARGET},SUFFIX>" "${OUT_DIR}/${BIN_FILE}")

# Linker Map file generation
set (LD_MAP --map=${OUT_DIR}/${LD_MAP_FILE})

# Set CMake variables for toolchain initialization
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_ASM_COMPILER "${AS}")
set(CMAKE_C_COMPILER "${CC}")
set(CMAKE_CXX_COMPILER "${CXX}")
set(CMAKE_OBJCOPY "${OC}")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMakeASM")

# Enable object files extension replacement
set(CMAKE_ASM_OUTPUT_EXTENSION_REPLACE 1)
set(CMAKE_C_OUTPUT_EXTENSION_REPLACE 1)
set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE 1)
