/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrParser.h"
#include "ProjMgrLogger.h"
#include "ProjMgrUtils.h"
#include "ProductInfo.h"
#include "RteFsUtils.h"

#include "CrossPlatformUtils.h"

#include <algorithm>
#include <functional>

using namespace std;

static constexpr const char* USAGE = "\n\
Usage:\n\
  csolution <command> [<name>.csolution.yml] [options]\n\n\
Commands:\n\
  convert                       Convert user input *.yml files to *.cprj files\n\
  list boards                   Print list of available board names\n\
  list configs                  Print list of configuration files\n\
  list contexts                 Print list of contexts in a <name>.csolution.yml\n\
  list components               Print list of available components\n\
  list dependencies             Print list of unresolved project dependencies\n\
  list devices                  Print list of available device names\n\
  list environment              Print list of environment configurations\n\
  list generators               Print list of code generators of a given context\n\
  list layers                   Print list of available, referenced and compatible layers\n\
  list packs                    Print list of used packs from the pack repository\n\
  list toolchains               Print list of supported toolchains\n\
  run                           Run code generator\n\
  update-rte                    Create/update configuration files and validate solution\n\n\
Options:\n\
  -c, --context arg [...]       Input context names [<project-name>][.<build-type>][+<target-type>]\n\
  -d, --debug                   Enable debug messages\n\
  -D, --dry-run                 Enable dry-run\n\
  -e, --export arg              Set suffix for exporting <context><suffix>.cprj retaining only specified versions\n\
  -f, --filter arg              Filter words\n\
  -g, --generator arg           Code generator identifier\n\
  -l, --load arg                Set policy for packs loading [latest | all | required]\n\
  -L, --clayer-path arg         Set search path for external clayers\n\
  -m, --missing                 List only required packs that are missing in the pack repository\n\
  -n, --no-check-schema         Skip schema check\n\
  -N, --no-update-rte           Skip creation of RTE directory and files\n\
  -o,-O --output arg            Add prefix to 'outdir' and 'tmpdir'\n\
  -q, --quiet                   Run silently, printing only error messages\n\
  -R, --relative-paths          Print paths relative to project or ${CMSIS_PACK_ROOT}\n\
  -S, --context-set             Select the context names from cbuild-set.yml for generating the target application\n\
  -t, --toolchain arg           Selection of the toolchain used in the project optionally with version\n\
  -v, --verbose                 Enable verbose messages\n\
  -V, --version                 Print version\n\n\
Use 'csolution <command> -h' for more information about a command.\n\
";

ProjMgr::ProjMgr() :
  m_parser(),
  m_extGenerator(&m_parser),
  m_worker(&m_parser, &m_extGenerator),
  m_checkSchema(false),
  m_missingPacks(false),
  m_updateRteFiles(true),
  m_verbose(false),
  m_debug(false),
  m_dryRun(false),
  m_ymlOrder(false),
  m_contextSet(false),
  m_relativePaths(false),
  m_frozenPacks(false),
  m_updateIdx(false)
{
}

ProjMgr::~ProjMgr() {
  // Reserved
}

bool ProjMgr::PrintUsage(
  const map<string, pair<bool, vector<cxxopts::Option>>>& cmdOptionsDict,
  const std::string& cmd, const std::string& subCmd)
{
  string signature = string("csolution: Project Manager ") + VERSION_STRING + string(" ") + COPYRIGHT_NOTICE;
  if (cmd.empty() && subCmd.empty()) {
    // print main help
    cout << signature << endl;
    cout << USAGE << endl;
    return true;
  }

  string filter = cmd + (subCmd.empty() ? "" : (string(" ") + subCmd));
  if (cmdOptionsDict.find(filter) == cmdOptionsDict.end()) {
    ProjMgrLogger::Get().Error("'" + filter + "' is not a valid command. See 'csolution --help'.");
    return false;
  }

  // print command help
  cout << signature << endl;
  auto [optionalArg, cmdOptions] = cmdOptionsDict.at(filter);

  string program = ORIGINAL_FILENAME + string(" ") + cmd +
    (subCmd.empty() ? "" : (string(" ") + subCmd));

  if (!cmdOptions.empty()) {
    // Add positional help
    program += (optionalArg ? " [" : " <") + string("csolution.yml") + (optionalArg ? "]" : ">");
  }

  cxxopts::Options options(program);
  for (auto& option : cmdOptions) {
    options.add_option(filter, option);
  }

  if (cmdOptions.empty()) {
    // overwrite default custom help
    options.custom_help(RteUtils::EMPTY_STRING);
  }

  cout << options.help() << endl;
  return true;
}

void ProjMgr::ShowVersion(void) {
  cout << ORIGINAL_FILENAME << " " << VERSION_STRING << " " << COPYRIGHT_NOTICE << endl;
}

