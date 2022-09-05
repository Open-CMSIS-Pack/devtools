/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 // Entry point for the console application.

#include "AuxCmd.h"
#include "CMakeListsGenerator.h"
#include "ProductInfo.h"

#include "CbuildKernel.h"
#include "CbuildUtils.h"
#include "ErrLog.h"
#include "ErrOutputterSaveToStdoutOrFile.h"
#include "CrossPlatformUtils.h"

#include <iostream>
#include <string>

using namespace std;

class CMSISBuildConsole {
public:
  CMSISBuildConsole(void);
  ~CMSISBuildConsole(void);

  /**
   * @brief prints module name, version and copyright
   * @param
  */
  void Signature(void);

  /**
   * @brief print module's date and time
   * @param
  */
  void Usage(void);

  /**
   * @brief print module's command line options
   * @param
  */
  void DateTime(void);
};

CMSISBuildConsole::CMSISBuildConsole(void) {
  // Reserved
}

CMSISBuildConsole::~CMSISBuildConsole(void) {
  // Reserved
}

void CMSISBuildConsole::Signature(void) {
  LogMsg("M021", VAL("EXE", ORIGINAL_FILENAME), VAL("PROD", PRODUCT_NAME), VAL("VER", VERSION_STRING), TXT(COPYRIGHT_NOTICE));
}

void CMSISBuildConsole::DateTime(void) {
  LogMsg("M022", VAL("EXE", ORIGINAL_FILENAME), VAL("DATE", __DATE__), VAL("TIME", __TIME__));
}

void CMSISBuildConsole::Usage(void) {
  LogMsg("M020", VAL("EXE", ORIGINAL_FILENAME));
}

