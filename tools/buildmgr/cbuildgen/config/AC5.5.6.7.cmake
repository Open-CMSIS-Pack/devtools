# This file maps the CMSIS project options to toolchain settings.
#
#   - Applies to toolchain: ARM Compiler 5.06 update 7 (build 960)

############### EDIT BELOW ###############
# Set base directory of toolchain
set(TOOLCHAIN_ROOT)
set(EXT)

############ DO NOT EDIT BELOW ###########

set(AS ${TOOLCHAIN_ROOT}/armasm${EXT})
set(CC ${TOOLCHAIN_ROOT}/armcc${EXT})
set(CXX ${TOOLCHAIN_ROOT}/armcc${EXT})
set(OC ${TOOLCHAIN_ROOT}/fromelf${EXT})

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
      if (VALUE MATCHES "^[0-9]+$")
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

set(CC_DEFINES ${DEFINES})
cbuild_set_defines(C CC_DEFINES)

# C++ Compiler

set(CXX_CPU "${CC_CPU}")
set(CXX_DEFINES "${CC_DEFINES}")
set(CXX_BYTE_ORDER "${CC_BYTE_ORDER}")
set(CXX_SECURE "${CC_SECURE}")
set(CXX_FLAGS "${CC_FLAGS}")

# Linker

set(LD_CPU ${ARMASM_CPU})
set(_LS "--scatter=")

set(LD_FLAGS "")

# Target Output

set (LIB_PREFIX)
set (LIB_SUFFIX ".lib")
set (EXE_SUFFIX ".axf")

# ELF to HEX conversion
set (ELF2HEX --i32combined --output "${OUT_DIR}/${TARGET}.hex" "${OUT_DIR}/${TARGET}${EXE_SUFFIX}")

# ELF to BIN conversion
set (ELF2BIN --bin --output "${OUT_DIR}/${TARGET}.bin" "${OUT_DIR}/${TARGET}${EXE_SUFFIX}")

# Set CMake variables for toolchain initialization
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_ASM_COMPILER "${AS}")
set(CMAKE_C_COMPILER "${CC}")
set(CMAKE_CXX_COMPILER "${CXX}")
set(CMAKE_OBJCOPY "${OC}")
set(CMAKE_ASM_COMPILER_FORCED TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

# Set CMake variables for skipping compiler identification
set(CMAKE_C_COMPILER_ID "ARMCC")
set(CMAKE_C_COMPILER_ID_RUN TRUE)
set(CMAKE_C_COMPILER_VERSION "5.6.7")
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_ID "${CMAKE_C_COMPILER_ID}")
set(CMAKE_CXX_COMPILER_ID_RUN "${CMAKE_C_COMPILER_ID_RUN}")
set(CMAKE_CXX_COMPILER_VERSION "${CMAKE_C_COMPILER_VERSION}")
set(CMAKE_CXX_COMPILER_FORCED "${CMAKE_C_COMPILER_FORCED}")
set(CMAKE_CXX_COMPILER_WORKS "${CMAKE_C_COMPILER_WORKS}")
