/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRUTILS_H
#define PROJMGRUTILS_H

#include "ProjMgrKernel.h"
#include "ProjMgrParser.h"

#include "RteConstants.h"

 /**
 * @brief vector of ConnectItem pointers
 */
typedef std::vector<const ConnectItem*> ConnectPtrVec;

/**
 * @brief connections collection item containing
 *        filename pointer
 *        layer type pointer
 *        vector of ConnectItem pointers
 *        copy assignment operator
 *        default copy constructor
*/
struct ConnectionsCollection {
  const std::string& filename;
  const std::string& type;
  ConnectPtrVec connections;
  ConnectionsCollection& operator=(const ConnectionsCollection& c) { return *this; };
  ConnectionsCollection(const ConnectionsCollection& c) = default;
};

/**
 * @brief output type item containing
 *        on boolean
 *        string filename
*/
struct OutputType {
  bool on;
  std::string filename;
};

/**
 * @brief output types item containing
 *        bin output type
 *        elf output type
 *        hex output type
 *        lib output type
 *        cmse output type
*/
struct OutputTypes {
  OutputType bin;
  OutputType elf;
  OutputType hex;
  OutputType lib;
  OutputType cmse;
};

/**
 * @brief vector of ConnectionsCollection
*/
typedef std::vector<ConnectionsCollection> ConnectionsCollectionVec;

/**
 * @brief map of ConnectionsCollection
*/
typedef std::map<std::string, ConnectionsCollectionVec> ConnectionsCollectionMap;

/**
  * @brief string pair
 */
typedef std::pair<std::string, std::string> StrPair;

/**
 * @brief string vector
*/
typedef std::vector<std::string> StrVec;

/**
 * @brief string set
*/
typedef std::set<std::string> StrSet;

/**
 * @brief vector of string pair
*/
typedef std::vector<StrPair> StrPairVec;

/**
 * @brief vector of string pair pointer
*/
typedef std::vector<const StrPair*> StrPairPtrVec;

/**
 * @brief map of vector of string pair
*/
typedef std::map<std::string, StrPairVec> StrPairVecMap;

/**
 * @brief map of string vector
*/
typedef std::map<std::string, StrVec> StrVecMap;

/**
 * @brief map of int
*/
typedef std::map<std::string, int> IntMap;

/**
 * @brief map of bool
*/
typedef std::map<std::string, bool> BoolMap;

/**
 * @brief map of string
*/
typedef std::map<std::string, std::string> StrMap;

/**
 * @brief project manager utility class
*/
class ProjMgrUtils {

protected:
  /**
   * @brief protected class constructor to avoid instantiation
  */
  ProjMgrUtils() {};

public:

  typedef std::pair<std::string, int> Result;

  /**
   * @brief output types
  */
  static constexpr const char* OUTPUT_TYPE_BIN = "bin";
  static constexpr const char* OUTPUT_TYPE_ELF = "elf";
  static constexpr const char* OUTPUT_TYPE_HEX = "hex";
  static constexpr const char* OUTPUT_TYPE_LIB = "lib";
  static constexpr const char* OUTPUT_TYPE_CMSE = "cmse-lib";

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

  static constexpr const char* AS_SOLUTION_DIR = "SolutionDir";
  static constexpr const char* AS_PROJECT_DIR = "ProjectDir";
  static constexpr const char* AS_OUT_DIR = "OutDir";
  static constexpr const char* AS_BIN = OUTPUT_TYPE_BIN;
  static constexpr const char* AS_ELF = OUTPUT_TYPE_ELF;
  static constexpr const char* AS_HEX = OUTPUT_TYPE_HEX;
  static constexpr const char* AS_LIB = OUTPUT_TYPE_LIB;
  static constexpr const char* AS_CMSE = OUTPUT_TYPE_CMSE;

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
  static constexpr const char* RTE_FP_MVE = "FP_FVE";
  static constexpr const char* RTE_NO_MVE = "NO_MVE";
  static constexpr const char* RTE_ENDIAN_BIG = "Big-endian";
  static constexpr const char* RTE_ENDIAN_LITTLE = "Little-endian";
  static constexpr const char* RTE_ENDIAN_CONFIGURABLE = "Configurable";
  static constexpr const char* RTE_SECURE = "Secure";
  static constexpr const char* RTE_NON_SECURE = "Non-secure";
  static constexpr const char* RTE_TZ_DISABLED = "TZ-disabled";
  static constexpr const char* RTE_NO_TZ = "NO_TZ";
  static constexpr const char* RTE_BTI = "BTI";
  static constexpr const char* RTE_BTI_SIGNRET = "BTI_SIGNRET";
  static constexpr const char* RTE_NO_BRANCHPROT = "NO_BRANCHPROT";
  static constexpr const char* RTE_NO_PACBTI = "NO_PACBTI";

