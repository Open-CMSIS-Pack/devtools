/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 // CMakeListsGenerator.cpp

#include "CMakeListsGenerator.h"

#include "CbuildUtils.h"
#include "ErrLog.h"

#include <fstream>
#include <sstream>
#include <string>

using namespace std;

bool CMakeListsGenerator::GenBuildCMakeLists(void) {

  // Create CMakeLists stream
  m_genfile = m_intdir + "CMakeLists" + TXTEXT;
  stringstream cmakelists;

  cmakelists << "# CMSIS Build CMakeLists generated on " << CbuildUtils::GetLocalTimestamp() << EOL << EOL;

  cmakelists << "cmake_minimum_required(VERSION 3.22)" << EOL << EOL;

  cmakelists << "# Target options" << EOL;

  cmakelists << EOL << "set(TARGET " << m_targetName << ")";
  cmakelists << EOL << "set(CPU " << m_targetCpu << ")";
  cmakelists << EOL << "set(PRJ_DIR \"" << CbuildUtils::RemoveTrailingSlash(m_projectDir) << "\")";
  cmakelists << EOL << "set(OUT_DIR \"" << CbuildUtils::RemoveTrailingSlash(m_outdir) << "\")";
  cmakelists << EOL << "set(INT_DIR \"" << CbuildUtils::RemoveTrailingSlash(m_intdir) << "\")";
  if (!m_targetFpu.empty())          cmakelists << EOL << "set(FPU " << m_targetFpu << ")";
  if (!m_targetDsp.empty())          cmakelists << EOL << "set(DSP " << m_targetDsp << ")";
  if (!m_targetTz.empty())           cmakelists << EOL << "set(TZ " << m_targetTz << ")";
  if (!m_targetSecure.empty())       cmakelists << EOL << "set(SECURE " << m_targetSecure << ")";
  if (!m_targetMve.empty())          cmakelists << EOL << "set(MVE " << m_targetMve << ")";
  if (!m_targetBranchProt.empty())   cmakelists << EOL << "set(BRANCHPROT " << m_targetBranchProt << ")";
  if (!m_byteOrder.empty())          cmakelists << EOL << "set(BYTE_ORDER " << m_byteOrder << ")";
  if (!m_optimize.empty())           cmakelists << EOL << "set(OPTIMIZE " << m_optimize << ")";
  if (!m_debug.empty())              cmakelists << EOL << "set(DEBUG " << m_debug << ")";
  if (!m_warnings.empty())           cmakelists << EOL << "set(WARNINGS " << m_warnings << ")";
  if (!m_languageC.empty())          cmakelists << EOL << "set(LANGUAGE_CC " << m_languageC << ")";
  if (!m_languageCpp.empty())        cmakelists << EOL << "set(LANGUAGE_CXX " << m_languageCpp << ")";
  if (!m_asMscGlobal.empty())        cmakelists << EOL << "set(AS_FLAGS_GLOBAL \"" << CbuildUtils::EscapeQuotes(m_asMscGlobal) << "\")";
  if (!m_ccMscGlobal.empty())        cmakelists << EOL << "set(CC_FLAGS_GLOBAL \"" << CbuildUtils::EscapeQuotes(m_ccMscGlobal) << "\")";
  if (!m_cxxMscGlobal.empty())       cmakelists << EOL << "set(CXX_FLAGS_GLOBAL \"" << CbuildUtils::EscapeQuotes(m_cxxMscGlobal) << "\")";
  // LINK, LINK-C and LINK-CPP flags
  if (!(m_linkerMscGlobal.empty() && m_linkerCMscGlobal.empty() && m_linkerCxxMscGlobal.empty())) {
    string separator = "";
    if (!m_linkerMscGlobal.empty()) separator = " ";
    if(m_cxxFilesList.empty()) {
      if (!m_linkerCMscGlobal.empty()) m_linkerMscGlobal += separator + m_linkerCMscGlobal;
    } else {
      if (!m_linkerCxxMscGlobal.empty()) m_linkerMscGlobal += separator + m_linkerCxxMscGlobal;
    }
    cmakelists << EOL << "set(LD_FLAGS_GLOBAL \"" << CbuildUtils::EscapeQuotes(m_linkerMscGlobal) << "\")";
  }
  // Linker flags libraries
  if (!(m_linkerLibsGlobal.empty())) {
    cmakelists << EOL << "set(LD_FLAGS_LIBRARIES \"" << CbuildUtils::EscapeQuotes(m_linkerLibsGlobal) << "\")";
  }

  const string linkerExt = fs::path(m_linkerScript).extension().generic_string();
  if (!m_linkerScript.empty()) {
    cmakelists << EOL << "set(LD_SCRIPT \"" << m_linkerScript << "\")";
    if (!m_linkerRegionsFile.empty()) {
      cmakelists << EOL << "set(LD_REGIONS \"" << m_linkerRegionsFile << "\")";
    }
    if ((linkerExt == SRCPPEXT) || !m_linkerRegionsFile.empty() || !m_linkerPreProcessorDefines.empty()) {
      string absLinkerScript = fs::path(m_linkerScript).filename().generic_string();
      RteFsUtils::NormalizePath(absLinkerScript, m_intdir);
      const string linkerScriptPreProcessed = (linkerExt == SRCPPEXT) ? fs::path(absLinkerScript).replace_extension("").generic_string() :
        fs::path(absLinkerScript).concat(PPEXT).generic_string();
      cmakelists << EOL << "set(LD_SCRIPT_PP \"" << linkerScriptPreProcessed << "\")";
    }
  }

  fs::path outputPath;
  if (m_outputFiles.find("elf") != m_outputFiles.end()) {
    outputPath = fs::path(StrNorm(m_outputFiles.at("elf")));
  } else if (m_outputFiles.find("lib") != m_outputFiles.end()) {
    outputPath = fs::path(StrNorm(m_outputFiles.at("lib")));
  }
  const string outExt = outputPath.extension().generic_string();
  const string outFile = outputPath.replace_extension().generic_string();

  bool lib_output = (m_outputType.compare("lib") == 0) ? true :
    (m_outputFiles.find("lib") != m_outputFiles.end() ? true : false);
  bool hex_output = m_outputFiles.find("hex") != m_outputFiles.end();
  bool bin_output = m_outputFiles.find("bin") != m_outputFiles.end();
  bool cmse_output = m_outputFiles.find("cmse-lib") != m_outputFiles.end();

  if (hex_output) {
    cmakelists << EOL << "set(HEX_FILE \"" << StrNorm(m_outputFiles.at("hex")) << "\")";
  }
  if (bin_output) {
    cmakelists << EOL << "set(BIN_FILE \"" << StrNorm(m_outputFiles.at("bin")) << "\")";
  }
  if (cmse_output) {
    cmakelists << EOL << "set(CMSE_LIB \"" << StrNorm(m_outputFiles.at("cmse-lib")) << "\")";
  }

  cmakelists << EOL << EOL;


  // Linker script pre-processor defines
  if (!m_linkerScript.empty() && !m_linkerPreProcessorDefines.empty()) {
    cmakelists << "set(LD_SCRIPT_PP_DEFINES";
    for (auto n : m_linkerPreProcessorDefines) {
      cmakelists << EOL << "  " << n;
    }
    cmakelists << EOL << ")" << EOL << EOL;
  }

  // Defines
  if (!m_definesList.empty()) {
    cmakelists << "set(DEFINES";
    for (auto n : m_definesList) {
      cmakelists << EOL << "  " << n;
    }
    cmakelists << EOL << ")" << EOL << EOL;
  }

  vector<std::map<std::string, module>*> filesLists = {
    &m_ccFilesList , &m_cxxFilesList, &m_asFilesList,
    &m_asGnuFilesList, &m_asArmclangFilesList, &m_asLegacyFilesList
  };

  // file specific defines
  bool file_specific_defines = false;
  for (auto& filesList : filesLists) {
    for (const auto& [src, file] : *filesList) {
      if (!file.defines.empty()) {
        list<string> segments;
        RteUtils::SplitString(segments, file.defines, ' ');
        cmakelists << "set(DEFINES_" << CbuildUtils::ReplaceSpacesByQuestionMarks(src);
        for (auto n : segments) {
          cmakelists << EOL << "  " << n;
        }
        cmakelists << EOL << ")" << EOL << EOL;
        file_specific_defines = true;
      }
    }
  }

  // group specific defines
  bool group_specific_defines = false;
  for (auto [group, controls] : m_groupsList) {
    if (!controls.defines.empty()) {
      for (auto& filesList : filesLists) {
        for (auto [src, file] : *filesList) {
          if ((fs::path(file.group).generic_string() == group) && (file.defines.empty())) {
            list<string> segments;
            RteUtils::SplitString(segments, controls.defines, ' ');
            cmakelists << "set(DEFINES_" << CbuildUtils::ReplaceSpacesByQuestionMarks(src);
            for (auto n : segments) {
              cmakelists << EOL << "  " << n;
            }
            cmakelists << EOL << ")" << EOL << EOL;
            group_specific_defines = true;
          }
        }
      }
    }
  }

  // file specific options (optimize, debug, warnings, languageC, languageCpp)
  bool file_specific_options = false;
  for (auto& filesList : filesLists) {
    for (const auto& [src, file] : *filesList) {
      map<string, string> file_options = {
        {"OPTIMIZE", file.optimize},
        {"DEBUG", file.debug},
        {"WARNINGS", file.warnings},
        {"LANGUAGE_CC", file.languageC},
        {"LANGUAGE_CXX", file.languageCpp},
      };
      for (const auto& [option, option_value] : file_options) {
        if (((option == "LANGUAGE_CC") && (filesList != &m_ccFilesList)) ||
          ((option == "LANGUAGE_CXX") && (filesList != &m_cxxFilesList))) {
          continue;
        }
        if (option_value.empty()) continue;
        cmakelists << "set(" << option << "_" << CbuildUtils::ReplaceSpacesByQuestionMarks(src) << " \"" << CbuildUtils::EscapeQuotes(option_value) << "\")"<< EOL;
        file_specific_options = true;
      }
    }
  }

  // group specific options (optimize, debug, warnings, languageC, languageCpp)
  bool group_specific_options = false;
  for (auto [group, controls] : m_groupsList) {
    map<string, string> group_options = {
      {"OPTIMIZE", controls.optimize},
      {"DEBUG", controls.debug},
      {"WARNINGS", controls.warnings},
      {"LANGUAGE_CC", controls.languageC},
      {"LANGUAGE_CXX", controls.languageCpp},
    };
    for (const auto& [option, group_option_value] : group_options) {
      if (group_option_value.empty()) continue;
      for (auto& filesList : filesLists) {
        if (((option == "LANGUAGE_CC") && (filesList != &m_ccFilesList)) ||
          ((option == "LANGUAGE_CXX") && (filesList != &m_cxxFilesList))) {
          continue;
        }
        for (auto [src, file] : *filesList) {
          if (fs::path(file.group).generic_string() != group) continue;
          map<string, string> file_options = {
            {"OPTIMIZE", file.optimize},
            {"DEBUG", file.debug},
            {"WARNINGS", file.warnings},
            {"LANGUAGE_CC", file.languageC},
            {"LANGUAGE_CXX", file.languageCpp},
          };
          if (!file_options.at(option).empty()) continue;
          cmakelists << "set(" << option << "_" << CbuildUtils::ReplaceSpacesByQuestionMarks(src)
            << " \"" << CbuildUtils::EscapeQuotes(group_option_value) << "\")"<< EOL;
          group_specific_options = true;
        }
      }
    }
  }

  bool specific_options = file_specific_options || group_specific_options;
  if (specific_options) cmakelists << EOL;

  // Include Paths
  if (!m_incPathsList.empty()) {
    cmakelists << "set(INC_PATHS";
    for (auto n : m_incPathsList) {
      cmakelists << EOL << "  \"" << n << "\"";
    }
    cmakelists << EOL << ")" << EOL << EOL;
  }

  // file specific includes
  bool file_specific_includes = false;
  for (auto& filesList : filesLists) {
    for (const auto& [src, file] : *filesList) {
      if (!file.includes.empty()) {
        list<string> segments;
        RteUtils::SplitString(segments, CbuildUtils::EscapeQuotes(file.includes), ' ');
        cmakelists << "set(INC_PATHS_" << CbuildUtils::ReplaceSpacesByQuestionMarks(src);
        for (auto n : segments) {
          cmakelists << EOL << "  \"" << n << "\"";
        }
        cmakelists << EOL << ")" << EOL << EOL;
        file_specific_includes = true;
      }
    }
  }

  // group specific includes
  bool group_specific_includes = false;
  for (auto [group, controls] : m_groupsList) {
    if (!controls.includes.empty()) {
      for (auto& filesList : filesLists) {
        for (auto [src, file] : *filesList) {
          if ((fs::path(file.group).generic_string() == group) && (file.includes.empty())) {
            list<string> segments;
            RteUtils::SplitString(segments, CbuildUtils::EscapeQuotes(controls.includes), ' ');
            cmakelists << "set(INC_PATHS_" << CbuildUtils::ReplaceSpacesByQuestionMarks(src);
            for (auto n : segments) {
              cmakelists << EOL << "  \"" << n << "\"";
            }
            cmakelists << EOL << ")" << EOL << EOL;
            group_specific_includes = true;
          }
        }
      }
    }
  }

  // Assembly lists prefixes
  map<string, map<string, module>> asFilesLists;

  /* Assembler and assembly syntax handling
  AS_LEG: legacy armasm or gas + Arm syntax
  AS_ARM: armclang + Arm syntax
  AS_GNU: armclang or gcc + GNU syntax
  ASM: default assembler (e.g. armclang or gcc + pre-processing) */
  asFilesLists = {{"ASM", m_asFilesList}, {"AS_LEG", m_asLegacyFilesList}, {"AS_ARM", m_asArmclangFilesList}, {"AS_GNU", m_asGnuFilesList}};

  // Source Files
  for (auto list : asFilesLists) {
    if (!list.second.empty()) {
      string prefix = list.first;
      cmakelists << "set(" << prefix << "_SRC_FILES";
      for (auto [src, _] : list.second) {
        cmakelists << EOL << "  \"" << src << "\"";
      }
      cmakelists << EOL << ")" << EOL << EOL;
    }
  }

  if (!m_ccFilesList.empty()) {
    cmakelists << "set(CC_SRC_FILES";
    for (auto [src, _] : m_ccFilesList) {
      cmakelists << EOL << "  \"" << src << "\"";
    }
    cmakelists << EOL << ")" << EOL << EOL;
  }

  if (!m_cxxFilesList.empty()) {
    cmakelists << "set(CXX_SRC_FILES";
    for (auto [src, _] : m_cxxFilesList) {
      cmakelists << EOL << "  \"" << src << "\"";
    }
    cmakelists << EOL << ")" << EOL << EOL;
  }

  // Library Files
  if (!m_libFilesList.empty()) {
    cmakelists << "set(LIB_FILES";
    for (auto n : m_libFilesList) {
      cmakelists << EOL << "  \"" << n  << "\"";
    }
    cmakelists << EOL << ")" << EOL << EOL;
  }

  // Pre-Include Global
  if (!m_preincGlobal.empty()) {
    cmakelists << "set(PRE_INC_GLOBAL";
    for (auto n : m_preincGlobal) {
      cmakelists << EOL << "  \"" << n  << "\"";
    }
    cmakelists << EOL << ")" << EOL << EOL;
  }

  // Pre-Include Local
  bool preinc_local = false;
  for (auto [group, controls] : m_groupsList) {
    if (!controls.preinc.empty()) {
      preinc_local = true;
      auto lists = {&m_ccFilesList, &m_cxxFilesList};
      for (auto list: lists) {
        for (auto [src, file] : *list) {
          if (fs::path(file.group).generic_string() == group) {
            cmakelists << "set(PRE_INC_LOCAL_" << CbuildUtils::ReplaceSpacesByQuestionMarks(src);
            for (auto it : controls.preinc) {
              cmakelists << EOL << "  \"" << it << "\"";
            }
            cmakelists << EOL << ")" << EOL << EOL;
          }
        }
      }
    }
  }

  // File specific flags
  bool as_file_specific_flags = false;
  for (auto list : asFilesLists) {
    for (auto [src, file] : list.second) {
      if (!file.flags.empty()) {
        cmakelists << "set(AS_FLAGS_" << CbuildUtils::ReplaceSpacesByQuestionMarks(src) << " \"" << CbuildUtils::EscapeQuotes(file.flags) << "\")"<< EOL;
        as_file_specific_flags = true;
      }
    }
  }
  bool cc_file_specific_flags = false;
  for (auto [src, file] : m_ccFilesList) {
    if (!file.flags.empty()) {
      cmakelists << "set(CC_FLAGS_" << CbuildUtils::ReplaceSpacesByQuestionMarks(src) << " \"" << CbuildUtils::EscapeQuotes(file.flags) << "\")"<< EOL;
      cc_file_specific_flags = true;
    }
  }
  bool cxx_file_specific_flags = false;
  for (auto [src, file] : m_cxxFilesList) {
    if (!file.flags.empty()) {
      cmakelists << "set(CXX_FLAGS_" << CbuildUtils::ReplaceSpacesByQuestionMarks(src) << " \"" << CbuildUtils::EscapeQuotes(file.flags) << "\")"<< EOL;
      cxx_file_specific_flags = true;
    }
  }

  // Group specific flags
  bool as_group_specific_flags = false;
  bool cc_group_specific_flags = false;
  bool cxx_group_specific_flags = false;
  for (auto [group, controls] : m_groupsList) {
    if (!controls.asMsc.empty()) {
       for (auto list : asFilesLists) {
         for (auto [src, file] : list.second) {
           if ((fs::path(file.group).generic_string() == group) && (file.flags.empty())) {
            cmakelists << "set(AS_FLAGS_" << CbuildUtils::ReplaceSpacesByQuestionMarks(src) << " \"" << CbuildUtils::EscapeQuotes(controls.asMsc) << "\")"<< EOL;
            as_group_specific_flags = true;
          }
        }
      }
    }
    if (!controls.ccMsc.empty()) {
      for (auto [src, file] : m_ccFilesList) {
        if ((fs::path(file.group).generic_string() == group) && (file.flags.empty())) {
          cmakelists << "set(CC_FLAGS_" << CbuildUtils::ReplaceSpacesByQuestionMarks(src) << " \"" << CbuildUtils::EscapeQuotes(controls.ccMsc) << "\")"<< EOL;
          cc_group_specific_flags = true;
        }
      }
    }
    if (!controls.cxxMsc.empty()) {
      for (auto [src, file] : m_cxxFilesList) {
        if ((fs::path(file.group).generic_string() == group) && (file.flags.empty())) {
          cmakelists << "set(CXX_FLAGS_" << CbuildUtils::ReplaceSpacesByQuestionMarks(src) << " \"" << CbuildUtils::EscapeQuotes(controls.cxxMsc) << "\")"<< EOL;
          cxx_group_specific_flags = true;
        }
      }
    }
  }

  bool asflags = as_file_specific_flags || as_group_specific_flags;
  bool ccflags = cc_file_specific_flags || cc_group_specific_flags;
  bool cxxflags = cxx_file_specific_flags || cxx_group_specific_flags;
  if (asflags || ccflags || cxxflags) {
    cmakelists << EOL;
  }

  // Toolchain config
  cmakelists << "# Toolchain config map" << EOL << EOL;
  if (!m_toolchainRegisteredRoot.empty()) {
    cmakelists << "set(REGISTERED_TOOLCHAIN_ROOT \"" << m_toolchainRegisteredRoot << "\")" << EOL;
    cmakelists << "set(REGISTERED_TOOLCHAIN_VERSION \"" << m_toolchainRegisteredVersion << "\")" << EOL;
  }
  const string& toolchainVersionMin = RteUtils::GetPrefix(m_toolchainVersion);
  if (!toolchainVersionMin.empty()) {
    cmakelists << "set(TOOLCHAIN_VERSION_MIN \"" << toolchainVersionMin << "\")" << EOL;
  }
  const string& toolchainVersionMax = RteUtils::GetSuffix(m_toolchainVersion);
  if (!toolchainVersionMax.empty()) {
    cmakelists << "set(TOOLCHAIN_VERSION_MAX \"" << toolchainVersionMax << "\")" << EOL;
  }
  cmakelists << "include (\"" << m_toolchainConfig << "\")" << EOL;
  cmakelists << "include (\"" << m_compilerRoot + "/CMSIS-Build-Utils.cmake" << "\")" << EOL << EOL;

  // Setup project
  vector<string> languages;
  for (auto list : asFilesLists) {
    if (!list.second.empty()) {
      languages.push_back(list.first);
    }
  }
  if (!m_ccFilesList.empty()) {
    languages.push_back("C");
  }
  if (!m_cxxFilesList.empty()) {
    languages.push_back("CXX");
  }
  cmakelists << "# Setup project" << EOL << EOL;
  cmakelists << "project(${TARGET} LANGUAGES";
  for (auto language : languages) {
    cmakelists << " " << language;
  }
  cmakelists << ")" << EOL << EOL;

  cmakelists << "cbuild_get_running_toolchain(TOOLCHAIN_ROOT TOOLCHAIN_VERSION " << languages.back() << ")" << EOL << EOL;

  // Set global flags
  cmakelists << "# Global Flags" << EOL << EOL;

  bool specific_defines = file_specific_defines || group_specific_defines;
  bool target_options = !m_optimize.empty() || !m_debug.empty() || !m_warnings.empty() || !m_languageC.empty() || !m_languageCpp.empty();
  for (auto list : asFilesLists) {
    if (!list.second.empty()) {
      string prefix = list.first;
      cmakelists << "set(CMAKE_" << prefix << "_FLAGS \"${" << prefix << "_CPU}";
      if (!m_byteOrder.empty()) cmakelists << " ${" << prefix << "_BYTE_ORDER}";
      if (!specific_defines && !m_definesList.empty()) cmakelists << " ${" << prefix << "_DEFINES}";
      if (!specific_options && target_options) cmakelists << " ${" << prefix << "_OPTIONS_FLAGS}";
      cmakelists << " ${" << prefix << "_FLAGS}";
      if (!asflags && !preinc_local && !m_asMscGlobal.empty()) cmakelists << " ${AS_FLAGS_GLOBAL}";
      cmakelists << "\")" << EOL;
    }
  }
  if (!m_ccFilesList.empty()) {
    cmakelists << "cbuild_get_system_includes(" <<
      (m_toolchain == "CLANG" ? "CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES" : "CC_SYS_INC_PATHS_LIST") << " CC_SYS_INC_PATHS)" << EOL;
    cmakelists << "set(CMAKE_C_FLAGS \"${CC_CPU}";
    if (!m_byteOrder.empty()) cmakelists << " ${CC_BYTE_ORDER}";
    if (!specific_defines && !m_definesList.empty()) cmakelists << " ${CC_DEFINES}";
    if (!m_targetSecure.empty()) cmakelists << " ${CC_SECURE}";
    if (!m_targetBranchProt.empty()) cmakelists << " ${CC_BRANCHPROT}";
    if (!specific_options && target_options) cmakelists << " ${CC_OPTIONS_FLAGS}";
    cmakelists << " ${CC_FLAGS}";
    if (!ccflags && !preinc_local && !m_ccMscGlobal.empty()) cmakelists << " ${CC_FLAGS_GLOBAL}";
    cmakelists << " ${CC_SYS_INC_PATHS}";
    cmakelists << "\")" << EOL;
  }
  if (!m_cxxFilesList.empty()) {
    cmakelists << "cbuild_get_system_includes(" <<
      (m_toolchain == "CLANG" ? "CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES" : "CXX_SYS_INC_PATHS_LIST") << " CXX_SYS_INC_PATHS)" << EOL; 
    cmakelists << "set(CMAKE_CXX_FLAGS \"${CXX_CPU}";
    if (!m_byteOrder.empty()) cmakelists << " ${CXX_BYTE_ORDER}";
    if (!specific_defines && !m_definesList.empty()) cmakelists << " ${CXX_DEFINES}";
    if (!m_targetSecure.empty()) cmakelists << " ${CXX_SECURE}";
    if (!m_targetBranchProt.empty()) cmakelists << " ${CXX_BRANCHPROT}";
    if (!specific_options && target_options) cmakelists << " ${CXX_OPTIONS_FLAGS}";
    cmakelists << " ${CXX_FLAGS}";
    if (!cxxflags && !preinc_local && !m_cxxMscGlobal.empty()) cmakelists << " ${CXX_FLAGS_GLOBAL}";
    cmakelists << " ${CXX_SYS_INC_PATHS}";
    cmakelists << "\")" << EOL;
  }

  // Linker flags
  cmakelists << "set(CMAKE_";
  if (!m_cxxFilesList.empty()) cmakelists << "CXX";
  else cmakelists << "C";
  cmakelists << "_LINK_FLAGS \"${LD_CPU}";
  if (!m_linkerScript.empty() && !lib_output) {
    cmakelists << " ${_LS}\\\"${LD_SCRIPT";
    if ((linkerExt == SRCPPEXT) || !m_linkerRegionsFile.empty() || !m_linkerPreProcessorDefines.empty()) {
      cmakelists << "_PP";
    }
    cmakelists << "}\\\"";
  }
  if (!m_targetSecure.empty()) cmakelists << " ${LD_SECURE}";
  if (!m_linkerMscGlobal.empty()) cmakelists << " ${LD_FLAGS_GLOBAL}";
  if (target_options) cmakelists << " ${LD_OPTIONS_FLAGS}";
  cmakelists << " ${LD_FLAGS}\")" << EOL << EOL;

  // Pre-include Global
  if (!m_preincGlobal.empty()) {
    cmakelists << "foreach(ENTRY ${PRE_INC_GLOBAL})" << EOL;
    if (!m_ccFilesList.empty()) cmakelists << "  string(APPEND CMAKE_C_FLAGS \" ${_PI}\\\"${ENTRY}\\\"\")" << EOL;
    if (!m_cxxFilesList.empty()) cmakelists << "  string(APPEND CMAKE_CXX_FLAGS \" ${_PI}\\\"${ENTRY}\\\"\")" << EOL;
    cmakelists << "endforeach()" << EOL << EOL;
  }

  bool as_special_lang = (!m_asLegacyFilesList.empty() || !m_asArmclangFilesList.empty() || !m_asGnuFilesList.empty());
  bool specific_includes = file_specific_includes || group_specific_includes;

  if (asflags || ccflags || cxxflags || as_special_lang || preinc_local) {
    // Set local flags
    cmakelists << "# Local Flags" << EOL << EOL;

    if (asflags || as_special_lang) {
      for (auto list : asFilesLists) {
        if (!list.second.empty()) {
          string lang = list.first;
          cmakelists << "foreach(SRC ${" << lang << "_SRC_FILES})" << EOL;
          if (asflags) {
            cmakelists << "  string(REPLACE \" \" \"?\" S ${SRC})" << EOL;
            cmakelists << "  if(DEFINED AS_FLAGS_${S})" << EOL;
            cmakelists << "    set(AS_FLAGS_LOCAL \"${AS_FLAGS_${S}}\")" << EOL;
            cmakelists << "  else()" << EOL;
            cmakelists << "    set(AS_FLAGS_LOCAL \"${AS_FLAGS_GLOBAL}\")" << EOL;
            cmakelists << "  endif()" << EOL;
            cmakelists << "  set_source_files_properties(${SRC} PROPERTIES COMPILE_FLAGS \"${AS_FLAGS_LOCAL}\")" << EOL;
          }
          if (as_special_lang) {
            cmakelists << "  set_source_files_properties(${SRC} PROPERTIES LANGUAGE " << lang << ")" << EOL;
          }
          cmakelists << "endforeach()" << EOL << EOL;
        }
      }
    }
    map <string, bool> flagsLang = {{"CC", !m_ccFilesList.empty() && (ccflags || preinc_local)},
                                    {"CXX", !m_cxxFilesList.empty() && (cxxflags || preinc_local)}};
    for (auto list : flagsLang) {
      if (list.second) {
        string lang = list.first;
        cmakelists << "foreach(SRC ${" << lang << "_SRC_FILES})" << EOL;
        cmakelists << "  string(REPLACE \" \" \"?\" S ${SRC})" << EOL;
        if (ccflags || cxxflags) {
          cmakelists << "  if(DEFINED " << lang << "_FLAGS_${S})" << EOL;
          cmakelists << "    set(" << lang << "_FLAGS_LOCAL \"${" << lang << "_FLAGS_${S}}\")" << EOL;
          cmakelists << "  else()" << EOL;
          cmakelists << "    set(" << lang << "_FLAGS_LOCAL \"${" << lang << "_FLAGS_GLOBAL}\")" << EOL;
          cmakelists << "  endif()" << EOL;
        } else {
          cmakelists << "  set(" << lang << "_FLAGS_LOCAL \"${" << lang << "_FLAGS_GLOBAL}\")" << EOL;
        }
        if (preinc_local) {
          cmakelists << "  if(DEFINED PRE_INC_LOCAL_${S})" << EOL;
          cmakelists << "    foreach(ENTRY ${PRE_INC_LOCAL_${S}})" << EOL;
          cmakelists << "      string(APPEND " << lang << "_FLAGS_LOCAL \" ${_PI}\\\"${ENTRY}\\\"\")" << EOL;
          cmakelists << "    endforeach()" << EOL;
          cmakelists << "  endif()" << EOL;
        }
        cmakelists << "  set_source_files_properties(${SRC} PROPERTIES COMPILE_FLAGS \"${" << lang << "_FLAGS_LOCAL}\")" << EOL;
        cmakelists << "endforeach()" << EOL << EOL;
      }
    }
  }

  // Includes, Defines and Options
  if (specific_includes || specific_defines || specific_options) {
    map<string, bool> languages = {
      {"ASM",    !m_asFilesList.empty()         },
      {"AS_LEG", !m_asLegacyFilesList.empty()   },
      {"AS_ARM", !m_asArmclangFilesList.empty() },
      {"AS_GNU", !m_asGnuFilesList.empty()      },
      {"CC",     !m_ccFilesList.empty()         },
      {"CXX",    !m_cxxFilesList.empty()        }
    };
    cmakelists << "# File Includes, Defines and Options" << EOL << EOL;
    for (const auto& [lang, present] : languages) {
      if (present) {
        cmakelists << "foreach(SRC ${" << lang << "_SRC_FILES})" << EOL;
        cmakelists << "  string(REPLACE \" \" \"?\" S ${SRC})" << EOL;
        if (specific_includes) {
          cmakelists << "  if(DEFINED INC_PATHS_${S})" << EOL;
          cmakelists << "    set(INC_PATHS_LOCAL \"${INC_PATHS_${S}}\")" << EOL;
          cmakelists << "    set_source_files_properties(${SRC} PROPERTIES INCLUDE_DIRECTORIES \"${INC_PATHS_LOCAL}\")" << EOL;
          cmakelists << "  endif()" << EOL;
        }
        if (specific_defines) {
          cmakelists << "  get_source_file_property(FILE_FLAGS ${SRC} COMPILE_FLAGS)" << EOL;
          cmakelists << "  if(FILE_FLAGS STREQUAL \"NOTFOUND\")" << EOL;
          cmakelists << "    set(FILE_FLAGS)" << EOL;
          cmakelists << "  endif()" << EOL;
          cmakelists << "  if(DEFINED DEFINES_${S})" << EOL;
          cmakelists << "    cbuild_set_defines(" << lang << " DEFINES_${S})" << EOL;
          cmakelists << "    string(APPEND FILE_FLAGS \" ${DEFINES_${S}}\")" << EOL;
          cmakelists << "  else()" << EOL;
          cmakelists << "    string(APPEND FILE_FLAGS \" ${" << lang << "_DEFINES}\")" << EOL;
          cmakelists << "  endif()" << EOL;
          cmakelists << "  set_source_files_properties(${SRC} PROPERTIES COMPILE_FLAGS \"${FILE_FLAGS}\")" << EOL;
        }
        if (specific_options) {
          cmakelists << "  foreach(OPTION OPTIMIZE DEBUG WARNINGS";
          if ((lang == "CC" || lang == "CXX")) {
            cmakelists << " LANGUAGE_" << lang;
          }
          cmakelists << ")" << EOL;
          cmakelists << "    if(DEFINED ${OPTION}_${S})" << EOL;
          cmakelists << "      set(${OPTION}_LOCAL \"${${OPTION}_${S}}\")" << EOL;
          cmakelists << "    else()" << EOL;
          cmakelists << "      set(${OPTION}_LOCAL \"${${OPTION}}\")" << EOL;
          cmakelists << "    endif()" << EOL;
          cmakelists << "  endforeach()" << EOL;
          cmakelists << "  get_source_file_property(FILE_FLAGS ${SRC} COMPILE_FLAGS)" << EOL;
          cmakelists << "  if(FILE_FLAGS STREQUAL \"NOTFOUND\")" << EOL;
          cmakelists << "    set(FILE_FLAGS)" << EOL;
          cmakelists << "  endif()" << EOL;
          cmakelists << "  cbuild_set_options_flags(" << lang << " \"${OPTIMIZE_LOCAL}\" \"${DEBUG_LOCAL}\" \"${WARNINGS_LOCAL}\" \"";
          if ((lang == "CC" || lang == "CXX")) {
            cmakelists << "${LANGUAGE_" << lang << "_LOCAL}";
          }
          cmakelists << "\" FILE_FLAGS)" << EOL;
          
          cmakelists << "  set_source_files_properties(${SRC} PROPERTIES COMPILE_FLAGS \"${FILE_FLAGS}\")" << EOL;
        }
        cmakelists << "endforeach()" << EOL << EOL;
      }
    }
  }

  // Compilation Database
  cmakelists << "# Compilation Database" << EOL << EOL;
  cmakelists << "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)" << EOL;
  cmakelists << "add_custom_target(database COMMAND ${CMAKE_COMMAND} -E copy_if_different \"${INT_DIR}/compile_commands.json\" \"${OUT_DIR}\")" << EOL << EOL;

  // Setup Target
  cmakelists << "# Setup Target" << EOL << EOL;

  if (lib_output) {
    cmakelists << "add_library(${TARGET}";
  } else {
    cmakelists << "add_executable(${TARGET}";
  }
  for (auto list : asFilesLists) {
    if (!list.second.empty()) {
      string prefix = list.first;
      cmakelists << " ${" << prefix << "_SRC_FILES}";
    }
  }
  if (!m_ccFilesList.empty()) {
    cmakelists << " ${CC_SRC_FILES}";
  }
  if (!m_cxxFilesList.empty()) {
    cmakelists << " ${CXX_SRC_FILES}";
  }
  cmakelists << ")" << EOL;

  cmakelists << "set_target_properties(${TARGET} PROPERTIES PREFIX \"\" ";
  if (!outExt.empty()) {
    cmakelists << "SUFFIX \"" << outExt << "\" ";
  }
  if (!outFile.empty()) {
    cmakelists << "OUTPUT_NAME \"" << outFile << "\")" << EOL;
  } else {
    cmakelists << "OUTPUT_NAME \"${TARGET}\")" << EOL;
  }
  if (lib_output) {
    cmakelists << "set_target_properties(${TARGET} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR})" << EOL;
  } else {
    cmakelists << "set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR}";
    if (!m_linkerScript.empty()) {
      cmakelists << " LINK_DEPENDS ${LD_SCRIPT}";
    }
    cmakelists << ")" << EOL;
  }
  if (!m_incPathsList.empty()) {
    cmakelists << "target_include_directories(${TARGET} PUBLIC ${INC_PATHS})" << EOL;
  }
  if (!m_libFilesList.empty() || !m_linkerLibsGlobal.empty()) {
    cmakelists << "target_link_libraries(${TARGET}";
    if (!m_libFilesList.empty()) {
      cmakelists << " ${LIB_FILES}";
    }
    if (!m_linkerLibsGlobal.empty()) {
      cmakelists << " ${LD_FLAGS_LIBRARIES}";
    }
    cmakelists << ")" << EOL;
  }

  // Linker script pre-processing
  if (!m_linkerScript.empty() && !lib_output && ((linkerExt == SRCPPEXT) || !m_linkerRegionsFile.empty() || !m_linkerPreProcessorDefines.empty())) {
    cmakelists << EOL << "# Linker script pre-processing" << EOL << EOL;
    cmakelists << "add_custom_command(TARGET ${TARGET} PRE_LINK COMMAND ${CPP} ARGS ${CPP_ARGS_LD_SCRIPT} BYPRODUCTS ${LD_SCRIPT_PP})" << EOL;
  }

  // Bin and Hex Conversion
  if (hex_output) {
    cmakelists << EOL << "# Hex Conversion" << EOL << EOL;
    cmakelists << "add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND ${CMAKE_OBJCOPY} ${ELF2HEX})" << EOL;
  }
  if (bin_output) {
    cmakelists << EOL << "# Bin Conversion" << EOL << EOL;
    cmakelists << "add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND ${CMAKE_OBJCOPY} ${ELF2BIN})" << EOL;
  }

  // Compare cmakelists contents
  if (!CompareFile(m_genfile, cmakelists)) {
    // Create cmakelists
    ofstream fileStream(m_genfile);
    if (!fileStream) {
      LogMsg("M210", PATH(m_genfile));
      return false;
    }
    fileStream << cmakelists.rdbuf();
    fileStream << flush;
    fileStream.close();
  }
  return true;
}
