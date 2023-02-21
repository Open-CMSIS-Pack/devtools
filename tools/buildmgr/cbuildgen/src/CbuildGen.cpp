/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "AuxCmd.h"
#include "CbuildGen.h"
#include "CMakeListsGenerator.h"
#include "ProductInfo.h"

#include "Cbuild.h"
#include "CbuildKernel.h"
#include "CbuildUtils.h"
#include "CrossPlatformUtils.h"
#include "ErrLog.h"
#include "ErrOutputterSaveToStdoutOrFile.h"

#include <regex>

using namespace std;

static constexpr const char* USAGE = "\n\
Usage:\n\
  cbuildgen [-V] [--version] [-h] [--help]\n\
            [<ProjectFile>.cprj] <command> [<args>]\n\n\
 Commands:\n\
   packlist              Write the URLs of missing packs into <ProjectFile>.cpinstall\n\
   cmake                 Generate CMakeLists.txt\n\
   extract               Export <Layer1>...<LayerN> from <ProjectFile>.cprj into <OutDir> folder\n\
   remove                Delete <Layer1>...<LayerN> from <ProjectFile>.cprj\n\
   compose               Generate a new <ProjectFile>.cprj from <1.clayer>...<N.clayer>\n\
   add                   Insert <1.clayer>...<N.clayer> into <ProjectFile>.cprj\n\
   mkdir                 Create directories including parents\n\
   touch                 Set access, modification time to the current time and create file if it does not exist\n\
   rmdir                 Remove directories and their contents recursively\n\n\
 Options:\n\
   --toolchain arg       Toolchain to be used\n\
   --update arg          CprjFile to be generated with fixed versions for reproducing the current build\n\
   --intdir arg          Path of intermediate directory\n\
   --outdir arg          Path of output directory\n\
   --layer arg           Optional layer(s) ID\n\
   --name arg            Name of the project to be composed\n\
   --description arg     Description of the project to be composed\n\
   --pack-root arg       Path to the CMSIS-Pack root directory that stores software packs\n\
   --compiler-root arg   Path to the installation 'etc' directory\n\
   --update-rte          Update the RTE directory and files\n\
   --quiet               Run cbuildgen silently, printing only error messages\n\n\
Use 'cbuildgen <command> -h' for more information about a command.\n\
";

CbuildGen::CbuildGen(void) {
  // Reserved
}

CbuildGen::~CbuildGen(void) {
  // Reserved
}

void CbuildGen::Signature(void) {
  LogMsg("M021", VAL("EXE", ORIGINAL_FILENAME), VAL("PROD", PRODUCT_NAME), VAL("VER", VERSION_STRING), TXT(COPYRIGHT_NOTICE));
}

void CbuildGen::Usage(void) {
  LogMsg("M020", VAL("HELP", USAGE));
}

void CbuildGen::ShowVersion(void) {
  LogMsg("M022", VAL("EXE", ORIGINAL_FILENAME), VAL("VER", VERSION_STRING), TXT(COPYRIGHT_NOTICE));
}

