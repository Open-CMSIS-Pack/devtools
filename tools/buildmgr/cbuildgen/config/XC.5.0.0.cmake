# This file maps the CMSIS project options to toolchain settings.
#
#   - Applies to toolchain: Microchip XC32 Compiler version 5.0.0 and greater

set(AS "xc32-gcc")
set(CC "xc32-gcc")
set(CPP "xc32-gcc")
set(CXX "xc32-g++")
set(OC "xc32-objcopy")

set(TOOLCHAIN_ROOT "${REGISTERED_TOOLCHAIN_ROOT}")
set(TOOLCHAIN_VERSION "${REGISTERED_TOOLCHAIN_VERSION}")

if(DEFINED TOOLCHAIN_ROOT)
  set(PREFIX)
  set(EXT)
  
  set(AS ${TOOLCHAIN_ROOT}/${PREFIX}${AS}${EXT})
  set(CC ${TOOLCHAIN_ROOT}/${PREFIX}${CC}${EXT})
  set(CPP ${TOOLCHAIN_ROOT}/${PREFIX}${CPP}${EXT})
  set(CXX ${TOOLCHAIN_ROOT}/${PREFIX}${CXX}${EXT})
  set(OC ${TOOLCHAIN_ROOT}/${PREFIX}${OC}${EXT})
endif()

# Helpers

function(cbuild_set_option_flags lang option value flags)
  if(NOT DEFINED ${option}_${lang}_FLAGS)
    return()
  endif()
  list(FIND ${option}_VALUES "${value}" _index)
  if (${_index} GREATER -1)
    list(GET ${option}_${lang}_FLAGS ${_index} flag)
    if(NOT flag STREQUAL "")
      string(STRIP "${flag} ${${flags}}" ${flags})
      set(${flags} "${${flags}}" PARENT_SCOPE)
    endif()
  elseif(NOT value STREQUAL "")
    string(TOLOWER "${option}" _option)
    message(FATAL_ERROR "unknown '${_option}' value '${value}' !")
  endif()
endfunction()

function(cbuild_set_options_flags lang optimize debug warnings language flags)
  set(tmp "")
  cbuild_set_option_flags(${lang} OPTIMIZE "${optimize}" tmp)
  cbuild_set_option_flags(${lang} DEBUG    "${debug}"    tmp)
  cbuild_set_option_flags(${lang} WARNINGS "${warnings}" tmp)
  if(lang STREQUAL "CC" OR lang STREQUAL "CXX")
    cbuild_set_option_flags(${lang} LANGUAGE "${language}" tmp)
  endif()
  set(${flags} "${tmp}" PARENT_SCOPE)
endfunction()

set(MDFP "")
file(GLOB_RECURSE ALL_DIRS LIST_DIRECTORIES true "${DPACK_DIR}/*")
foreach(DIR_PATH ${ALL_DIRS})
    if(IS_DIRECTORY "${DIR_PATH}")
	    get_filename_component(CURRENT_DIR_NAME "${DIR_PATH}" NAME)
	    if(CURRENT_DIR_NAME STREQUAL DNAME AND EXISTS "${DIR_PATH}/specs-${DNAME}")
		    set(MDFP "${DIR_PATH}/../../")
		    break()
	    endif()
    endif()
endforeach()

if(NOT DEFINED MDFP OR MDFP STREQUAL "")
	message(FATAL_ERROR " Error: Could not determine the DFP path for -mdfp option!!")
endif()

set(OPTIMIZE_VALUES       "debug" "none" "balanced" "size" "speed")
set(OPTIMIZE_CC_FLAGS     "-Og"   "-O0"  "-O2"      "-Os"  "-O1")
set(OPTIMIZE_CXX_FLAGS    ${OPTIMIZE_CC_FLAGS})
set(OPTIMIZE_LD_FLAGS     ${OPTIMIZE_CC_FLAGS})
set(OPTIMIZE_ASM_FLAGS    ${OPTIMIZE_CC_FLAGS})

set(DEBUG_VALUES          "on"    "off")
set(DEBUG_CC_FLAGS        "-g"    "")
set(DEBUG_CXX_FLAGS       "-g"    "")
set(DEBUG_LD_FLAGS        "-g"    "")
set(DEBUG_ASM_FLAGS       "-g"    "")

