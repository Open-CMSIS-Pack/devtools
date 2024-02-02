/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 // BuildSystemGenerator.cpp

#include "BuildSystemGenerator.h"

#include "Cbuild.h"
#include "CbuildKernel.h"
#include "CbuildUtils.h"

#include "ErrLog.h"
#include "RteFsUtils.h"
#include "RteUtils.h"

#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

using namespace std;

BuildSystemGenerator::BuildSystemGenerator(void) {
  // Reserved
}

BuildSystemGenerator::~BuildSystemGenerator(void) {
  // Reserved
}

bool BuildSystemGenerator::Collect(const string& inputFile, const CbuildModel *model, const string& outdir, const string& intdir, const string& compilerRoot) {
  error_code ec;
  m_projectDir = StrConv(RteFsUtils::AbsolutePath(inputFile).remove_filename().generic_string());
  m_workingDir = fs::current_path(ec).generic_string() + SS;
  m_compilerRoot = compilerRoot;
  RteFsUtils::NormalizePath(m_compilerRoot);

  // Find toolchain config
  m_toolchainConfig = StrNorm(model->GetToolchainConfig());
  m_toolchain = model->GetCompiler();
  m_toolchainVersion = model->GetCompilerVersion();
  m_toolchainRegisteredRoot = model->GetToolchainRegisteredRoot();
  m_toolchainRegisteredVersion = model->GetToolchainRegisteredVersion();

  // Output and intermediate directories
  if (!outdir.empty()) {
    m_outdir = StrConv(outdir);
    if (fs::path(m_outdir).is_relative()) {
      m_outdir = m_workingDir + m_outdir;
    }
  } else if (!model->GetOutDir().empty()) {
    m_outdir = StrConv(model->GetOutDir());
    if (fs::path(m_outdir).is_relative()) {
      m_outdir = m_projectDir + m_outdir;
    }
  } else {
    m_outdir = m_projectDir + "OutDir";
  }
  if (!fs::exists(m_outdir, ec)) {
    fs::create_directories(m_outdir, ec);
  }
  m_outdir = fs::canonical(m_outdir, ec).generic_string() + SS;

  if (!intdir.empty()) {
    m_intdir = StrConv(intdir);
    if (fs::path(m_intdir).is_relative()) {
      m_intdir = m_workingDir + m_intdir;
    }
  } else if (!model->GetIntDir().empty()) {
    m_intdir = StrConv(model->GetIntDir());
    if (fs::path(m_intdir).is_relative()) {
      m_intdir = m_projectDir + m_intdir;
    }
  } else {
    m_intdir = m_projectDir + "IntDir";
  }
  if (!fs::exists(m_intdir, ec)) {
    fs::create_directories(m_intdir, ec);
  }
  m_intdir = fs::canonical(m_intdir, ec).generic_string() + SS;

  // Target attributes
  m_projectName = fs::path(inputFile).stem().generic_string();
  m_targetName = StrNorm(model->GetOutputName());
  const RteTarget* target = model->GetTarget();
  m_targetCpu = target->GetAttribute("Dcore");
  m_targetFpu = target->GetAttribute("Dfpu");
  m_targetDsp = target->GetAttribute("Ddsp");
  m_byteOrder = target->GetAttribute("Dendian");
  m_targetTz = target->GetAttribute("Dtz");
  m_targetSecure = target->GetAttribute("Dsecure");
  m_targetMve = target->GetAttribute("Dmve");
  m_targetBranchProt = target->GetAttribute("DbranchProt");
  m_linkerScript = StrNorm(model->GetLinkerScript());
  m_linkerRegionsFile = StrNorm(model->GetLinkerRegionsFile());
  m_outputType = model->GetOutputType();
  m_outputFiles = model->GetOutputFiles();

  m_optimize = model->GetTargetOptimize();
  m_debug = model->GetTargetDebug();
  m_warnings = model->GetTargetWarnings();
  m_languageC = model->GetTargetLanguageC();
  m_languageCpp = model->GetTargetLanguageCpp();

  m_ccMscGlobal = GetString(model->GetTargetCFlags());
  m_cxxMscGlobal = GetString(model->GetTargetCxxFlags());
  m_asMscGlobal = GetString(model->GetTargetAsFlags());
  m_linkerMscGlobal = GetString(model->GetTargetLdFlags());
  m_linkerCMscGlobal = GetString(model->GetTargetLdCFlags());
  m_linkerCxxMscGlobal = GetString(model->GetTargetLdCxxFlags());
  m_linkerLibsGlobal = GetString(model->GetTargetLdLibs());

  m_linkerPreProcessorDefines = model->GetLinkerPreProcessorDefines();
  MergeVecStr(model->GetTargetDefines(), m_definesList);
  MergeVecStrNorm(model->GetTargetIncludePaths(), m_incPathsList);
  MergeVecStrNorm(model->GetPreIncludeFilesGlobal(), m_preincGlobal);
  MergeVecStrNorm(model->GetLibraries(), m_libFilesList);
  MergeVecStrNorm(model->GetObjects(), m_libFilesList);

  for (const auto& [componentName, files] : model->GetPreIncludeFilesLocal())
  {
    MergeVecStrNorm(files, m_groupsList[StrNorm(componentName)].preinc);
  }

  // Misc, defines and includes
  if (!CollectMiscDefinesIncludes(model)) {
    return false;
  }

  // Optimize, debug, warnings, languageC and languageCpp options
  if (!CollectTranslationControls(model)) {
    return false;
  }

  // Configuration files
  for (const auto& cfg : model->GetConfigFiles())
  {
    m_cfgFilesList.insert({ StrNorm(cfg.first), StrNorm(cfg.second) });
  }

  // Audit data
  m_auditData = model->GetAuditData();

  return true;
}