int CbuildGen::RunCbuildGen(int argc, char **argv, char** envp) {
  bool cmdlineErr = false;
  CbuildGen console;

  ErrLog::Get()->SetOutputter(new ErrOutputterSaveToStdoutOrFile());
  ErrLog::Get()->SetLevel(MsgLevel::LEVEL_INFO);
  InitMessageTable();

  if (argc <= 1) {
    // Print Signature and Usage
    console.Signature();
    console.Usage();
    return 0;
  }
 
  // Command line options
  cxxopts::Options options(ORIGINAL_FILENAME);
  cxxopts::ParseResult parseResult;

  cxxopts::Option toolchain   ("toolchain", "Path to toolchain to be used", cxxopts::value<string>());
  cxxopts::Option update      ("update", "CprjFile to be generated with fixed versions for reproducing the current build", cxxopts::value<string>());
  cxxopts::Option intDir      ("intdir", "Path to intermediate directory", cxxopts::value<string>());
  cxxopts::Option outDir      ("outdir", "Path to output directory", cxxopts::value<string>());
  cxxopts::Option updateRte   ("update-rte", "Update the RTE directory and files", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option quiet       ("quiet", "Run cbuildgen silently, printing only error messages", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option layer       ("layer", "Optional layer(s) ID", cxxopts::value<vector<string>>());
  cxxopts::Option name        ("name", "Name of the project to be composed", cxxopts::value<string>());
  cxxopts::Option description ("description", "Description of the project to be composed", cxxopts::value<string>());
  cxxopts::Option except      ("except", "File or child directory exceptionally not deleted by rmdir command", cxxopts::value<string>());
  cxxopts::Option packRoot    ("pack-root", "Path to the CMSIS-Pack root directory that stores software packs", cxxopts::value<string>());
  cxxopts::Option compilerRoot("compiler-root", "Path to the installation 'etc' directory", cxxopts::value<string>());
  cxxopts::Option cprjFile    ("cprjfile", "CMSIS Project Description file", cxxopts::value<string>());
  cxxopts::Option args        ("args", "", cxxopts::value<vector<string>>());
  cxxopts::Option help        ("h,help", "Print usage");
  cxxopts::Option version     ("V,version", "Print version");

  // command options dictionary
  map<string, pair<vector<cxxopts::Option>, string>> optionsDict = {
    // { "Command", {<options...>, "positional arg help"}}
    {"packlist", {{toolchain, update, intDir, outDir, quiet},            "<ProjectFile>.cprj"}},
    {"cmake",    {{toolchain, update, intDir, outDir, updateRte, quiet}, "<ProjectFile>.cprj"}},
    {"extract",  {{layer, outDir},                                       "<ProjectFile>.cprj"}},
    {"remove",   {{layer},                                               "<ProjectFile>.cprj"}},
    {"compose",  {{name, description},                                   "<ProjectFile>.cprj <1.clayer>...<N.clayer>"}},
    {"add",      {{},                                                    "<ProjectFile>.cprj <1.clayer>...<N.clayer>"}},
    {"mkdir",    {{},                                                    "<path1>...<pathN>"}},
    {"touch",    {{},                                                    "<filepath1>...<filepathN>"}},
    {"rmdir",    {{except},                                              "<path1>...<pathN>"}},
  };

  string cprjFilePath, intDirPath, outDirPath, projectName, projectDesc;
  string toolchainPath, updateCPRJ, packRootPath, compilerRootPath, exceptPath;
  vector<string> posArgs;
  vector<string> layerIDs;
  bool updateRteFiles = false;

  try {
    options
      .set_width(80)
      .add_options(ORIGINAL_FILENAME, {
        cprjFile, args, toolchain,
        update, intDir, outDir, quiet,
        layer, name, description, packRoot,
        compilerRoot, except, help, version,
        updateRte
      });

    options.parse_positional({"args"});
    parseResult = options.parse(argc, argv);
  }
  catch (cxxopts::OptionException& e) {
    LogMsg("M217", VAL("MSG", e.what()));
    console.Usage();
    return 1;
  }

  if (parseResult.count("quiet") && parseResult["quiet"].as<bool>()) {
    // quiet mode
    ErrLog::Get()->SuppressAllInfo();
  }

  if (parseResult.count("version")) {
    // show version
    console.ShowVersion();
    return 0;
  }

  if (parseResult.count("args")) {
    posArgs = parseResult["args"].as<vector<string>>();
  }
  else if (parseResult.count("help")) {
    // show help
    console.Signature();
    console.Usage();
    return 0;
  }

  if (parseResult.count("except")) {
    exceptPath = parseResult["except"].as<string>();
  }

  if (parseResult.count("intdir")) {
    intDirPath = parseResult["intdir"].as<string>();
  }

  if (parseResult.count("outdir")) {
    outDirPath = parseResult["outdir"].as<string>();
  }

  if (parseResult.count("pack-root")) {
    packRootPath = parseResult["pack-root"].as<string>();
  }

  if (parseResult.count("compiler-root")) {
    compilerRootPath = parseResult["compiler-root"].as<string>();
  }

  if (parseResult.count("layer")) {
    auto layers = parseResult["layer"].as<vector<string>>();
    layerIDs.insert(layerIDs.begin(), layers.begin(), layers.end());
  }

  if (parseResult.count("name")) {
    projectName = parseResult["name"].as<string>();
  }

  if (parseResult.count("description")) {
    projectDesc = parseResult["description"].as<string>();
  }

  if (parseResult.count("toolchain")) {
    toolchainPath = parseResult["toolchain"].as<string>();
  }

  if (parseResult.count("update")) {
    updateCPRJ = parseResult["update"].as<string>();
  }

  if (parseResult.count("update-rte")) {
    updateRteFiles = parseResult["update-rte"].as<bool>();
  }

  bool mkdirCmd = false, rmdirCmd = false, touchCmd = false, packMode = false, cmakeMode = false;
  bool extractLayer = false, composeLayer = false, addLayer = false, removeLayer = false;
  string command = "UNKNOWN";
  list<string> params;

  for (const string& arg : posArgs) {
    bool bParam = false;
    if      (arg.compare("mkdir")    == 0)      mkdirCmd = true;
    else if (arg.compare("rmdir")    == 0)      rmdirCmd = true;
    else if (arg.compare("touch")    == 0)      touchCmd = true;
    else if (arg.compare("packlist") == 0)      packMode = true;
    else if (arg.compare("cmake")    == 0)      cmakeMode = true;
    else if (arg.compare("extract")  == 0)      extractLayer = true;
    else if (arg.compare("compose")  == 0)      composeLayer = true;
    else if (arg.compare("add")      == 0)      addLayer = true;
    else if (arg.compare("remove")   == 0)      removeLayer = true;
    else if (arg.find(".cprj") != string::npos) cprjFilePath = arg;
    else {
      bParam = true;
      params.push_back(arg);
    }
    command = bParam ? command : arg;
  }

  int cnt = 0;
  for (auto b : { mkdirCmd, rmdirCmd, touchCmd, packMode, cmakeMode, extractLayer, composeLayer, addLayer, removeLayer }) {
    if (b) {
      cnt++;
    }
  }
  if (cnt > 1) {
    // Multiple commands were given
    LogMsg("M207");
    return 1;
  }

  if (cnt == 0) {
    // No command was given
    LogMsg("M206");
    cmdlineErr = true;
  }

  // print command help
  if (parseResult.count("help")) {
    return console.PrintUsage(optionsDict, command) ? 0 : 1;
  }

  // Auxiliary commands
  if (mkdirCmd || rmdirCmd || touchCmd) {
    console.Signature();
    int cmd = mkdirCmd ? AUX_MKDIR : rmdirCmd ? AUX_RMDIR : touchCmd ? AUX_TOUCH : 0;
    AuxCmd auxcmd = AuxCmd();
    if (!auxcmd.RunAuxCmd(cmd, params, exceptPath)) {
      return 1;
    }
    ErrLog::Get()->SetQuietMode();
    return 0;
  }

  if (cprjFilePath.empty()) {
    // No CPRJ file was given
    LogMsg("M202");
    cmdlineErr = true;
  }

  if (extractLayer && outDirPath.empty()) {
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

  if (packRootPath.empty()) {
    packRootPath = CrossPlatformUtils::GetEnv("CMSIS_PACK_ROOT");
    if (packRootPath.empty()) {
      packRootPath = CrossPlatformUtils::GetDefaultCMSISPackRootDir();
    }
  }

  if (compilerRootPath.empty()) {
    compilerRootPath = CrossPlatformUtils::GetEnv("CMSIS_COMPILER_ROOT");
    if (compilerRootPath.empty()) {
      error_code ec;
      string currExePath = CrossPlatformUtils::GetExecutablePath(ec);
      if (currExePath.empty()) {
        LogMsg("M216", VAL("MSG", ec.message()));
        return 1;
      }
      compilerRootPath = fs::path(currExePath).parent_path().parent_path().append("etc").generic_string();
      if (!fs::exists(compilerRootPath)) {
        LogMsg("M204", VAL("PATH", compilerRootPath));
        return 1;
      }
    }
  }

  // Environment variables
  vector<string> envVars;
  if (envp) {
    for (char** env = envp; *env != 0; env++) {
      envVars.push_back(string(*env));
    }
  }

  vector<string> layers(params.begin(), params.end());
  // Run layer command
  int cmd = extractLayer ? L_EXTRACT : composeLayer ? L_COMPOSE : addLayer ? L_ADD : removeLayer ? L_REMOVE : 0;
  vector<string>* pLayer = NULL;
  switch (cmd) {
  case L_EXTRACT:
  case L_REMOVE:
    pLayer = &layerIDs;
    break;
  case L_COMPOSE:
  case L_ADD:
    pLayer = &layers;
    break;
  }

  if (cmd != 0) {
    if (RunLayer(cmd, { cprjFilePath, packRootPath, compilerRootPath, *pLayer, envVars, projectName, projectDesc, outDirPath })) {
      // layer command run successfully
      LogMsg("M650");
      return 0;
    }
    else {
      // layer command run failed
      return 1;
    }
  }

  if ((cmakeMode || packMode) && (!CreateRte({ cprjFilePath, packRootPath, compilerRootPath, toolchainPath, updateCPRJ, intDirPath, envVars, packMode, updateRteFiles }))) {
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
    if (!instance.Collect(cprjFilePath, model, outDirPath, intDirPath, compilerRootPath)) {
      return 1;
    }
    if (instance.GenBuildCMakeLists()) {
      LogMsg("M652", VAL("NAME", instance.m_genfile));
    }
    instance.GenAuditFile();
  }
  return 0;
}

bool CbuildGen::PrintUsage(
  const map<string, pair<vector<cxxopts::Option>, string>>& cmdOptionsDict,
  const std::string& cmd)
{
  // print signature
  Signature();

  if (cmd.empty()) {
    Usage();
    return true;
  }

  if (cmdOptionsDict.find(cmd) == cmdOptionsDict.end()) {
    LogMsg("M219", VAL("CMD", cmd));
    return false;
  }

  // print command help
  cxxopts::Options options(ORIGINAL_FILENAME + string(" ") + cmd);
  auto cmdOptions = cmdOptionsDict.at(cmd);
  options
    .positional_help(cmdOptions.second)
    .show_positional_help();

  for (auto& option : cmdOptions.first) {
    options
      .set_width(80)
      .add_option(cmd, option);
  }
  options.parse_positional({RteUtils::EMPTY_STRING});

  if (cmdOptions.first.empty()) {
    // overwrite default custom help
    options.custom_help(RteUtils::EMPTY_STRING);
  }

  LogMsg("M020", VAL("HELP", options.help()));
  return true;
}