int ProjMgr::ParseCommandLine(int argc, char** argv) {

  // Command line options
  cxxopts::Options options(ORIGINAL_FILENAME);
  cxxopts::ParseResult parseResult;

  cxxopts::Option solution("s,solution", "Input csolution.yml file", cxxopts::value<string>());
  cxxopts::Option context("c,context", "Input context names [<project-name>][.<build-type>][+<target-type>]", cxxopts::value<std::vector<std::string>>());
  cxxopts::Option filter("f,filter", "Filter words", cxxopts::value<string>());
  cxxopts::Option help("h,help", "Print usage");
  cxxopts::Option generator("g,generator", "Code generator identifier", cxxopts::value<string>());
  cxxopts::Option load("l,load", "Set policy for packs loading [latest | all | required]", cxxopts::value<string>());
  cxxopts::Option clayerSearchPath("L,clayer-path", "Set search path for external clayers", cxxopts::value<string>());
  cxxopts::Option missing("m,missing", "List only required packs that are missing in the pack repository", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option schemaCheck("n,no-check-schema", "Skip schema check", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option noUpdateRte("N,no-update-rte", "Skip creation of RTE directory and files", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option output("o,output", "Add prefix to 'outdir' and 'tmpdir'", cxxopts::value<string>());
  cxxopts::Option outputAlt("O", "Add prefix to 'outdir' and 'tmpdir'", cxxopts::value<string>());
  cxxopts::Option version("V,version", "Print version");
  cxxopts::Option verbose("v,verbose", "Enable verbose messages", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option debug("d,debug", "Enable debug messages", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option dryRun("D,dry-run", "Enable dry-run", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option exportSuffix("e,export", "Set suffix for exporting <context><suffix>.cprj retaining only specified versions", cxxopts::value<string>());
  cxxopts::Option toolchain("t,toolchain", "Selection of the toolchain used in the project optionally with version", cxxopts::value<string>());
  cxxopts::Option ymlOrder("yml-order", "Preserve order as specified in input yml", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option contextSet("S,context-set", "Select the context names from cbuild-set.yml for generating the target application", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option relativePaths("R,relative-paths", "Output paths relative to project or to CMSIS_PACK_ROOT", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option frozenPacks("frozen-packs", "The list of packs from cbuild-pack.yml is frozen and raises error if not up-to-date", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option updateIdx("update-idx", "Update cbuild-idx file with layer info", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option quiet("q,quiet", "Run silently, printing only error messages", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option cbuildgen("cbuildgen", "Generate legacy *.cprj files", cxxopts::value<bool>()->default_value("false"));

  // command options dictionary
  map<string, std::pair<bool, vector<cxxopts::Option>>> optionsDict = {
    // command, optional args, options
    {"update-rte",        { false, {context, contextSet, debug, load, quiet, schemaCheck, toolchain, verbose, frozenPacks}}},
    {"convert",           { false, {context, contextSet, debug, exportSuffix, load, quiet, schemaCheck, noUpdateRte, output, outputAlt, toolchain, verbose, frozenPacks, cbuildgen}}},
    {"run",               { false, {context, contextSet, debug, generator, load, quiet, schemaCheck, verbose, dryRun}}},
    {"list packs",        { true,  {context, contextSet, debug, filter, load, missing, quiet, schemaCheck, toolchain, verbose, relativePaths}}},
    {"list boards",       { true,  {context, contextSet, debug, filter, load, quiet, schemaCheck, toolchain, verbose}}},
    {"list devices",      { true,  {context, contextSet, debug, filter, load, quiet, schemaCheck, toolchain, verbose}}},
    {"list configs",      { true,  {context, contextSet, debug, filter, load, quiet, schemaCheck, toolchain, verbose}}},
    {"list components",   { true,  {context, contextSet, debug, filter, load, quiet, schemaCheck, toolchain, verbose}}},
    {"list dependencies", { false, {context, contextSet, debug, filter, load, quiet, schemaCheck, toolchain, verbose}}},
    {"list contexts",     { false, {debug, filter, quiet, schemaCheck, verbose, ymlOrder}}},
    {"list generators",   { false, {context, contextSet, debug, load, quiet, schemaCheck, toolchain, verbose}}},
    {"list layers",       { false, {context, contextSet, debug, load, clayerSearchPath, quiet, schemaCheck, toolchain, verbose, updateIdx}}},
    {"list toolchains",   { false, {context, contextSet, debug, quiet, toolchain, verbose}}},
    {"list environment",  { true,  {}}},
  };

  try {
    options.add_options("", {
      {"positional", "", cxxopts::value<vector<string>>()},
      solution, context, contextSet, filter, generator,
      load, clayerSearchPath, missing, schemaCheck, noUpdateRte, output, outputAlt,
      help, version, verbose, debug, dryRun, exportSuffix, toolchain, ymlOrder,
      relativePaths, frozenPacks, updateIdx, quiet, cbuildgen
    });
    options.parse_positional({ "positional" });

    parseResult = options.parse(argc, argv);
    m_checkSchema = !parseResult.count("n");
    m_worker.SetCheckSchema(m_checkSchema);
    m_extGenerator.SetCheckSchema(m_checkSchema);
    m_missingPacks = parseResult.count("m");
    m_updateRteFiles = !parseResult.count("no-update-rte");
    m_verbose = parseResult.count("v");
    m_worker.SetVerbose(m_verbose);
    m_debug = parseResult.count("d");
    m_dryRun = parseResult.count("D");
    m_worker.SetDebug(m_debug);
    m_worker.SetDryRun(m_dryRun);
    m_ymlOrder = parseResult.count("yml-order");
    m_updateIdx = parseResult.count("update-idx");
    m_worker.SetUpCommand(m_updateIdx);
    m_contextSet = parseResult.count("context-set");
    m_relativePaths = parseResult.count("relative-paths");
    m_worker.SetPrintRelativePaths(m_relativePaths);
    m_frozenPacks = parseResult.count("frozen-packs");
    m_cbuildgen = parseResult.count("cbuildgen");
    m_worker.SetCbuild2Cmake(!m_cbuildgen);
    ProjMgrLogger::m_quiet = parseResult.count("quiet");

    vector<string> positionalArguments;
    if (parseResult.count("positional")) {
      positionalArguments = parseResult["positional"].as<vector<string>>();
    }
    else if (parseResult.count("version")) {
      ShowVersion();
      return -1;
    }
    else {
      // No command was given, print usage and return success
      return PrintUsage(optionsDict, "", "") ? -1 : 1;
    }

    for (auto it = positionalArguments.begin(); it != positionalArguments.end(); it++) {
      if (regex_match(*it, regex(".*\\.csolution\\.(yml|yaml)"))) {
        m_csolutionFile = *it;
      } else if (m_command.empty()) {
        m_command = *it;
      } else if (m_args.empty()) {
        m_args = *it;
      }
    }
    if (parseResult.count("solution")) {
      m_csolutionFile = parseResult["solution"].as<string>();
    }
    if (!m_csolutionFile.empty()) {
      if (!RteFsUtils::Exists(m_csolutionFile)) {
        ProjMgrLogger::Get().Error("csolution file was not found", "", m_csolutionFile);
        return ErrorCode::ERROR;
      }
      m_csolutionFile = RteFsUtils::MakePathCanonical(m_csolutionFile);
      m_rootDir = RteUtils::ExtractFilePath(m_csolutionFile, false);
      m_worker.SetRootDir(m_rootDir);
    }
    if (parseResult.count("context")) {
      m_context = parseResult["context"].as<vector<string>>();
    }
    if (parseResult.count("filter")) {
      m_filter = parseResult["filter"].as<string>();
    }
    if (parseResult.count("generator")) {
      m_codeGenerator = parseResult["generator"].as<string>();
    }
    if (parseResult.count("load")) {
      m_loadPacksPolicy = parseResult["load"].as<string>();
    }
    if (parseResult.count("clayer-path")) {
      m_clayerSearchPath = parseResult["clayer-path"].as<string>();
    }
    if (parseResult.count("export")) {
      m_export = parseResult["export"].as<string>();
    }
    if (parseResult.count("toolchain")) {
      m_selectedToolchain = parseResult["toolchain"].as<string>();
    }
    if (parseResult.count("output") || parseResult.count("O")) {
      const std::string& key = parseResult.count("output") ? "output" : "O";
      m_outputDir = parseResult[key].as<std::string>();
      m_outputDir = RteFsUtils::AbsolutePath(m_outputDir).generic_string();
    }
  } catch (cxxopts::OptionException& e) {
    ProjMgrLogger::Get().Error(e.what());
    return ErrorCode::ERROR;
  }

  // Unmatched items in the parse result
  if (!parseResult.unmatched().empty()) {
    ProjMgrLogger::Get().Error("too many command line arguments");
    return ErrorCode::ERROR;
  }

  if (parseResult.count("help")) {
    return PrintUsage(optionsDict, m_command, m_args) ? -1 : 1;
  }

  // Set load packs policy
  if (!SetLoadPacksPolicy()) {
    return ErrorCode::ERROR;
  }
  return ErrorCode::SUCCESS;
}


int ProjMgr::RunProjMgr(int argc, char** argv, char** envp) {
  ProjMgr manager;

  int res = manager.ParseCommandLine(argc, argv);
  if(res != 0) {
    // res == -1  means help or version is requested => program success
    return res > 0 ? res : ErrorCode::SUCCESS;
  }

  // Environment variables
  vector<string> envVars;
  if(envp) {
    for(char** env = envp; *env != 0; env++) {
      envVars.push_back(string(*env));
    }
  }
  manager.m_worker.SetEnvironmentVariables(envVars);
  if(manager.m_worker.InitializeModel()) {
    res = manager.ProcessCommands();
  } else {
    res = ErrorCode::ERROR;
  }
  return res;
}

int ProjMgr::ProcessCommands() {
  if (m_command == "list") {
    // Process 'list' command
    if (m_args.empty()) {
      ProjMgrLogger::Get().Error("list <args> was not specified");
      return ErrorCode::ERROR;
    }
    // Process argument
    if (m_args == "packs") {
      if (!RunListPacks()) {
        return ErrorCode::ERROR;
      }
    } else if (m_args == "boards") {
      if (!RunListBoards()) {
        return ErrorCode::ERROR;
      }
    } else if (m_args == "devices") {
      if (!RunListDevices()) {
        return ErrorCode::ERROR;
      }
    } else if (m_args == "components") {
      if (!RunListComponents()) {
        return ErrorCode::ERROR;
      }
    } else if (m_args == "configs") {
      if (!RunListConfigs()) {
        return ErrorCode::ERROR;
      }
    } else if (m_args == "dependencies") {
      if (!RunListDependencies()) {
        return ErrorCode::ERROR;
      }
    } else if (m_args == "contexts") {
      if (!RunListContexts()) {
        return ErrorCode::ERROR;
      }
    } else if (m_args == "generators") {
      if (!RunListGenerators()) {
        return ErrorCode::ERROR;
      }
    } else if (m_args == "layers") {
      if (!RunListLayers()) {
        return ErrorCode::ERROR;
      }
    } else if (m_args == "toolchains") {
      if (!RunListToolchains()) {
        return ErrorCode::ERROR;
      }
    } else if (m_args == "environment") {
      RunListEnvironment();
    }
    else {
      ProjMgrLogger::Get().Error("list <args> was not found");
      return ErrorCode::ERROR;
    }
  } else if (m_command == "update-rte") {
    // Process 'update-rte' command
    if (!RunConfigure()) {
      return ErrorCode::ERROR;
    }
  } else if (m_command == "convert") {
    // Process 'convert' command
    bool convSuccess = RunConvert();
    // Check if layer variable not defined (regardless of conversion status)
    if (m_worker.HasVarDefineError()) {
      return ErrorCode::VARIABLE_NOT_DEFINED;
    }
    if (!convSuccess) {
      return ErrorCode::ERROR;
    }
  } else if (m_command == "run") {
    // Process 'run' command
    if (!RunCodeGenerator()) {
      return ErrorCode::ERROR;
    }
  } else {
    ProjMgrLogger::Get().Error("<command> was not found");
    return ErrorCode::ERROR;
  }
  return ErrorCode::SUCCESS;
}

// Set load packs policy
bool ProjMgr::SetLoadPacksPolicy(void) {
  if (m_loadPacksPolicy.empty()) {
    m_worker.SetLoadPacksPolicy(LoadPacksPolicy::DEFAULT);
  } else if (m_loadPacksPolicy == "latest") {
    m_worker.SetLoadPacksPolicy(LoadPacksPolicy::LATEST);
  } else if (m_loadPacksPolicy == "all") {
    m_worker.SetLoadPacksPolicy(LoadPacksPolicy::ALL);
  } else if (m_loadPacksPolicy == "required") {
    m_worker.SetLoadPacksPolicy(LoadPacksPolicy::REQUIRED);
  } else {
    ProjMgrLogger::Get().Error("unknown load option: '" + m_loadPacksPolicy + "', it must be 'latest', 'all' or 'required'");
    return false;
  }
  return true;
}

bool ProjMgr::PopulateContexts(void) {
  if (!m_csolutionFile.empty()) {
    // Parse csolution
    if (!m_parser.ParseCsolution(m_csolutionFile, m_checkSchema, m_frozenPacks)) {
      return false;
    }
    // Check created-for requirement
    if (!ValidateCreatedFor(m_parser.GetCsolution().createdFor)) {
      return false;
    }
    // Parse cdefault
    if (m_parser.GetCsolution().enableCdefault && GetCdefaultFile()) {
      if (!m_parser.ParseCdefault(m_cdefaultFile, m_checkSchema)) {
        return false;
      }
    }
    // Check cproject separate folders and unique names
    const StrVec& cprojects = m_parser.GetCsolution().cprojects;
    if (cprojects.size() > 1) {
      multiset<string> dirs;
      multiset<string> names;
      for (const auto& cproject : cprojects) {
        error_code ec;
        dirs.insert(fs::path(cproject).parent_path().generic_string());
        names.insert(fs::path(cproject).filename().generic_string());
      }
      if (adjacent_find(names.begin(), names.end()) != names.end()) {
        ProjMgrLogger::Get().Error("cproject.yml filenames must be unique", "", m_csolutionFile);
        return false;
      }
      if (adjacent_find(dirs.begin(), dirs.end()) != dirs.end()) {
        ProjMgrLogger::Get().Warn("cproject.yml files should be placed in separate sub-directories", "", m_csolutionFile);
      }
    }
    // Parse cprojects
    for (const auto& cproject : cprojects) {
      error_code ec;
      string const& cprojectFile = fs::canonical(m_rootDir + "/" + cproject, ec).generic_string();
      if (cprojectFile.empty()) {
        ProjMgrLogger::Get().Error("cproject file was not found", "", cproject);
        return false;
      }
      if (!m_parser.ParseCproject(cprojectFile, m_checkSchema)) {
        return false;
      }
    }
  } else {
    ProjMgrLogger::Get().Error("input yml files were not specified");
    return false;
  }

  // Set toolchain
  m_worker.SetSelectedToolchain(m_selectedToolchain);

  // Set output directory
  m_worker.SetOutputDir(m_outputDir);

  // Update tmp directory
  m_worker.UpdateTmpDir();

  // Set root directory
  m_worker.SetRootDir(m_rootDir);

  // Add contexts
  for (auto& descriptor : m_parser.GetCsolution().contexts) {
    error_code ec;
    const string& cprojectFile = fs::path(descriptor.cproject).is_absolute() ?
      descriptor.cproject : fs::canonical(m_rootDir + "/" + descriptor.cproject, ec).generic_string();
    if (!m_worker.AddContexts(m_parser, descriptor, cprojectFile)) {
      return false;
    }
  }

  // Retrieve all context types
  m_worker.RetrieveAllContextTypes();

  return true;
}

bool ProjMgr::GenerateYMLConfigurationFiles() {
  // Generate cbuild pack file
  const bool isUsingContexts = m_contextSet || m_context.size() != 0;
  if (!m_emitter.GenerateCbuildPack(m_parser, m_processedContexts, isUsingContexts, m_frozenPacks, m_checkSchema)) {
    return false;
  }

  // Update the RTE files
  bool result = UpdateRte();

  // Generate cbuild files
  for (auto& contextItem : m_processedContexts) {
    if (!m_emitter.GenerateCbuild(contextItem, m_checkSchema)) {
      result = false;
    }
  }

  // Generate cbuild index file
  if (!m_allContexts.empty()) {
    map<string, ExecutesItem> executes;
    m_worker.GetExecutes(executes);
    if (!m_emitter.GenerateCbuildIndex(m_parser, m_worker, m_processedContexts, m_outputDir, m_failedContext, executes, m_checkSchema)) {
      return false;
    }
  }

  return result;
}

bool ProjMgr::ParseAndValidateContexts() {
  // Parse context selection
  if (!m_worker.ParseContextSelection(m_context, m_contextSet)) {
    return false;
  }

  if (m_contextSet) {
    const auto& selectedContexts = m_worker.GetSelectedContexts();
    m_selectedToolchain = m_worker.GetSelectedToochain();
    if (!selectedContexts.empty()) {
      const string& cbuildSetFile = (m_outputDir.empty() ? m_parser.GetCsolution().directory : m_outputDir) + "/" +
      m_parser.GetCsolution().name + ".cbuild-set.yml";
      // Generate cbuild-set file
      if (!m_emitter.GenerateCbuildSet(selectedContexts, m_selectedToolchain, cbuildSetFile, m_checkSchema)) {
        return false;
      }
    }
  }

  return true;
}

bool ProjMgr::Configure() {
  // Parse all input files and populate contexts inputs
  if (!PopulateContexts()) {
    return false;
  }

  if (!ParseAndValidateContexts()) {
    return false;
  }

  if (m_worker.HasVarDefineError()) {
    auto undefVars = m_worker.GetUndefLayerVars();
    string errMsg = "undefined variables in "+ fs::path(m_csolutionFile).filename().generic_string() +":\n";
    for (const string& var : undefVars) {
      errMsg += "  - $" + var + "$\n";
    }
    ProjMgrLogger::Get().Error(errMsg);
  }

  // Get context pointers
  map<string, ContextItem>* contexts = nullptr;
  m_worker.GetContexts(contexts);

  vector<string> orderedContexts;
  m_worker.GetYmlOrderedContexts(orderedContexts);

  // Process contexts
  bool error = false;
  m_allContexts.clear();
  m_processedContexts.clear();
  m_failedContext.clear();
  for (auto& contextName : orderedContexts) {
    auto& contextItem = (*contexts)[contextName];
    m_allContexts.push_back(&contextItem);
    if (!m_worker.IsContextSelected(contextName)) {
      continue;
    }
    if (!m_worker.ProcessContext(contextItem, true, true, false)) {
      ProjMgrLogger::Get().Error("processing context '" + contextName + "' failed", contextName);
      m_failedContext.insert(contextItem.name);
      error = true;
    }
    m_processedContexts.push_back(&contextItem);
  }

  if (m_worker.HasToolchainErrors()) {
    error = true;
  }

  m_selectedToolchain = m_worker.GetSelectedToochain();

  // Process solution level executes
  if (!m_worker.ProcessSolutionExecutes()) {
    error = true;
  }
  // Process executes dependencies
  m_worker.ProcessExecutesDependencies();

  // Check missing files
  if (!m_worker.CheckMissingFiles()) {
    error = true;
  }

  // Collect unused packs
  m_worker.CollectUnusedPacks();

  // Print warnings for missing filters
  m_worker.PrintMissingFilters();
  if (m_verbose) {
    // Print config files info
    vector<string> configFiles;
    m_worker.ListConfigFiles(configFiles);
    if (!configFiles.empty()) {
      string infoMsg = "config files for each component:";
      for (const auto& configFile : configFiles) {
        infoMsg += "\n  " + configFile;
      }
      ProjMgrLogger::Get().Info(infoMsg);
    }
  }

  return !error;
}

bool ProjMgr::UpdateRte() {
  // Update the RTE files
  if (m_updateRteFiles) {
    for (auto& contextItem : m_processedContexts) {
      if (contextItem->rteActiveProject != nullptr) {
        contextItem->rteActiveProject->SetAttribute("update-rte-files", "1");
        contextItem->rteActiveProject->UpdateRte();
      }
    }
  }
  bool result = true;
  for (auto& contextItem : m_processedContexts) {
    // Check PLM files
    if (!m_worker.CheckConfigPLMFiles(*contextItem)) {
      m_failedContext.insert(contextItem->name);
      result = false;
    }
  }
  return result;
}

bool ProjMgr::RunConfigure() {
  bool success = Configure() && UpdateRte();
  return success;
}

bool ProjMgr::RunConvert(void) {
  // Configure
  bool Success = Configure();

  // Generate YML build configuration files
  Success &= GenerateYMLConfigurationFiles();

  // Generate Cprjs
  if (m_cbuildgen) {
    for (auto& contextItem : m_processedContexts) {
      const string filename = RteFsUtils::MakePathCanonical(contextItem->directories.cprj + "/" + contextItem->name + ".cprj");
      RteFsUtils::CreateDirectories(contextItem->directories.cprj);
      if (m_generator.GenerateCprj(*contextItem, filename)) {
        ProjMgrLogger::Get().Info("file generated successfully", contextItem->name, filename);
      } else {
        ProjMgrLogger::Get().Error("file cannot be written", contextItem->name, filename);
        return false;
      }
      if (!m_export.empty()) {
        // Generate non-locked Cprj
        const string exportfilename = RteFsUtils::MakePathCanonical(contextItem->directories.cprj + "/" + contextItem->name + m_export + ".cprj");
        if (m_generator.GenerateCprj(*contextItem, exportfilename, true)) {
          ProjMgrLogger::Get().Info("export file generated successfully", contextItem->name, exportfilename);
        } else {
          ProjMgrLogger::Get().Error("export file cannot be written", contextItem->name, exportfilename);
          return false;
        }
      }
    }
  }

  return Success;
}

bool ProjMgr::RunListPacks(void) {
  if (!m_csolutionFile.empty()) {
    // Parse all input files and create contexts
    if (!PopulateContexts()) {
      return false;
    }
  }

  // Parse context selection
  if (!ParseAndValidateContexts()) {
    return false;
  }

  vector<string> packs;
  bool ret = m_worker.ListPacks(packs, m_missingPacks, m_filter);
  for (const auto& pack : packs) {
    cout << pack << endl;
  }
  return ret;
}

bool ProjMgr::RunListBoards(void) {
  if (!m_csolutionFile.empty()) {
    // Parse all input files and create contexts
    if (!PopulateContexts()) {
      return false;
    }
  }

  // Parse context selection
  if (!ParseAndValidateContexts()) {
    return false;
  }

  vector<string> boards;
  if (!m_worker.ListBoards(boards, m_filter)) {
    ProjMgrLogger::Get().Error("processing boards list failed");
    return false;
  }
  for (const auto& device : boards) {
    cout << device << endl;
  }
  return true;
}

bool ProjMgr::RunListDevices(void) {
  if (!m_csolutionFile.empty()) {
    // Parse all input files and create contexts
    if (!PopulateContexts()) {
      return false;
    }
  }

  // Parse context selection
  if (!ParseAndValidateContexts()) {
    return false;
  }

  vector<string> devices;
  if (!m_worker.ListDevices(devices, m_filter)) {
    ProjMgrLogger::Get().Error("processing devices list failed");
    return false;
  }
  for (const auto& device : devices) {
    cout << device << endl;
  }
  return true;
}

bool ProjMgr::RunListComponents(void) {
  if (!m_csolutionFile.empty()) {
    // Parse all input files and create contexts
    if (!PopulateContexts()) {
      return false;
    }
  }

  // Parse context selection
  if (!ParseAndValidateContexts()) {
    return false;
  }

  vector<string> components;
  if (!m_worker.ListComponents(components, m_filter)) {
    ProjMgrLogger::Get().Error("processing components list failed");
    return false;
  }

  for (const auto& component : components) {
    cout << component << endl;
  }
  return true;
}

bool ProjMgr::RunListConfigs() {
  if (!m_csolutionFile.empty()) {
    // Parse all input files and create contexts
    if (!PopulateContexts()) {
      return false;
    }
  }

  // Parse context selection
  if (!ParseAndValidateContexts()) {
    return false;
  }

  vector<string> configFiles;
  if (!m_worker.ListConfigs(configFiles, m_filter)) {
    ProjMgrLogger::Get().Error("processing config list failed");
    return false;
  }

  for (const auto& configFile : configFiles) {
    cout << configFile << endl;
  }
  return true;
}

bool ProjMgr::RunListDependencies(void) {
  // Parse all input files and create contexts
  if (!PopulateContexts()) {
    return false;
  }

  // Parse context selection
  if (!ParseAndValidateContexts()) {
    return false;
  }

  vector<string> dependencies;
  if (!m_worker.ListDependencies(dependencies, m_filter)) {
    ProjMgrLogger::Get().Error("processing dependencies list failed");
    return false;
  }

  for (const auto& dependency : dependencies) {
    cout << dependency << endl;
  }
  return true;
}

bool ProjMgr::RunListContexts(void) {
  // Parse all input files and create contexts
  if (!PopulateContexts()) {
    return false;
  }
  vector<string> contexts;
  if (!m_worker.ListContexts(contexts, m_filter, m_ymlOrder)) {
    ProjMgrLogger::Get().Error("processing contexts list failed");
    return false;
  }
  for (const auto& context : contexts) {
    cout << context << endl;
  }
  return true;
}

bool ProjMgr::RunListGenerators(void) {
  // Parse all input files and create contexts
  if (!PopulateContexts()) {
    return false;
  }

  // Parse context selection
  if (!ParseAndValidateContexts()) {
    return false;
  }

  // Get generators
  vector<string> generators;
  if (!m_worker.ListGenerators(generators)) {
    return false;
  }

  for (const auto& generator : generators) {
    cout << generator << endl;
  }
  return true;
}

bool ProjMgr::RunListLayers(void) {
  // Step1 : Parse all input files and create contexts
  if (!m_csolutionFile.empty()) {
    if (!PopulateContexts()) {
      return false;
    }
  }

  // Step2 : Parse selected contexts
  if (!ParseAndValidateContexts()) {
    return false;
  }

  bool error = m_worker.HasVarDefineError() && !m_updateIdx;
  if (error) {
    auto undefVars = m_worker.GetUndefLayerVars();
    string errMsg = "undefined variables in " + fs::path(m_csolutionFile).filename().generic_string() + ":\n";
    for (const string& var : undefVars) {
      errMsg += "  - $" + var + "$\n";
    }
    ProjMgrLogger::Get().Error(errMsg);
  }

  // Step3: Detect layers and list them
  vector<string> layers;
  error = !m_worker.ListLayers(layers, m_clayerSearchPath, m_failedContext);
  if (error) {
    for (const string& context : m_failedContext) {
      ProjMgrLogger::Get().Error("no compatible software layer found. Review required connections of the project", context);
    }
  }

  if (!m_updateIdx) {
    for (const auto& layer : layers) {
      cout << layer << endl;
    }
  }

  // Step4: Run only when --update-idx flag is used
  // Update the cbuild-idx.yml file with layer information
  if (m_updateIdx) {
    map<string, ContextItem>* contexts = nullptr;
    m_worker.GetContexts(contexts);

    // Check if contexts were properly retrieved
    if (contexts == nullptr) {
      return false;
    }

    // Generate Cbuild index
    m_processedContexts.clear();
    for (auto& contextName : m_worker.GetSelectedContexts()) {
      auto& contextItem = (*contexts)[contextName];
      m_processedContexts.push_back(&contextItem);
    }
    if (!m_processedContexts.empty()) {
      if (!m_emitter.GenerateCbuildIndex(
        m_parser, m_worker, m_processedContexts, m_outputDir,
        m_failedContext, map<string, ExecutesItem>(), m_checkSchema)) {
        return false;
      }
    }
  }
  return !error;
}

bool ProjMgr::RunCodeGenerator(void) {
  // Check input options
  if (m_codeGenerator.empty()) {
    ProjMgrLogger::Get().Error("generator identifier was not specified");
    return false;
  }
  // Parse all input files and create contexts
  if (!PopulateContexts()) {
    return false;
  }
  // Parse context selection
  if (!m_worker.ParseContextSelection(m_context, (m_context.size() == 0) && m_contextSet)) {
    return false;
  }
  if (m_extGenerator.IsGlobalGenerator(m_codeGenerator)) {
    // Run global code generator
    if (!m_worker.ExecuteExtGenerator(m_codeGenerator)) {
      return false;
    }
  } else {
    // Run legacy code generator
    if (!m_worker.ExecuteGenerator(m_codeGenerator)) {
      return false;
    }
  }
  
  if (m_worker.HasToolchainErrors()) {
    return false;
  }
  return true;
}

bool ProjMgr::RunListToolchains(void) {
  if (!m_csolutionFile.empty()) {
    // Parse all input files and create contexts
    if (!PopulateContexts()) {
      return false;
    }
  }

  // Parse context selection
  if (!ParseAndValidateContexts()) {
    return false;
  }

  vector<ToolchainItem> toolchains;
  bool success = m_worker.ListToolchains(toolchains);

  StrSet toolchainsSet;
  for (const auto& toolchain : toolchains) {
    string toolchainEntry = toolchain.name + "@" +
      (toolchain.required.empty() ? toolchain.version : toolchain.required) + "\n";
    if (m_verbose) {
      string env = toolchain.version;
      replace(env.begin(), env.end(), '.', '_');
      if (!toolchain.root.empty()) {
        toolchainEntry += "  Environment: " + toolchain.name + "_TOOLCHAIN_" + env + "\n";
        toolchainEntry += "  Toolchain: " + toolchain.root + "\n";
      }
      if (!toolchain.config.empty()) {
        toolchainEntry += "  Configuration: " + toolchain.config + "\n";
      }
    }
    toolchainsSet.insert(toolchainEntry);
  }
  for (const auto& toolchainEntry : toolchainsSet) {
    cout << toolchainEntry;
  }
  // If the worker has toolchain errors, set the success flag to false
  if (m_worker.HasToolchainErrors()) {
    success = false;
  }
  return success;
}

bool ProjMgr::RunListEnvironment(void) {
  string notFound = "<Not Found>";
  EnvironmentList env;
  m_worker.ListEnvironment(env);
  cout << "CMSIS_PACK_ROOT=" << (env.cmsis_pack_root.empty() ? notFound : env.cmsis_pack_root) << endl;
  cout << "CMSIS_COMPILER_ROOT=" << (env.cmsis_compiler_root.empty() ? notFound : env.cmsis_compiler_root) << endl;
  CrossPlatformUtils::REG_STATUS status = CrossPlatformUtils::GetLongPathRegStatus();
  if (status != CrossPlatformUtils::REG_STATUS::NOT_SUPPORTED) {
    cout << "Long pathname support=" <<
      (status == CrossPlatformUtils::REG_STATUS::ENABLED ? "enabled" : "disabled") << endl;
  }
  return true;
}

bool ProjMgr::GetCdefaultFile(void) {
  error_code ec;
  vector<string> searchPaths = { m_rootDir };
  const string& compilerRoot = m_worker.GetCompilerRoot();
  if (!compilerRoot.empty()) {
    searchPaths.push_back(compilerRoot);
  }
  string cdefaultFile;
  if (!RteFsUtils::FindFileRegEx(searchPaths, ".*/cdefault\\.(yml|yaml)", cdefaultFile)) {
    if (!cdefaultFile.empty()) {
      ProjMgrLogger::Get().Error("multiple cdefault files were found", "", cdefaultFile);
    }
    return false;
  }
  m_cdefaultFile = cdefaultFile;
  return true;
}

bool ProjMgr::ValidateCreatedFor(const string& createdFor) {
  if (!createdFor.empty()) {
    map<string, map<string, string>> registeredToolchains;
    static const regex regEx = regex("(.*)@(\\d+)\\.(\\d+)\\.(\\d+)");
    smatch sm;
    if (regex_match(createdFor, sm, regEx)) {
      string toolName = string(sm[1]);
      transform(toolName.begin(), toolName.end(), toolName.begin(), [](unsigned char c) {
        return std::tolower(c);
      });
      if (toolName == "cmsis-toolbox") {
        const string version = string(sm[2]) + '.' + string(sm[3]) + '.' + string(sm[4]);
        string cmsisToolboxDir = ProjMgrKernel::Get()->GetCmsisToolboxDir();
        string currentVersion = GetToolboxVersion(cmsisToolboxDir);
        if (currentVersion.empty()) {
          return true;
        }
        currentVersion += (":" + currentVersion);
        if (VersionCmp::RangeCompare(version, currentVersion) <= 0) {
          return true;
        } else {
          ProjMgrLogger::Get().Error("solution requires newer CMSIS-Toolbox version " + version, "", m_csolutionFile);
          return false;
        }
      }
    }
    ProjMgrLogger::Get().Warn("solution created for unknown tool: " + createdFor, "", m_csolutionFile);
  }
  return true;
}

string ProjMgr::GetToolboxVersion(const string& toolboxDir) {
  // Find file non recursively under given search directory
  string manifestFilePattern = "manifest_(\\d+\\.\\d+\\.\\d+)(.*).yml";
  string manifestFile;

  if (!RteFsUtils::FindFileWithPattern(toolboxDir, manifestFilePattern, manifestFile)) {
    ProjMgrLogger::Get().Warn("manifest file does not exist", "", toolboxDir);
    return "";
  }

  // Extract the version from filename and match it against the expected pattern
  static const regex regEx = regex(manifestFilePattern);
  smatch matchResult;
  regex_match(manifestFile, matchResult, regEx);
  return matchResult[1].str();
}