bool BuildSystemGenerator::CollectMiscDefinesIncludes(const CbuildModel* model) {
  // Misc, defines and includes
  const map<string, std::vector<string>>& defines = model->GetDefines();
  const map<string, std::vector<string>>& incPaths = model->GetIncludePaths();
  for (const auto& [group, files] : model->GetCSourceFiles())
  {
    string cGFlags, cGDefines, cGIncludes;
    const map<string, std::vector<string>>& cFlags = model->GetCFlags();
    string groupName = group;

    do {
      if (cFlags.find(groupName) != cFlags.end()) {
        cGFlags = GetString(cFlags.at(groupName));
        break;
      }
      groupName = fs::path(groupName).parent_path().generic_string();
    } while (!groupName.empty());
    m_groupsList[StrNorm(group)].ccMsc = cGFlags;
    CollectGroupDefinesIncludes(defines, incPaths, group);

    for (auto src : files) {
      string cFFlags;
      if (cFlags.find(src) != cFlags.end()) cFFlags = GetString(cFlags.at(src));
      src = StrNorm(src);
      m_ccFilesList[src].group = StrNorm(group + (group.empty() ? "" : SS));
      m_ccFilesList[src].flags = cFFlags;
      CollectFileDefinesIncludes(defines, incPaths, src, group, m_ccFilesList);
    }
  }

  for (const auto& [group, files] : model->GetCxxSourceFiles())
  {
    string cxxGFlags, cxxGDefines, cxxGIncludes;
    const map<string, std::vector<string>>& cxxFlags = model->GetCxxFlags();
    string groupName = group;

    do {
      if (cxxFlags.find(groupName) != cxxFlags.end()) {
        cxxGFlags = GetString(cxxFlags.at(groupName));
        break;
      }
      groupName = fs::path(groupName).parent_path().generic_string();
    } while (!groupName.empty());
    m_groupsList[StrNorm(group)].cxxMsc = cxxGFlags;
    CollectGroupDefinesIncludes(defines, incPaths, group);

    for (auto src : files) {
      string cxxFFlags;
      if (cxxFlags.find(src) != cxxFlags.end()) cxxFFlags = GetString(cxxFlags.at(src));
      src = StrNorm(src);
      m_cxxFilesList[src].group = StrNorm(group + (group.empty() ? "" : SS));
      m_cxxFilesList[src].flags = cxxFFlags;
      CollectFileDefinesIncludes(defines, incPaths, src, group, m_cxxFilesList);
    }
  }

  const map<string, bool> assembler = model->GetAsm();
  m_asTargetAsm = (!assembler.empty()) && (assembler.find("") != assembler.end()) ? assembler.at("") : false;

  for (const auto& [group, files] : model->GetAsmSourceFiles())
  {
    string asGFlags, asGDefines, asGIncludes;
    const map<string, std::vector<string>>& asFlags = model->GetAsFlags();
    string groupName = group;
    do {
      if (asFlags.find(groupName) != asFlags.end()) {
        asGFlags = GetString(asFlags.at(groupName));
        break;
      }
      groupName = fs::path(groupName).parent_path().generic_string();
    } while (!groupName.empty());
    m_groupsList[StrNorm(group)].asMsc = asGFlags;
    CollectGroupDefinesIncludes(defines, incPaths, group);

    bool group_asm = (assembler.find(group) != assembler.end()) ? assembler.at(group) : m_asTargetAsm;
    for (auto src : files) {
      string asFFlags;
      if (asFlags.find(src) != asFlags.end()) asFFlags = GetString(asFlags.at(src));

      // Default assembler: armclang or gcc with gnu syntax and preprocessing
      map<string, module>* pList = &m_asFilesList;

      // Special handling: "legacy" armasm or gas assembler; armasm or gnu syntax
      if ((m_toolchain == "AC6") || (m_toolchain == "GCC")) {
        bool file_asm = (assembler.find(src) != assembler.end()) ? assembler.at(src) : group_asm;
        string flags = !asFFlags.empty() ? asFFlags : !asGFlags.empty() ? asGFlags : m_asMscGlobal;
        if (file_asm) {
          // Legacy assembler (e.g. armasm or gas)
          pList = &m_asLegacyFilesList;
        } else if ((fs::path(StrConv(src)).extension().compare(".S") != 0) && (flags.find("-x assembler-with-cpp") == string::npos)) {
          // Default assembler (e.g. armclang or gcc) without preprocessing
          if ((m_toolchain == "AC6") && ((flags.find("-masm=armasm") != string::npos) || (flags.find("-masm=auto") != string::npos))) {
            // armclang with Arm syntax or Auto
            pList = &m_asArmclangFilesList;
          } else {
            // GNU syntax
            pList = &m_asGnuFilesList;
          }
        }
      }

      src = StrNorm(src);
      (*pList)[src].group = StrNorm(group + (group.empty() ? "" : SS));
      (*pList)[src].flags = asFFlags;
      CollectFileDefinesIncludes(defines, incPaths, src, group, (*pList));
    }
  }
  return true;
}

