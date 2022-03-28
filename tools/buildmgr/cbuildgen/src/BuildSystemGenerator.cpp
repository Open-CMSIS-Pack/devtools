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

bool BuildSystemGenerator::Collect(const string& inputFile, const CbuildModel *model, const string& outdir, const string& intdir) {
  error_code ec;
  m_projectDir = StrConv(RteFsUtils::AbsolutePath(inputFile).remove_filename().generic_string());
  m_workingDir = fs::current_path(ec).generic_string() + SS;

  // Find toolchain config
  m_toolchainConfig = StrNorm(model->GetToolchainConfig());
  m_toolchain = model->GetCompiler();

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
  m_linkerScript = StrNorm(model->GetLinkerScript());
  m_outputType = model->GetOutputType();

  m_ccMscGlobal = GetString(model->GetTargetCFlags());
  m_cxxMscGlobal = GetString(model->GetTargetCxxFlags());
  m_asMscGlobal = GetString(model->GetTargetAsFlags());
  m_linkerMscGlobal = GetString(model->GetTargetLdFlags());

  for (auto inc : model->GetTargetIncludePaths())
  {
    CbuildUtils::PushBackUniquely(m_incPathsList, StrNorm(inc));
  }

  for (auto preinc : model->GetPreIncludeFilesGlobal())
  {
    CbuildUtils::PushBackUniquely(m_preincGlobal, StrNorm(preinc));
  }

  for (auto group : model->GetPreIncludeFilesLocal())
  {
    set<string> preincSet;
    for (auto preinc : group.second) {
      preincSet.insert(StrNorm(preinc));
    }
    m_groupsList[StrNorm(group.first)].preinc = preincSet;
  }

  for (auto lib : model->GetLibraries())
  {
    CbuildUtils::PushBackUniquely(m_libFilesList, StrNorm(lib));
  }

  for (auto obj : model->GetObjects())
  {
    CbuildUtils::PushBackUniquely(m_libFilesList, StrNorm(obj));
  }

  for (auto def : model->GetTargetDefines())
  {
    CbuildUtils::PushBackUniquely(m_definesList, def);
  }

  const map<string, std::vector<string>>& defines = model->GetDefines();
  const map<string, std::vector<string>>& incPaths = model->GetIncludePaths();
  for (auto list : model->GetCSourceFiles())
  {
    string group = list.first;
    string cGFlags, cGDefines, cGIncludes;
    const map<string, std::vector<string>>& cFlags = model->GetCFlags();
    string groupName = group;

    // flags
    do {
      if (cFlags.find(groupName) != cFlags.end()) {
        cGFlags = GetString(cFlags.at(groupName));
        break;
      }
      groupName = fs::path(groupName).parent_path().generic_string();
    } while (!groupName.empty());
    m_groupsList[StrNorm(group)].ccMsc = cGFlags;
    CollectGroupDefinesIncludes(defines, incPaths, group);

    for (auto src : list.second) {
      string cFFlags;
      if (cFlags.find(src) != cFlags.end()) cFFlags = GetString(cFlags.at(src));
      src = StrNorm(src);
      m_ccFilesList[src].group = StrNorm(group + (group.empty() ? "" : SS));
      m_ccFilesList[src].flags = cFFlags;
      CollectFileDefinesIncludes(defines, incPaths, src, group, m_ccFilesList);
    }
  }

  for (auto list : model->GetCxxSourceFiles())
  {
    string group = list.first;
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

    for (auto src : list.second) {
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

  for (auto list : model->GetAsmSourceFiles())
  {
    string group = list.first;
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
    for (auto src : list.second) {
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

  // Configuration files
  for (auto cfg : model->GetConfigFiles())
  {
    m_cfgFilesList.insert({ StrNorm(cfg.first), StrNorm(cfg.second) });
  }

  // Audit data
  m_auditData = model->GetAuditData();

  return true;
}

bool BuildSystemGenerator::GenAuditFile(void) {
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

string BuildSystemGenerator::StrNorm(string path) {
  /* StrNorm:
  - Convert backslashes into forward slashes
  - Remove double slashes, leading dot and trailing slash
  */

  size_t s = 0;
  path = StrConv(path);
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
