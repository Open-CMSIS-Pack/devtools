#ifndef RteConstants_H
#define RteConstants_H
/******************************************************************************/
/* RTE  -  CMSIS Run-Time Environment				                          */
/******************************************************************************/
/** @file  RteConstants.h
  * @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include <string>
#include <list>
#include <vector>
#include <set>
#include "CollectionUtils.h"

class RteConstants
{
private:
  RteConstants() {}; // private constructor to forbid instantiation of a utility class

public:
  // common constants
  static constexpr const char* CR   = "\r";
  static constexpr const char* LF   = "\n";
  static constexpr const char* CRLF = "\r\n";

  /**
 * @brief component and pack delimiters
*/

  static constexpr const char* AND_STR  = "&";
  static constexpr const char  AND_CHAR = '&';

  static constexpr const char* AT_STR   = "@";
  static constexpr const char  AT_CHAR  = '@';

  static constexpr const char* COLON_STR  = ":";
  static constexpr const char  COLON_CHAR = ':';

  static constexpr const char* SEMI_COLON_STR = ";";
  static constexpr const char  SEM_COLON_CHAR = ';';

  static constexpr const char* COMMA_STR  = ",";
  static constexpr const char  COMMA_CHAR = ',';

  static constexpr const char* DOT_STR = ".";
  static constexpr const char  DOT_CHAR = '.';

  static constexpr const char* OBRACE_STR = "(";
  static constexpr const char  OBRACE_CHAR= '(';

  static constexpr const char* CBRACE_STR = ")";
  static constexpr const char  CBRACE_CHAR = ')';

  static constexpr const char* OSQBRACE_STR = "[";
  static constexpr const char  OSQBRACE_CHAR = '[';

  static constexpr const char* CSQBRACE_STR = "]";
  static constexpr const char  CSQBRACE_CHAR = ']';

  static constexpr const char* SPACE_STR = " ";
  static constexpr const char  SPACE_CHAR = ' ';


  static constexpr const char* COMPONENT_DELIMITERS = ":&@";

  static constexpr const char* SUFFIX_CVENDOR = "::";

  static constexpr const char* PREFIX_CBUNDLE = "&";
  static constexpr const char  PREFIX_CBUNDLE_CHAR = *PREFIX_CBUNDLE;
  static constexpr const char* PREFIX_CGROUP = ":";
  static constexpr const char* PREFIX_CSUB = ":";
  static constexpr const char* PREFIX_CVARIANT = "&";
  static constexpr const char  PREFIX_CVARIANT_CHAR = *PREFIX_CVARIANT;
  static constexpr const char* PREFIX_CVERSION = "@";
  static constexpr const char  PREFIX_CVERSION_CHAR = *PREFIX_CVERSION;
  static constexpr const char* SUFFIX_PACK_VENDOR = "::";
  static constexpr const char* PREFIX_PACK_VERSION = "@";
  static constexpr const char  PREFIX_PACK_VERSION_CHAR = '@';

  /**
   * @brief output types
  */
  static constexpr const char* OUTPUT_TYPE_BIN = "bin";
  static constexpr const char* OUTPUT_TYPE_ELF = "elf";
  static constexpr const char* OUTPUT_TYPE_HEX = "hex";
  static constexpr const char* OUTPUT_TYPE_LIB = "lib";
  static constexpr const char* OUTPUT_TYPE_CMSE = "cmse-lib";
  static constexpr const char* OUTPUT_TYPE_MAP = "map";

  /**
   * @brief access sequences
  */
  static constexpr const char* AS_SOLUTION = "Solution";
  static constexpr const char* AS_PROJECT = "Project";
  static constexpr const char* AS_COMPILER = "Compiler";
  static constexpr const char* AS_BUILD_TYPE = "BuildType";
  static constexpr const char* AS_TARGET_TYPE = "TargetType";
  static constexpr const char* AS_DNAME = "Dname";
  static constexpr const char* AS_PNAME = "Pname";
  static constexpr const char* AS_BNAME = "Bname";
  static constexpr const char* AS_DPACK = "Dpack";
  static constexpr const char* AS_BPACK = "Bpack";
  

  static constexpr const char* AS_SOLUTION_DIR = "SolutionDir";
  static constexpr const char* AS_PROJECT_DIR = "ProjectDir";
  static constexpr const char* AS_SOLUTION_DIR_BR = "SolutionDir()";
  static constexpr const char* AS_PROJECT_DIR_BR = "ProjectDir()";
  static constexpr const char* AS_OUT_DIR = "OutDir";
  static constexpr const char* AS_PACK_DIR = "Pack";
  static constexpr const char* AS_BIN = OUTPUT_TYPE_BIN;
  static constexpr const char* AS_ELF = OUTPUT_TYPE_ELF;
  static constexpr const char* AS_HEX = OUTPUT_TYPE_HEX;
  static constexpr const char* AS_LIB = OUTPUT_TYPE_LIB;
  static constexpr const char* AS_CMSE = OUTPUT_TYPE_CMSE;
  static constexpr const char* AS_MAP = OUTPUT_TYPE_MAP;

  /**
   * @brief default and toolchain specific output affixes
  */
  static constexpr const char* DEFAULT_ELF_SUFFIX = ".elf";
  static constexpr const char* DEFAULT_LIB_PREFIX = "";
  static constexpr const char* DEFAULT_LIB_SUFFIX = ".a";

  static constexpr const char* AC6_ELF_SUFFIX = ".axf";
  static constexpr const char* GCC_ELF_SUFFIX = ".elf";
  static constexpr const char* IAR_ELF_SUFFIX = ".out";
  static constexpr const char* AC6_LIB_PREFIX = "";
  static constexpr const char* GCC_LIB_PREFIX = "lib";
  static constexpr const char* IAR_LIB_PREFIX = "";
  static constexpr const char* AC6_LIB_SUFFIX = ".lib";
  static constexpr const char* GCC_LIB_SUFFIX = ".a";
  static constexpr const char* IAR_LIB_SUFFIX = ".a";

  /**
   * @brief device attributes maps
  */
  static constexpr const char* YAML_FPU = "fpu";
  static constexpr const char* YAML_DSP = "dsp";
  static constexpr const char* YAML_MVE = "mve";
  static constexpr const char* YAML_ENDIAN = "endian";
  static constexpr const char* YAML_TRUSTZONE = "trustzone";
  static constexpr const char* YAML_BRANCH_PROTECTION = "branch-protection";

  static constexpr const char* YAML_ON = "on";
  static constexpr const char* YAML_OFF = "off";
  static constexpr const char* YAML_FPU_DP = "dp";
  static constexpr const char* YAML_FPU_SP = "sp";
  static constexpr const char* YAML_MVE_FP = "fp";
  static constexpr const char* YAML_MVE_INT = "int";
  static constexpr const char* YAML_ENDIAN_BIG = "big";
  static constexpr const char* YAML_ENDIAN_LITTLE = "little";
  static constexpr const char* YAML_BP_BTI = "bti";
  static constexpr const char* YAML_BP_BTI_SIGNRET = "bti-signret";
  static constexpr const char* YAML_TZ_SECURE = "secure";
  static constexpr const char* YAML_TZ_SECURE_ONLY = "secure-only";
  static constexpr const char* YAML_TZ_NON_SECURE = "non-secure";

  static constexpr const char* RTE_DFPU = "Dfpu";
  static constexpr const char* RTE_DDSP = "Ddsp";
  static constexpr const char* RTE_DMVE = "Dmve";
  static constexpr const char* RTE_DENDIAN = "Dendian";
  static constexpr const char* RTE_DSECURE = "Dsecure";
  static constexpr const char* RTE_DTZ = "Dtz";
  static constexpr const char* RTE_DBRANCHPROT = "DbranchProt";
  static constexpr const char* RTE_DPACBTI = "Dpacbti";

  static constexpr const char* RTE_DP_FPU = "DP_FPU";
  static constexpr const char* RTE_SP_FPU = "SP_FPU";
  static constexpr const char* RTE_NO_FPU = "NO_FPU";
  static constexpr const char* RTE_DSP = "DSP";
  static constexpr const char* RTE_NO_DSP = "NO_DSP";
  static constexpr const char* RTE_MVE = "MVE";
  static constexpr const char* RTE_FP_MVE = "FP_MVE";
  static constexpr const char* RTE_NO_MVE = "NO_MVE";
  static constexpr const char* RTE_ENDIAN_BIG = "Big-endian";
  static constexpr const char* RTE_ENDIAN_LITTLE = "Little-endian";
  static constexpr const char* RTE_ENDIAN_CONFIGURABLE = "Configurable";
  static constexpr const char* RTE_SECURE = "Secure";
  static constexpr const char* RTE_SECURE_ONLY = "Secure-only";
  static constexpr const char* RTE_NON_SECURE = "Non-secure";
  static constexpr const char* RTE_TZ_DISABLED = "TZ-disabled";
  static constexpr const char* RTE_NO_TZ = "NO_TZ";
  static constexpr const char* RTE_BTI = "BTI";
  static constexpr const char* RTE_BTI_SIGNRET = "BTI_SIGNRET";
  static constexpr const char* RTE_NO_BRANCHPROT = "NO_BRANCHPROT";
  static constexpr const char* RTE_NO_PACBTI = "NO_PACBTI";

  static const StrMap        DeviceAttributesKeys;
  static const StrPairVecMap DeviceAttributesValues;

  /**
   * @brief get equivalent device attribute
   * @param key device attribute rte key
   * @param value device attribute value (rte or yaml)
   * @return rte or yaml equivalent device value
  */
  static const std::string& GetDeviceAttribute(const std::string& key, const std::string& value);

};

#endif // RteConstants_H