bool BuildSystemGenerator::CollectTranslationControls(const CbuildModel* model) {
  // Optimize, debug, warnings, languageC and languageCpp options
  vector<std::map<std::string, std::list<std::string>>> sourceFilesList = {
    model->GetAsmSourceFiles(), model->GetCSourceFiles(), model->GetCxxSourceFiles()
  };

  for (const auto& sourceFiles : sourceFilesList)
  {
    if (sourceFiles.empty()) continue;

    const map<string, string>& optimizeOpt = model->GetOptimizeOption();
    const map<string, string>& debugOpt = model->GetDebugOption();
    const map<string, string>& warningsOpt = model->GetWarningsOption();
    const map<string, string>& languageCOpt = model->GetLanguageCOption();
    const map<string, string>& languageCppOpt = model->GetLanguageCppOption();

    for (const auto& [group, files] : sourceFiles)
    {
      string groupName;
      string optGOptimize, optGDebug, optGWarnings, optGLanguageC, optGLanguageCpp;

      // optimize option
      groupName = group;
      do {
        if (optimizeOpt.find(groupName) != optimizeOpt.end()) {
          optGOptimize = optimizeOpt.at(groupName);
          break;
        }
        groupName = fs::path(groupName).parent_path().generic_string();
      } while (!groupName.empty());
      m_groupsList[StrNorm(group)].optimize = optGOptimize;

      // debug option
      groupName = group;
      do {
        if (debugOpt.find(groupName) != debugOpt.end()) {
          optGDebug = debugOpt.at(groupName);
          break;
        }
        groupName = fs::path(groupName).parent_path().generic_string();
      } while (!groupName.empty());
      m_groupsList[StrNorm(group)].debug = optGDebug;

      // warnings option
      groupName = group;
      do {
        if (warningsOpt.find(groupName) != warningsOpt.end()) {
          optGWarnings = warningsOpt.at(groupName);
          break;
        }
        groupName = fs::path(groupName).parent_path().generic_string();
      } while (!groupName.empty());
      m_groupsList[StrNorm(group)].warnings = optGWarnings;

      // language C option
      groupName = group;
      do {
        if (languageCOpt.find(groupName) != languageCOpt.end()) {
          optGLanguageC = languageCOpt.at(groupName);
          break;
        }
        groupName = fs::path(groupName).parent_path().generic_string();
      } while (!groupName.empty());
      m_groupsList[StrNorm(group)].languageC = optGLanguageC;

      // language Cpp option
      groupName = group;
      do {
        if (languageCppOpt.find(groupName) != languageCppOpt.end()) {
          optGLanguageCpp = languageCppOpt.at(groupName);
          break;
        }
        groupName = fs::path(groupName).parent_path().generic_string();
      } while (!groupName.empty());
      m_groupsList[StrNorm(group)].languageCpp = optGLanguageCpp;

      for (auto src : files) {
        string optFOptimize, optFDebug, optFWarnings, optFLanguageC, optFLanguageCpp;
        src = StrNorm(src);

        std::map<std::string, module>* m_langFilesList;
        if (m_ccFilesList.find(src) != m_ccFilesList.end()) m_langFilesList = &m_ccFilesList;
        else if (m_cxxFilesList.find(src) != m_cxxFilesList.end()) m_langFilesList = &m_cxxFilesList;
        else if (m_asFilesList.find(src) != m_asFilesList.end()) m_langFilesList = &m_asFilesList;
        else if (m_asGnuFilesList.find(src) != m_asGnuFilesList.end()) m_langFilesList = &m_asGnuFilesList;
        else if (m_asArmclangFilesList.find(src) != m_asArmclangFilesList.end()) m_langFilesList = &m_asArmclangFilesList;
        else if (m_asLegacyFilesList.find(src) != m_asLegacyFilesList.end()) m_langFilesList = &m_asLegacyFilesList;
        else { LogMsg("M101"); return false; }

        (*m_langFilesList)[src].group = StrNorm(group + (group.empty() ? "" : SS));
        if (optimizeOpt.find(src) != optimizeOpt.end()) optFOptimize = optimizeOpt.at(src);
        (*m_langFilesList)[src].optimize = optFOptimize;
        if (debugOpt.find(src) != debugOpt.end()) optFDebug = debugOpt.at(src);
        (*m_langFilesList)[src].debug = optFDebug;
        if (warningsOpt.find(src) != warningsOpt.end()) optFWarnings = warningsOpt.at(src);
        (*m_langFilesList)[src].warnings = optFWarnings;
        if (languageCOpt.find(src) != languageCOpt.end()) optFLanguageC = languageCOpt.at(src);
        (*m_langFilesList)[src].languageC = optFLanguageC;
        if (languageCppOpt.find(src) != languageCppOpt.end()) optFLanguageCpp = languageCppOpt.at(src);
        (*m_langFilesList)[src].languageCpp = optFLanguageCpp;
      }
    }
  }
  return true;
}