int main(int argc, const char *argv[])
{
  CMSISBuildConsole console = CMSISBuildConsole();
  ErrLog::Get()->SetOutputter(new ErrOutputterSaveToStdoutOrFile());
  ErrLog::Get()->SetLevel(MsgLevel::LEVEL_INFO);
  InitMessageTable();

  if (argc <= 1) {
    // Print Signature, DateTime and Usage
    console.Signature();
    console.DateTime();
    console.Usage();
    return 0;
  }

  // Parse command line
  list<string> args;
  for (int i = 1; i < argc; ++i) {
    args.push_back(argv[i]);
  }

  // Auxiliary commands
  bool mkdirCmd = false, rmdirCmd = false, touchCmd = false;
  list<string> params;
  string except;
  for (auto arg : args) {
    if       (arg.compare("mkdir")                  == 0)            mkdirCmd = true;
    else if  (arg.compare("rmdir")                  == 0)            rmdirCmd = true;
    else if  (arg.compare("touch")                  == 0)            touchCmd = true;
    else if  (arg.compare(0, 9, "--except=")        == 0)            except = arg.substr(9, arg.length());
    else params.push_back(arg);
  }
  if (mkdirCmd || rmdirCmd || touchCmd) {
    console.Signature();
    int cnt = 0;
    for (auto b : { mkdirCmd, rmdirCmd, touchCmd }) {
      if (b) {
        cnt++;
      }
    }
    if (cnt > 1) {
      // Multiple commands were given
      LogMsg("M207");
      return 1;
    }
    int cmd = mkdirCmd ? AUX_MKDIR : rmdirCmd ? AUX_RMDIR : touchCmd ? AUX_TOUCH : 0;
    AuxCmd auxcmd = AuxCmd();
    if (!auxcmd.RunAuxCmd(cmd, params, except)) {
      return 1;
    }
    ErrLog::Get()->SetQuietMode();
    return 0;
  }

  // Normal commands
  bool packMode = false, cmakeMode = false, cmdlineErr = false, extractLayer = false, composeLayer = false, addLayer = false, removeLayer = false, quiet = false;
  string cprjFile, toolchain, intdir, outdir, name, description, update;
  string packRoot, compilerRoot, buildRoot;
  list<string> layerFiles, layerIDs;
  std::map<string, string> optionAttributes;
  for (auto arg : args) {
    if       (arg.compare("packlist")                == 0)            packMode = true;
    else if  (arg.compare("cmake")                   == 0)            cmakeMode = true;
    else if  (arg.compare("extract")                 == 0)            extractLayer = true;
    else if  (arg.compare("compose")                 == 0)            composeLayer = true;
    else if  (arg.compare("add")                     == 0)            addLayer = true;
    else if  (arg.compare("remove")                  == 0)            removeLayer = true;
    else if  (arg.compare(0, 12, "--toolchain=")     == 0)            toolchain = arg.substr(12, arg.length());
    else if  (arg.compare(0, 9,  "--update=")        == 0)            update = arg.substr(9, arg.length());
    else if  (arg.compare(0, 9,  "--intdir=")        == 0)            intdir = arg.substr(9, arg.length());
    else if  (arg.compare(0, 9,  "--outdir=")        == 0)            outdir = arg.substr(9, arg.length());
    else if  (arg.compare(0, 7,  "--name=")          == 0)            name = arg.substr(7, arg.length());
    else if  (arg.compare(0, 14, "--description=")   == 0)            description = arg.substr(14, arg.length());
    else if  (arg.compare(0, 8,  "--layer=")         == 0)            layerIDs.push_back(arg.substr(8, arg.length()));
    else if  (arg.compare("--quiet")                 == 0)            quiet = true;
    else if  (arg.compare(0, 12, "--pack_root=")     == 0)            packRoot = arg.substr(12, arg.length());
    else if  (arg.compare(0, 16, "--compiler_root=") == 0)            compilerRoot = arg.substr(16, arg.length());
    else if ((arg.compare(0, 1,  "-") == 0) && (arg.compare(1, 1, "-") != 0)) {
      size_t div = arg.find_first_of("=");
      if (div != string::npos) optionAttributes[arg.substr(0, div)] = arg.substr(div+1, arg.length());
      else cmdlineErr = true;
    }
    else if  (arg.find(".cprj")                  != string::npos) cprjFile = arg;
    else if  (arg.find(".clayer")                != string::npos) layerFiles.push_back(arg);
    else cmdlineErr = true;
  }

  if (quiet) {
    // quiet mode
    ErrLog::Get()->SuppressAllInfo();
  }

  // Print Signature
  console.Signature();

  if (!packMode && !cmakeMode && !extractLayer && !composeLayer && !addLayer && !removeLayer) {
    // No command was given
    LogMsg("M206");
    cmdlineErr = true;
  }

  int cnt = 0;
  for (auto b : { packMode, cmakeMode, extractLayer, composeLayer, addLayer, removeLayer }) {
    if (b) {
      cnt++;
    }
  }
  if (cnt > 1) {
    // Multiple commands were given
    LogMsg("M207");
    cmdlineErr = true;
  }

  if (cprjFile.empty()) {
    // No CPRJ file was given
    LogMsg("M202");
    cmdlineErr = true;
  }

  if (extractLayer && outdir.empty()) {
    // No output was given for extract command
    LogMsg("M214");
    cmdlineErr = true;
  }

  if (cmdlineErr) {
    // Invalid arguments
    LogMsg("M200");
    console.Usage();
    return 1;
  }

  if (packRoot.empty()) {
    packRoot = CrossPlatformUtils::GetEnv("CMSIS_PACK_ROOT");
    if (packRoot.empty()) {
      packRoot = CrossPlatformUtils::GetDefaultCMSISPackRootDir();
    }
  }

  if (compilerRoot.empty()) {
    compilerRoot = CrossPlatformUtils::GetEnv("CMSIS_COMPILER_ROOT");
    if (compilerRoot.empty()) {
      error_code ec;
      string currExePath = CrossPlatformUtils::GetExecutablePath(ec);
      if (currExePath.empty()) {
        LogMsg("M216", VAL("MSG", ec.message()));
        return 1;
      }
      compilerRoot = fs::path(currExePath).parent_path().append("etc").generic_string();
      if (fs::exists(compilerRoot)) {
        LogMsg("M204", VAL("PATH", compilerRoot));
        return 1;
      }
    }
  }

  // Run layer command
  int cmd = extractLayer ? L_EXTRACT : composeLayer ? L_COMPOSE : addLayer ? L_ADD : removeLayer ? L_REMOVE : 0;
  list<string>* pLayer = NULL;
  switch (cmd) {
    case L_EXTRACT:
    case L_REMOVE:
      pLayer = &layerIDs;
      break;
    case L_COMPOSE:
    case L_ADD:
      pLayer = &layerFiles;
      break;
  }
  if (cmd != 0) {
    if (RunLayer(cmd, {cprjFile, packRoot, compilerRoot, optionAttributes, *pLayer, name, description, outdir} )) {
      // layer command run successfully
      LogMsg("M650");
      return 0;
    } else {
      // layer command run failed
      return 1;
    }
  }

  if ((cmakeMode || packMode) && (!CreateRte({cprjFile, packRoot, compilerRoot, optionAttributes, toolchain, CMEXT, update, intdir, packMode}))) {
    // CreateRte failed
    return 1;
  }

  if (packMode) {
    // packlist command run successfully
    LogMsg("M650");
    return 0;
  }

  // Get RTE Model output
  const CbuildModel* model = CbuildKernel::Get()->GetModel();

  if (cmakeMode) {
    // Create CMakeListsGenerator instance and collect data
    CMakeListsGenerator instance;
    if (!instance.Collect(cprjFile, model, outdir, intdir)) {
      return 1;
    }
    if (instance.GenBuildCMakeLists()) {
      LogMsg("M652", VAL("NAME", instance.m_genfile));
    }
    instance.GenAuditFile();
  }

  return 0;
}
