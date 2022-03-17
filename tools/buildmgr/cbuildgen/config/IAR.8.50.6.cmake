# This file maps the CMSIS project options to toolchain settings.
#
#   - Applies to toolchain: IAR ARM C/C++ Compiler V8.50.6

############### EDIT BELOW ###############
# Set base directory of toolchain
set(TOOLCHAIN_ROOT)
set(EXT)

############ DO NOT EDIT BELOW ###########

set(AS ${TOOLCHAIN_ROOT}/iasmarm${EXT})
set(CC ${TOOLCHAIN_ROOT}/iccarm${EXT})
set(CXX ${TOOLCHAIN_ROOT}/iccarm${EXT})
set(LD ${TOOLCHAIN_ROOT}/ilinkarm${EXT})
set(AR ${TOOLCHAIN_ROOT}/iarchive${EXT})
set(OC ${TOOLCHAIN_ROOT}/ielftool${EXT})

# Core Options

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
  if(FPU STREQUAL "SP_FPU")
    set(IAR_CPU "Cortex-M4.fp.sp")
  endif()
elseif(CPU STREQUAL "Cortex-M7")
  set(IAR_CPU "Cortex-M7")
  if(FPU STREQUAL "DP_FPU")
    set(IAR_CPU "${IAR_CPU}.fp.dp")
  elseif(FPU STREQUAL "SP_FPU")
    set(IAR_CPU "${IAR_CPU}.fp.sp")
  endif()
elseif(CPU STREQUAL "Cortex-M23")
  set(IAR_CPU "Cortex-M23")
  if(TZ STREQUAL "NO_TZ")
    set(IAR_CPU "${IAR_CPU}.no_se")
  endif()
elseif(CPU STREQUAL "Cortex-M33")
  set(IAR_CPU "Cortex-M33")
  if(FPU STREQUAL "SP_FPU")
    set(IAR_CPU "${IAR_CPU}.fp")
  endif()
  if(DSP STREQUAL "NO_DSP")
    set(IAR_CPU "${IAR_CPU}.no_dsp")
  endif()
  if(TZ STREQUAL "NO_TZ")
    set(IAR_CPU "${IAR_CPU}.no_se")
  endif()
elseif(CPU STREQUAL "Cortex-M35P")
  set(IAR_CPU "Cortex-M35P")
  if(FPU STREQUAL "SP_FPU")
    set(IAR_CPU "${IAR_CPU}.fp")
  endif()
  if(DSP STREQUAL "NO_DSP")
    set(IAR_CPU "${IAR_CPU}.no_dsp")
  endif()
  if(TZ STREQUAL "NO_TZ")
    set(IAR_CPU "${IAR_CPU}.no_se")
  endif()
endif()
if(NOT DEFINED IAR_CPU)
  message(FATAL_ERROR "Error: CPU is not supported!")
endif()

# Assembler

set(ASM_CPU "--cpu ${IAR_CPU}")
set(ASM_FLAGS)

foreach(DEFINE ${DEFINES})
  string(REPLACE "\"" "\\\"" ENTRY ${DEFINE})
  string(REGEX REPLACE "=.*" "" KEY ${ENTRY})
  if (KEY STREQUAL ENTRY)
    set(D_OPT ${KEY})
  else()
    string(REGEX REPLACE ".*=" "" VALUE ${ENTRY})
    set(D_OPT "${KEY}=${VALUE}")
  endif()
  string(APPEND ASM_DEFINES "-D${D_OPT} ")
  string(APPEND CC_DEFINES "-D ${D_OPT} ")
endforeach()

if(BYTE_ORDER STREQUAL "Little-endian")
  set(IAR_BYTE_ORDER "little")
elseif(BYTE_ORDER STREQUAL "Big-endian")
  set(IAR_BYTE_ORDER "big")
endif()

set(ASM_BYTE_ORDER "--endian ${IAR_BYTE_ORDER}")

# C Compiler

set(CC_CPU "--cpu=${IAR_CPU}")
set(CC_FLAGS "--silent")
set(CC_BYTE_ORDER "--endian=${IAR_BYTE_ORDER}")
set(_PI "-include ")

if(SECURE STREQUAL "Secure")
  set(CC_SECURE "--cmse")
endif()

# C++ Compiler

set(CXX_CPU "${CC_CPU}")
set(CXX_DEFINES "${CC_DEFINES}")
set(CXX_BYTE_ORDER "${CC_BYTE_ORDER}")
set(CXX_SECURE "${CC_SECURE}")
set(CXX_FLAGS "${CC_FLAGS}")

# Linker

set(LD_CPU "--cpu=${IAR_CPU}")
if(LD_SCRIPT)
  set(LD_SCRIPT "--config \"${LD_SCRIPT}\"")
endif()

if(SECURE STREQUAL "Secure")
  set(LD_SECURE "--import_cmse_lib_out \"${OUT_DIR}/${TARGET}_CMSE_Lib.o\"")
endif()

set(LD_FLAGS "--silent")

# Target Output

set (LIB_PREFIX)
set (LIB_SUFFIX ".a")
set (EXE_SUFFIX ".out")

# ELF to HEX conversion
set (ELF2HEX --silent --ihex "${OUT_DIR}/${TARGET}${EXE_SUFFIX}" "${OUT_DIR}/${TARGET}.hex")

# ELF to BIN conversion
set (ELF2BIN --silent --bin "${OUT_DIR}/${TARGET}${EXE_SUFFIX}" "${OUT_DIR}/${TARGET}.bin")

# Set CMake variables for toolchain initialization
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_ASM_COMPILER "${AS}")
set(CMAKE_C_COMPILER "${CC}")
set(CMAKE_CXX_COMPILER "${CXX}")
set(CMAKE_OBJCOPY "${OC}")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMakeASM")

# Set CMake variables for skipping compiler identification
set(CMAKE_ASM_COMPILER_FORCED TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_ID "${CMAKE_C_COMPILER_ID}")
set(CMAKE_CXX_COMPILER_ID_RUN "${CMAKE_C_COMPILER_ID_RUN}")
set(CMAKE_CXX_COMPILER_VERSION "${CMAKE_C_COMPILER_VERSION}")
set(CMAKE_CXX_COMPILER_FORCED "${CMAKE_C_COMPILER_FORCED}")
set(CMAKE_CXX_COMPILER_WORKS "${CMAKE_C_COMPILER_WORKS}")