bool BuildSystemGenerator::GenAuditFile(void) {
  // Clean output directory
  if (!CleanOutDir()) {
    return false;
  }

  // Create audit file
  string filename = m_outdir + m_projectName + LOGEXT;
  ofstream auditFile(filename);
  if (!auditFile) {
    LogMsg("M210", PATH(filename));
    return false;
  }

  // Header
  auditFile << "# CMSIS Build Audit File generated on " << CbuildUtils::GetLocalTimestamp() << EOL << EOL;
  auditFile << "# Project Description File: " << m_projectDir << m_projectName << PDEXT << EOL << EOL;
  auditFile << "# Toolchain Configuration File: " << m_toolchainConfig;

  // Add audit data from RTE Model
  auditFile << m_auditData;

  // Flush and close audit file
  auditFile << std::endl;
  auditFile.close();
  return true;
}

void BuildSystemGenerator::MergeVecStr(const std::vector<std::string>& src, std::vector<std::string>& dest) {
  for (const auto& item : src)
  {
    CollectionUtils::PushBackUniquely(dest, item);
  }
}

void BuildSystemGenerator::MergeVecStrNorm(const std::vector<std::string>& src, std::vector<std::string>& dest) {
  for (const auto& item : src)
  {
    CollectionUtils::PushBackUniquely(dest, StrNorm(item));
  }
}

string BuildSystemGenerator::StrNorm(string path) {
  /* StrNorm:
  - Convert backslashes into forward slashes
  - Remove double slashes, leading dot and trailing slash
  */

  size_t s = 0;
  path = StrConv(path);
  // Check if path is network path
  if (path.compare(0, 2, DS) == 0) {
    s = 2;
  }
  while ((s = path.find(DS, s)) != string::npos) path.replace(s, 2, SS);
  if (path.compare(0, 2, LDOT) == 0) path.replace(0, 2, EMPTY);
  if (path.empty()) {
    return path;
  }
  if (path.rfind(SS) == path.length() - 1) path.pop_back();
  return path;
}