  static const StrMap DeviceAttributesKeys;
  static const StrPairVecMap DeviceAttributesValues;

  /**
   * @brief read gpdsc file
   * @param path to gpdsc file
   * @param pointer to rte generator model
   * @param reference to validation result
   * @return pointer to created RtePackage if successful, nullptr otherwise
  */
  static RtePackage* ReadGpdscFile(const std::string& gpdsc, bool& valid);

  /**
   * @brief execute shell command
   * @param cmd string shell command to be executed
   * @return command execution result <string, error_code>
  */
  static const Result ExecCommand(const std::string& cmd);

  /**
   * @brief get file category according to file extension
   * @param filename with extension
   * @return string category
  */
  static const std::string GetCategory(const std::string& file);

  /**
   * @brief add a value into a vector/list if it does not already exist in the vector/list
   * @param vec/list the vector/list to add the value into
   * @param value the value to add
  */
  static void PushBackUniquely(std::vector<std::string>& vec, const std::string& value);
  static void PushBackUniquely(std::list<std::string>& lst, const std::string& value);
  static void PushBackUniquely(StrPairVec& vec, const StrPair& value);

  /**
   * @brief merge two string vector maps
   * @param map1 first string vector map
   * @param map2 second string vector map
   * @return StrVecMap merged map
  */
 static StrVecMap MergeStrVecMap(const StrVecMap& map1, const StrVecMap& map2);

  /**
   * @brief convert string to int, return 0 if it's empty or not convertible
   * @param string
   * @return int
  */
  static int StringToInt(const std::string& value);

  /**
   * @brief expand compiler id the format <name>@[>=]<version> into name, minimum and maximum versions
   * @param compiler id
   * @param name reference to compiler name
   * @param minVer reference to compiler minimum version
   * @param maxVer reference to compiler maximum version
  */
  static void ExpandCompilerId(const  std::string& compiler, std::string& name, std::string& minVer, std::string& maxVer);

  /**
   * @brief check if compilers are compatible in the format <name>@[>=]<version>
   * @param first compiler id
   * @param second compiler id
   * @return true if compilers are compatible, false otherwise
  */
  static bool AreCompilersCompatible(const std::string& first, const std::string& second);

  /**
   * @brief get compilers version range intersection in the format <name>@[>=]<version>
   * @param first compiler id
   * @param second compiler id
   * @param intersection reference to intersection id
  */
  static void CompilersIntersect(const std::string& first, const std::string& second, std::string& intersection);

  /**
   * @brief get compiler root
   * @param compilerRoot reference
  */
  static void GetCompilerRoot(std::string& compilerRoot);

  /**
   * @brief parse context entry
   * @param contextEntry string in the format <project-name>.<build-type>+<target-type>
   * @param context output structure with project name, build type and target type strings
   * @return true if parse successfully, false otherwise
  */
  static bool ParseContextEntry(const std::string& contextEntry, ContextName& context);

  /**
   * @brief set output type
   * @param typeString string with output type
   * @param type reference to OutputTypes structure
  */
  static void SetOutputType(const std::string typeString, OutputTypes& type);

  struct Error {
    Error(std::string errMsg = RteUtils::EMPTY_STRING) {
      m_errMsg = errMsg;
    }
    std::string m_errMsg;
    operator bool() const { return (m_errMsg != RteUtils::EMPTY_STRING); }
  };

  /**
   * @brief get selected list of contexts
   * @param selectedContexts list of matched contexts
   * @param allAvailableContexts list of all available contexts
   * @param contextFilter filter criteria
   * @return list of filters for which match was not found, else empty list
  */
  static Error GetSelectedContexts(
    std::vector<std::string>& selectedContexts,
    const std::vector<std::string>& allAvailableContexts,
    const std::vector<std::string>& contextFilters);

  /**
   * @brief replace list of contexts
   * @param selectedContexts list of matched contexts
   * @param allContexts list of all available contexts
   * @param contextReplace filter criteria
   * @return Error object with error message (if any)
  */
  static Error ReplaceContexts(
    std::vector<std::string>& selectedContexts,
    const std::vector<std::string>& allContexts,
    const std::string& contextReplace);

  /**
   * @brief get equivalent device attribute
   * @param key device attribute rte key
   * @param value device attribute value (rte or yaml)
   * @return rte or yaml equivalent device value
  */
  static const std::string& GetDeviceAttribute(const std::string& key, const std::string& value);

protected:
  static std::string ConstructID(const std::vector<std::pair<const char*, const std::string&>>& elements);
  /**
   * @brief get filtered list of contexts
   * @param allContexts list of all available contexts
   * @param contextFilter filter criteria
   * @return list of filters for which match was not found, else empty list
  */
  static std::vector<std::string> GetFilteredContexts(
    const std::vector<std::string>& allContexts,
    const std::string& contextFilter);
};

#endif  // PROJMGRUTILS_H