set(WARNINGS_VALUES       "on"     "off"       "all")
set(WARNINGS_CC_FLAGS     "-Wall"  "-w"        "-Wall -Wextra")
set(WARNINGS_CXX_FLAGS    "-Wall"  "-w"        "-Wall -Wextra")
set(WARNINGS_ASM_FLAGS    "-Wall"  "-w"        "-Wall -Wextra")
set(WARNINGS_LD_FLAGS     ""       ""          "")

set(LANGUAGE_VALUES       "c90"      "gnu90"      "c99"      "gnu99"      "c11"      "gnu11"       ""    ""    "c++98"      "gnu++98"      "c++03"      "gnu++03"      "c++11"      "gnu++11"      "c++14"      "gnu++14"      "c++17"      "gnu++17"      "" "" "" "" )
set(LANGUAGE_CC_FLAGS     "-std=c90" "-std=gnu90" "-std=c99" "-std=gnu99" "-std=c11" "-std=gnu11"  ""    ""    ""           ""             ""           ""             ""           ""             ""           ""             ""           ""             "" "" "" "" )
set(LANGUAGE_CXX_FLAGS    ""         ""           ""         ""           ""         ""            ""    ""    "-std=c++98" "-std=gnu++98" "-std=c++03" "-std=gnu++03" "-std=c++11" "-std=gnu++11" "-std=c++14" "-std=gnu++14" "-std=c++17" "-std=gnu++17" "" "" "" "" )

# XC32 Processor/DFP flags
set(XC32_COMMON_FLAGS "-mprocessor=${DNAME} -mdfp=${MDFP}")

set(CPP_FLAGS "-E -P ${XC32_COMMON_FLAGS} -xc")
set(CPP_DEFINES ${LD_SCRIPT_PP_DEFINES})
if(DEFINED LD_REGIONS AND NOT LD_REGIONS STREQUAL "")
  set(CPP_INCLUDES "-include \"${LD_REGIONS}\"")
endif()
set(CPP_ARGS_LD_SCRIPT "${CPP_FLAGS} ${CPP_DEFINES} ${CPP_INCLUDES} \"${LD_SCRIPT}\" -o \"${LD_SCRIPT_PP}\"")
separate_arguments(CPP_ARGS_LD_SCRIPT NATIVE_COMMAND ${CPP_ARGS_LD_SCRIPT})

# C Compiler

set(CC_OPTIONS_FLAGS "")
cbuild_set_options_flags(CC "${OPTIMIZE}" "${DEBUG}" "${WARNINGS}" "${LANGUAGE_CC}" CC_OPTIONS_FLAGS)
set(CMAKE_C_FLAGS "${XC32_COMMON_FLAGS} ${CC_OPTIONS_FLAGS}")


# C++ Compiler

set(CXX_OPTIONS_FLAGS "")
cbuild_set_options_flags(CXX "${OPTIMIZE}" "${DEBUG}" "${WARNINGS}" "${LANGUAGE_CXX}" CXX_OPTIONS_FLAGS)
set(CMAKE_CXX_FLAGS "${XC32_COMMON_FLAGS} ${CXX_OPTIONS_FLAGS}")

# Assembler

set(ASM_OPTIONS_FLAGS "")
cbuild_set_options_flags(ASM "${OPTIMIZE}" "${DEBUG}" "${WARNINGS}" "" ASM_OPTIONS_FLAGS)
set(CMAKE_ASM_FLAGS "${XC32_COMMON_FLAGS} ${ASM_OPTIONS_FLAGS}")

# Linker

set(LD_OPTIONS_FLAGS "")
cbuild_set_options_flags(LD "${OPTIMIZE}" "${DEBUG}" "${WARNINGS}" "" LD_OPTIONS_FLAGS)
set(CMAKE_EXE_LINKER_FLAGS "${XC32_COMMON_FLAGS} ${LD_OPTIONS_FLAGS} -T")

# ELF to HEX conversion
set(ELF2HEX  -O ihex   "${OUT_DIR}/$<TARGET_PROPERTY:${TARGET},OUTPUT_NAME>.elf" "${OUT_DIR}/${HEX_FILE}")

# ELF to BIN conversion
set(ELF2BIN  -O binary "${OUT_DIR}/$<TARGET_PROPERTY:${TARGET},OUTPUT_NAME>.elf" "${OUT_DIR}/${BIN_FILE}")

# Set CMake variables for toolchain initialization
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_ASM_COMPILER "${AS}")
set(CMAKE_C_COMPILER "${CC}")
set(CMAKE_CXX_COMPILER "${CXX}")
set(CMAKE_OBJCOPY "${OC}")