string BuildSystemGenerator::StrConv(string path) {
  /* StrConv:
  - Convert backslashes into forward slashes
  */
  if (path.empty()) {
    return path;
  }
  size_t s = 0;
  while ((s = path.find(BS, s)) != string::npos) {
    path.replace(s, 1, SS);
  }
  return path;
}

template<typename T> string BuildSystemGenerator::GetString(T data) {
  /*
  GetString:
  Concatenate elements from a set or list of strings separated by a space
  */
  if (!data.size()) {
    return string();
  }
  ostringstream stream;
  copy(data.begin(), data.end(), ostream_iterator<string>(stream, WS));
  string s = stream.str();
  if (s.rfind(WS) == s.length() - 1) s.pop_back();
  return s;
}
template string BuildSystemGenerator::GetString<set<string>>(set<string>);
template string BuildSystemGenerator::GetString<list<string>>(list<string>);

bool BuildSystemGenerator::CompareFile(const string& filename, stringstream& buffer) const {
  /*
  CompareFile:
  Compare file contents
  */
  ifstream fileStream;
  string line1, line2;

  // open the file if it exists
  fileStream.open(filename);
  if (!fileStream.is_open()) {
    return false;
  }

  // file is empty
  if (fileStream.peek() == ifstream::traits_type::eof()) {
    fileStream.close();
    return false;
  }

  // iterate over file and buffer lines
  while (getline(fileStream, line1), getline(buffer, line2)) {
    if (line1 != line2) {
      // ignore commented lines
      if ((!line1.empty() && line1.at(0) == '#') && (!line2.empty() && line2.at(0) == '#')) continue;
      // a difference was found
      break;
    }
  }

  // check if both streams where fully scanned
  bool done = fileStream.eof() && buffer.eof() ? true : false;

  // rewind stream buffer
  buffer.clear();
  buffer.seekg(0);

  // return true if contents are identical
  fileStream.close();
  return done;
}

void BuildSystemGenerator::CollectGroupDefinesIncludes(
  const map<string, vector<string>>& defines,
  const map<string, vector<string>>& includes, const string& group)
{
  string groupDefines, groupIncludes;

  // define
  string groupName = group;
  do {
    if (defines.find(groupName) != defines.end()) {
      groupDefines = GetString(defines.at(groupName));
      break;
    }
    groupName = fs::path(groupName).parent_path().generic_string();
  } while (!groupName.empty());
  m_groupsList[StrNorm(group)].defines = groupDefines;

  // include
  groupName = group;
  do {
    if (includes.find(groupName) != includes.end()) {
      groupIncludes = GetString(includes.at(groupName));
      break;
    }
    groupName = fs::path(groupName).parent_path().generic_string();
  } while (!groupName.empty());
  m_groupsList[StrNorm(group)].includes = groupIncludes;
}

void BuildSystemGenerator::CollectFileDefinesIncludes(
  const map<string, vector<string>>& defines, const map<string, vector<string>>& includes,
  string& src, const string& group, map<string, module>& FilesList)
{
  // defines
  string fileDefine, incPath;
  if (defines.find(src) != defines.end()) {
    fileDefine = GetString(defines.at(src));
  }
  src = StrNorm(src);
  FilesList[src].group = StrNorm(group + (group.empty() ? "" : SS));
  FilesList[src].defines = fileDefine;

  // includes
  if (includes.find(src) != includes.end()) {
    incPath = GetString(includes.at(src));
  }
  src = StrNorm(src);
  FilesList[src].group = StrNorm(group + (group.empty() ? "" : SS));
  FilesList[src].includes = incPath;
 }

bool BuildSystemGenerator::CleanOutDir() {
  if ((m_outdir == m_projectDir) || RteFsUtils::Exists(m_outdir + "/" + m_projectName + LOGEXT)) {
    return true;
  }

  // collect artifacts to be deleted
  RteFsUtils::PathVec matchedFiles = RteFsUtils::GrepFiles(m_outdir, "*[\\/]" + m_targetName + "[.]*");
  auto libFiles = RteFsUtils::GrepFiles(m_outdir, "*[\\/]lib" + m_targetName + "[.]*");
  std::copy(libFiles.begin(), libFiles.end(), std::back_inserter(matchedFiles));

  // remove existing redundant build artifacts (if any)
  for (const fs::path& file : matchedFiles) {
    if (!RteFsUtils::RemoveFile(file.generic_string())) {
      LogMsg("M212", VAL("PATH", file.generic_string()));
      return false;
    }
  }
  return true;
}
