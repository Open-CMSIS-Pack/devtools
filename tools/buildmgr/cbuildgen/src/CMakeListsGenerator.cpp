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

  cmakelists << "cmake_minimum_required(VERSION 3.18)" << EOL << EOL;

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
  if (!m_byteOrder.empty())          cmakelists << EOL << "set(BYTE_ORDER " << m_byteOrder << ")";
  if (!m_asMscGlobal.empty())       cmakelists << EOL << "set(AS_FLAGS_GLOBAL \"" << CbuildUtils::EscapeQuotes(m_asMscGlobal) << "\")";
  if (!m_ccMscGlobal.empty())       cmakelists << EOL << "set(CC_FLAGS_GLOBAL \"" << CbuildUtils::EscapeQuotes(m_ccMscGlobal) << "\")";
  if (!m_cxxMscGlobal.empty())      cmakelists << EOL << "set(CXX_FLAGS_GLOBAL \"" << CbuildUtils::EscapeQuotes(m_cxxMscGlobal) << "\")";
  if (!m_linkerMscGlobal.empty())   cmakelists << EOL << "set(LD_FLAGS_GLOBAL \"" <<CbuildUtils::EscapeQuotes( m_linkerMscGlobal) << "\")";
  if (!m_linkerScript.empty())       cmakelists << EOL << "set(LD_SCRIPT \"" << m_linkerScript << "\")";

  cmakelists << EOL << EOL;

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
  if ((cc_file_specific_flags) || (cxx_file_specific_flags) || (as_file_specific_flags) ||
     (cc_group_specific_flags) || (cxx_group_specific_flags) || (as_group_specific_flags)) {
    cmakelists << EOL;
  }

  // Toolchain config
  cmakelists << "# Toolchain config map" << EOL << EOL;
  cmakelists << "include (\"" << m_toolchainConfig << "\")" << EOL << EOL;

  // Setup project
  cmakelists << "# Setup project" << EOL << EOL;
  cmakelists << "project(${TARGET} LANGUAGES";
  for (auto list : asFilesLists) {
    if (!list.second.empty()) cmakelists << " " << list.first;
  }
  if (!m_ccFilesList.empty())   cmakelists << " C";
  if (!m_cxxFilesList.empty())  cmakelists << " CXX";
  cmakelists << ")" << EOL << EOL;

  // Set global flags
  cmakelists << "# Global Flags" << EOL << EOL;

  bool asflags = as_file_specific_flags || as_group_specific_flags;
  bool ccflags = cc_file_specific_flags || cc_group_specific_flags;
  bool cxxflags = cxx_file_specific_flags || cxx_group_specific_flags;

  for (auto list : asFilesLists) {
    if (!list.second.empty()) {
      string prefix = list.first;
      cmakelists << "set(CMAKE_" << prefix << "_FLAGS \"${" << prefix << "_CPU}";
      if (!m_byteOrder.empty()) cmakelists << " ${" << prefix << "_BYTE_ORDER}";
      if (!m_definesList.empty()) cmakelists << " ${" << prefix << "_DEFINES} ${";
      cmakelists << prefix << "_FLAGS}";
      if (!asflags && !preinc_local && !m_asMscGlobal.empty()) cmakelists << " ${AS_FLAGS_GLOBAL}";
      cmakelists << "\")" << EOL;
    }
  }
  if (!m_ccFilesList.empty()) {
    cmakelists << "set(CMAKE_C_FLAGS \"${CC_CPU}";
    if (!m_byteOrder.empty()) cmakelists << " ${CC_BYTE_ORDER}";
    if (!m_definesList.empty()) cmakelists << " ${CC_DEFINES}";
    if (!m_targetSecure.empty()) cmakelists << " ${CC_SECURE}";
    cmakelists << " ${CC_FLAGS}";
    if (!ccflags && !preinc_local && !m_ccMscGlobal.empty()) cmakelists << " ${CC_FLAGS_GLOBAL}";
    cmakelists << " ${CC_SYS_INC_PATHS}";
    cmakelists << "\")" << EOL;
  }
  if (!m_cxxFilesList.empty()) {
    cmakelists << "set(CMAKE_CXX_FLAGS \"${CXX_CPU}";
    if (!m_byteOrder.empty()) cmakelists << " ${CXX_BYTE_ORDER}";
    if (!m_definesList.empty()) cmakelists << " ${CXX_DEFINES}";
    if (!m_targetSecure.empty()) cmakelists << " ${CXX_SECURE}";
    cmakelists << " ${CXX_FLAGS}";
    if (!cxxflags && !preinc_local && !m_cxxMscGlobal.empty()) cmakelists << " ${CXX_FLAGS_GLOBAL}";
    cmakelists << " ${CXX_SYS_INC_PATHS}";
    cmakelists << "\")" << EOL;
  }

  // Linker flags
  cmakelists << "set(CMAKE_";
  if (!m_cxxFilesList.empty()) cmakelists << "CXX";
  else cmakelists << "C";
  cmakelists << "_LINK_FLAGS \"${LD_CPU} ${LD_SCRIPT}";
  if (!m_targetSecure.empty()) cmakelists << " ${LD_SECURE}";
  if (!m_linkerMscGlobal.empty()) cmakelists << " ${LD_FLAGS_GLOBAL}";
  cmakelists << " ${LD_FLAGS}\")" << EOL << EOL;

  // Pre-include Global
  if (!m_preincGlobal.empty()) {
    cmakelists << "foreach(ENTRY ${PRE_INC_GLOBAL})" << EOL;
    if (!m_ccFilesList.empty()) cmakelists << "  string(APPEND CMAKE_C_FLAGS \" ${_PI}\\\"${ENTRY}\\\"\")" << EOL;
    if (!m_cxxFilesList.empty()) cmakelists << "  string(APPEND CMAKE_CXX_FLAGS \" ${_PI}\\\"${ENTRY}\\\"\")" << EOL;
    cmakelists << "endforeach()" << EOL << EOL;
  }

  bool as_special_lang = (!m_asLegacyFilesList.empty() || !m_asArmclangFilesList.empty() || !m_asGnuFilesList.empty());
  bool specific_defines = file_specific_defines || group_specific_defines;
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
        if (ccflags) {
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

  // Includes and Defines
  if (specific_includes || specific_defines) {
    map <string, bool> languages = {
      {"ASM",    !m_asFilesList.empty()         },
      {"AS_LEG", !m_asLegacyFilesList.empty()   },
      {"AS_ARM", !m_asArmclangFilesList.empty() },
      {"AS_GNU", !m_asGnuFilesList.empty()      },
      {"CC",     !m_ccFilesList.empty()         },
      {"CXX",    !m_cxxFilesList.empty()        }
    };
    cmakelists << "# File Includes and Defines" << EOL << EOL;
    for (const auto [lang, present] : languages) {
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
          cmakelists << "  if(DEFINED DEFINES_${S})" << EOL;
          cmakelists << "    cbuild_set_defines(" << lang << " DEFINES_${S})" << EOL;
          cmakelists << "    get_source_file_property(FILE_FLAGS ${SRC} COMPILE_FLAGS)" << EOL;
          cmakelists << "    if(FILE_FLAGS STREQUAL \"NOTFOUND\")" << EOL;
          cmakelists << "      set(FILE_FLAGS)" << EOL;
          cmakelists << "    endif()" << EOL;
          cmakelists << "    string(APPEND FILE_FLAGS \" ${DEFINES_${S}}\")" << EOL;
          cmakelists << "    set_source_files_properties(${SRC} PROPERTIES COMPILE_FLAGS \"${FILE_FLAGS}\")" << EOL;
          cmakelists << "  endif()" << EOL;
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

  bool lib_output = (m_outputType.compare("lib") == 0) ? true : false;
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
  if (lib_output) {
    cmakelists << "set(CMAKE_STATIC_LIBRARY_PREFIX ${LIB_PREFIX})" << EOL;
    cmakelists << "set(CMAKE_STATIC_LIBRARY_SUFFIX ${LIB_SUFFIX})" << EOL;
    cmakelists << "set_target_properties(${TARGET} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${OUT_DIR})" << EOL;
  } else {
    cmakelists << "set(CMAKE_EXECUTABLE_SUFFIX ${EXE_SUFFIX})" << EOL;
    cmakelists << "set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR})" << EOL;
  }
  if (!m_incPathsList.empty()) {
    cmakelists << "target_include_directories(${TARGET} PUBLIC ${INC_PATHS})" << EOL;
  }
  if (!m_libFilesList.empty()) {
    cmakelists << "target_link_libraries(${TARGET} ${LIB_FILES})" << EOL;
  }

  if (!lib_output) {
    cmakelists << EOL << "# Bin and Hex Conversion" << EOL << EOL;
    cmakelists << "add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND ${CMAKE_OBJCOPY} ${ELF2HEX})" << EOL;
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
