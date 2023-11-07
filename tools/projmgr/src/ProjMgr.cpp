/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
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
      --context-replacement arg Input context replacement name [<project-name>][.<build-type>][+<target-type>]\n\
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
  -o, --output arg              Output directory\n\
  -S, --context-set             Use context set\n\
  -t, --toolchain arg           Selection of the toolchain used in the project optionally with version\n\
  -v, --verbose                 Enable verbose messages\n\
  -V, --version                 Print version\n\n\
Use 'csolution <command> -h' for more information about a command.\n\
";

ProjMgr::ProjMgr(void) :
  m_parser(),
  m_extGenerator(&m_parser),
  m_worker(&m_parser, &m_extGenerator),
  m_checkSchema(false),
  m_updateRteFiles(true) {
  // Reserved
}

ProjMgr::~ProjMgr(void) {
  // Reserved
}

bool ProjMgr::PrintUsage(
  const map<string, pair<bool, vector<cxxopts::Option>>>& cmdOptionsDict,
  const std::string& cmd, const std::string& subCmd)
{
  string signature = PRODUCT_NAME + string(" ") + VERSION_STRING + string(" ") + COPYRIGHT_NOTICE;
  if (cmd.empty() && subCmd.empty()) {
    // print main help
    cout << signature << endl;
    cout << USAGE << endl;
    return true;
  }

  string filter = cmd + (subCmd.empty() ? "" : (string(" ") + subCmd));
  if (cmdOptionsDict.find(filter) == cmdOptionsDict.end()) {
    ProjMgrLogger::Error("'" + filter + "' is not a valid command. See 'csolution --help'.");
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

int ProjMgr::RunProjMgr(int argc, char **argv, char** envp) {
  ProjMgr manager;

  // Command line options
  cxxopts::Options options(ORIGINAL_FILENAME);
  cxxopts::ParseResult parseResult;

  cxxopts::Option solution("s,solution", "Input csolution.yml file", cxxopts::value<string>());
  cxxopts::Option context("c,context", "Input context names [<project-name>][.<build-type>][+<target-type>]", cxxopts::value<std::vector<std::string>>());
  cxxopts::Option contextReplacement("context-replacement", "Input context replacement name [<cproject>][.<build-type>][+<target-type>]", cxxopts::value<std::string>());
  cxxopts::Option filter("f,filter", "Filter words", cxxopts::value<string>());
  cxxopts::Option help("h,help", "Print usage");
  cxxopts::Option generator("g,generator", "Code generator identifier", cxxopts::value<string>());
  cxxopts::Option load("l,load", "Set policy for packs loading [latest | all | required]", cxxopts::value<string>());
  cxxopts::Option clayerSearchPath("L,clayer-path", "Set search path for external clayers", cxxopts::value<string>());
  cxxopts::Option missing("m,missing", "List only required packs that are missing in the pack repository", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option schemaCheck("n,no-check-schema", "Skip schema check", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option noUpdateRte("N,no-update-rte", "Skip creation of RTE directory and files", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option output("o,output", "Output directory", cxxopts::value<string>());
  cxxopts::Option version("V,version", "Print version");
  cxxopts::Option verbose("v,verbose", "Enable verbose messages", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option debug("d,debug", "Enable debug messages", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option dryRun("D,dry-run", "Enable dry-run", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option exportSuffix("e,export", "Set suffix for exporting <context><suffix>.cprj retaining only specified versions", cxxopts::value<string>());
  cxxopts::Option toolchain("t,toolchain","Selection of the toolchain used in the project optionally with version", cxxopts::value<string>());
  cxxopts::Option ymlOrder("yml-order", "Preserve order as specified in input yml", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option contextSet("S,context-set", "Use context set", cxxopts::value<bool>()->default_value("false"));

  // command options dictionary
  map<string, std::pair<bool, vector<cxxopts::Option>>> optionsDict = {
    // command, optional args, options
    {"update-rte",        { false, {context, contextSet, debug, load, schemaCheck, toolchain, verbose}}},
    {"convert",           { false, {context, contextSet, debug, exportSuffix, load, schemaCheck, noUpdateRte, output, toolchain, verbose}}},
    {"run",               { false, {context, debug, generator, load, schemaCheck, verbose, dryRun}}},
    {"list packs",        { true,  {context, debug, filter, load, missing, schemaCheck, toolchain, verbose}}},
    {"list boards",       { true,  {context, debug, filter, load, schemaCheck, toolchain, verbose}}},
    {"list devices",      { true,  {context, debug, filter, load, schemaCheck, toolchain, verbose}}},
    {"list configs",      { true,  {context, debug, filter, load, schemaCheck, toolchain, verbose}}},
    {"list components",   { true,  {context, debug, filter, load, schemaCheck, toolchain, verbose}}},
    {"list dependencies", { false, {context, debug, filter, load, schemaCheck, toolchain, verbose}}},
    {"list contexts",     { false, {debug, filter, schemaCheck, verbose, ymlOrder}}},
    {"list generators",   { false, {context, contextReplacement, debug, load, schemaCheck, toolchain, verbose}}},
    {"list layers",       { false, {context, contextReplacement, debug, load, clayerSearchPath, schemaCheck, toolchain, verbose}}},
    {"list toolchains",   { false, {context, contextReplacement, debug, toolchain, verbose}}},
    {"list environment",  { true,  {}}},
  };

  try {
    options.add_options("", {
      {"positional", "", cxxopts::value<vector<string>>()},
      solution, context, contextReplacement, contextSet, filter, generator,
      load, clayerSearchPath, missing, schemaCheck, noUpdateRte, output,
      help, version, verbose, debug, dryRun, exportSuffix, toolchain, ymlOrder
    });
    options.parse_positional({ "positional" });

    parseResult = options.parse(argc, argv);
    manager.m_checkSchema = !parseResult.count("n");
    manager.m_worker.SetCheckSchema(manager.m_checkSchema);
    manager.m_extGenerator.SetCheckSchema(manager.m_checkSchema);
    manager.m_missingPacks = parseResult.count("m");
    manager.m_updateRteFiles = !parseResult.count("no-update-rte");
    manager.m_verbose = parseResult.count("v");
    manager.m_worker.SetVerbose(manager.m_verbose);
    manager.m_debug = parseResult.count("d");
    manager.m_dryRun = parseResult.count("D");
    manager.m_worker.SetDebug(manager.m_debug);
    manager.m_worker.SetDryRun(manager.m_dryRun);
    manager.m_ymlOrder = parseResult.count("yml-order");
    manager.m_contextSet = parseResult.count("context-set");

    vector<string> positionalArguments;
    if (parseResult.count("positional")) {
      positionalArguments = parseResult["positional"].as<vector<string>>();
    }
    else if (parseResult.count("version")) {
      manager.ShowVersion();
      return 0;
    }
    else {
      // No command was given, print usage and return success
      return manager.PrintUsage(optionsDict, "", "") ? 0 : 1;
    }

    for (auto it = positionalArguments.begin(); it != positionalArguments.end(); it++) {
      if (regex_match(*it, regex(".*\\.csolution\\.(yml|yaml)"))) {
        manager.m_csolutionFile = *it;
      } else if (manager.m_command.empty()) {
        manager.m_command = *it;
      } else if (manager.m_args.empty()) {
        manager.m_args = *it;
      }
    }
    if (parseResult.count("solution")) {
      manager.m_csolutionFile = parseResult["solution"].as<string>();
    }
    if (!manager.m_csolutionFile.empty()) {
      error_code ec;
      if (!fs::exists(manager.m_csolutionFile, ec)) {
        ProjMgrLogger::Error(manager.m_csolutionFile, "csolution file was not found");
        return 1;
      }
      manager.m_csolutionFile = fs::canonical(manager.m_csolutionFile, ec).generic_string();
      manager.m_rootDir = fs::path(manager.m_csolutionFile).parent_path().generic_string();
    }
    if (parseResult.count("context")) {
      manager.m_context = parseResult["context"].as<vector<string>>();
    }
    if (parseResult.count("context-replacement")) {
      manager.m_contextReplacement = parseResult["context-replacement"].as<string>();
    }
    if (parseResult.count("filter")) {
      manager.m_filter = parseResult["filter"].as<string>();
    }
    if (parseResult.count("generator")) {
      manager.m_codeGenerator = parseResult["generator"].as<string>();
    }
    if (parseResult.count("load")) {
      manager.m_loadPacksPolicy = parseResult["load"].as<string>();
    }
    if (parseResult.count("clayer-path")) {
      manager.m_clayerSearchPath = parseResult["clayer-path"].as<string>();
    }
    if (parseResult.count("export")) {
      manager.m_export = parseResult["export"].as<string>();
    }
    if (parseResult.count("toolchain")) {
      manager.m_selectedToolchain = parseResult["toolchain"].as<string>();
    }
    if (parseResult.count("output")) {
      manager.m_outputDir = parseResult["output"].as<string>();
      manager.m_outputDir = fs::path(manager.m_outputDir).generic_string();
    }
  } catch (cxxopts::OptionException& e) {
    ProjMgrLogger::Error(e.what());
    return 1;
  }

  // Unmatched items in the parse result
  if (!parseResult.unmatched().empty()) {
    ProjMgrLogger::Error("too many command line arguments");
    return 1;
  }

  if (parseResult.count("help")) {
    return manager.PrintUsage(optionsDict, manager.m_command, manager.m_args) ? 0 : 1;
  }

  // Set load packs policy
  if (!manager.SetLoadPacksPolicy()) {
    return 1;
  }

  // Environment variables
  vector<string> envVars;
  if (envp) {
    for (char** env = envp; *env != 0; env++) {
      envVars.push_back(string(*env));
    }
  }
  manager.m_worker.SetEnvironmentVariables(envVars);

  if (manager.m_command == "list") {
    // Process 'list' command
    if (manager.m_args.empty()) {
      ProjMgrLogger::Error("list <args> was not specified");
      return 1;
    }
    // Process argument
    if (manager.m_args == "packs") {
      if (!manager.RunListPacks()) {
        return 1;
      }
    } else if (manager.m_args == "boards") {
      if (!manager.RunListBoards()) {
        return 1;
      }
    } else if (manager.m_args == "devices") {
      if (!manager.RunListDevices()) {
        return 1;
      }
    } else if (manager.m_args == "components") {
      if (!manager.RunListComponents()) {
        return 1;
      }
    } else if (manager.m_args == "configs") {
      if (!manager.RunListConfigs()) {
        return 1;
      }
    } else if (manager.m_args == "dependencies") {
      if (!manager.RunListDependencies()) {
        return 1;
      }
    } else if (manager.m_args == "contexts") {
      if (!manager.RunListContexts()) {
        return 1;
      }
    } else if (manager.m_args == "generators") {
      if (!manager.RunListGenerators()) {
        return 1;
      }
    } else if (manager.m_args == "layers") {
      if (!manager.RunListLayers()) {
        return 1;
      }
    } else if (manager.m_args == "toolchains") {
      if (!manager.RunListToolchains()) {
        return 1;
      }
    } else if (manager.m_args == "environment") {
      manager.RunListEnvironment();
    }
    else {
      ProjMgrLogger::Error("list <args> was not found");
      return 1;
    }
  } else if (manager.m_command == "update-rte") {
    // Process 'update-rte' command
    if (!manager.RunConfigure(true)) {
      return 1;
    }
  } else if (manager.m_command == "convert") {
    // Process 'convert' command
    if (!manager.RunConvert()) {
      return 1;
    }
  } else if (manager.m_command == "run") {
    // Process 'run' command
    if (!manager.RunCodeGenerator()) {
      return 1;
    }
  } else {
    ProjMgrLogger::Error("<command> was not found");
    return 1;
  }
  return 0;
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
    ProjMgrLogger::Error("unknown load option: '" + m_loadPacksPolicy + "', it must be 'latest', 'all' or 'required'");
    return false;
  }
  return true;
}

bool ProjMgr::PopulateContexts(void) {
  if (!m_csolutionFile.empty()) {
    // Parse csolution
    if (!m_parser.ParseCsolution(m_csolutionFile, m_checkSchema)) {
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
        ProjMgrLogger::Error(m_csolutionFile, "cproject.yml filenames must be unique");
        return false;
      }
      if (adjacent_find(dirs.begin(), dirs.end()) != dirs.end()) {
        ProjMgrLogger::Warn(m_csolutionFile, "cproject.yml files should be placed in separate sub-directories");
      }
    }
    // Parse cprojects
    for (const auto& cproject : cprojects) {
      error_code ec;
      string const& cprojectFile = fs::canonical(m_rootDir + "/" + cproject, ec).generic_string();
      if (cprojectFile.empty()) {
        ProjMgrLogger::Error(cproject, "cproject file was not found");
        return false;
      }
      if (!m_parser.ParseCproject(cprojectFile, m_checkSchema)) {
        return false;
      }
    }
  } else {
    ProjMgrLogger::Error("input yml files were not specified");
    return false;
  }

  // Set toolchain
  m_worker.SetSelectedToolchain(m_selectedToolchain);

  // Set output directory
  m_worker.SetOutputDir(m_outputDir);

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

  // Retrieve global generators
  if (!m_extGenerator.RetrieveGlobalGenerators()) {
    return false;
  }

  return true;
}

bool ProjMgr::RunConfigure(bool printConfig) {
  // Parse all input files and populate contexts inputs
  if (!PopulateContexts()) {
    return false;
  }

  bool checkCbuildSet = (m_context.size() == 0) && m_contextSet;
  // Parse context selection
  if (!m_worker.ParseContextSelection(m_context, m_contextReplacement, checkCbuildSet)) {
    return false;
  }
  // Get context pointers
  map<string, ContextItem>* contexts = nullptr;
  m_worker.GetContexts(contexts);

  // Initialize model
  if (!m_worker.InitializeModel()) {
    return false;
  }
  vector<ContextItem*> allContexts;
  vector<string> orderedContexts;
  m_worker.GetYmlOrderedContexts(orderedContexts);
  // Process contexts
  bool error = false;
  m_processedContexts.clear();
  for (auto& contextName : orderedContexts) {
    auto& contextItem = (*contexts)[contextName];
    allContexts.push_back(&contextItem);
    if (!m_worker.IsContextSelected(contextName)) {
      continue;
    }
    if (!m_worker.ProcessContext(contextItem, true, true, m_updateRteFiles)) {
      ProjMgrLogger::Error("processing context '" + contextName + "' failed");
      error = true;
    } else {
      m_processedContexts.push_back(&contextItem);
    }
  }
  m_selectedToolchain = m_worker.GetSelectedToochain();
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
      ProjMgrLogger::Info(infoMsg);
    }
  }

  // Generate cbuild index
  if (!allContexts.empty()) {
    if (!m_emitter.GenerateCbuildIndex(m_parser, allContexts, m_outputDir)) {
      return false;
    }
  }

  // Generate cbuild-set file
  if (m_contextSet && (!m_processedContexts.empty())) {
    if (!m_emitter.GenerateCbuildSet(m_parser, m_processedContexts, m_selectedToolchain)) {
      return false;
    }
  }

  return !error;
}

bool ProjMgr::RunConvert(void) {
  // Configure
  bool error = !RunConfigure();
  // Generate Cprjs
  for (auto& contextItem : m_processedContexts) {
    error_code ec;
    const string& filename = fs::weakly_canonical(contextItem->directories.cprj + "/" + contextItem->name + ".cprj", ec).generic_string();
    RteFsUtils::CreateDirectories(contextItem->directories.cprj);
    if (m_generator.GenerateCprj(*contextItem, filename)) {
      ProjMgrLogger::Info(filename, "file generated successfully");
    } else {
      ProjMgrLogger::Error(filename, "file cannot be written");
      return false;
    }
    if (!m_export.empty()) {
      // Generate non-locked Cprj
      const string& exportfilename = fs::weakly_canonical(contextItem->directories.cprj + "/" + contextItem->name + m_export + ".cprj", ec).generic_string();
      if (m_generator.GenerateCprj(*contextItem, exportfilename, true)) {
        ProjMgrLogger::Info(exportfilename, "export file generated successfully");
      } else {
        ProjMgrLogger::Error(exportfilename, "export file cannot be written");
        return false;
      }
    }
  }

  // Generate cbuild files
  for (auto& contextItem : m_processedContexts) {
    if (!m_emitter.GenerateCbuild(contextItem)) {
      return false;
    }
  }

  return !error;
}

bool ProjMgr::RunListPacks(void) {
  if (!m_csolutionFile.empty()) {
    // Parse all input files and create contexts
    if (!PopulateContexts()) {
      return false;
    }
  }
  // Parse context selection
  if (!m_worker.ParseContextSelection(m_context, m_contextReplacement)) {
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
  if (!m_worker.ParseContextSelection(m_context, m_contextReplacement)) {
    return false;
  }
  vector<string> boards;
  if (!m_worker.ListBoards(boards, m_filter)) {
    ProjMgrLogger::Error("processing boards list failed");
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
  if (!m_worker.ParseContextSelection(m_context, m_contextReplacement)) {
    return false;
  }
  vector<string> devices;
  if (!m_worker.ListDevices(devices, m_filter)) {
    ProjMgrLogger::Error("processing devices list failed");
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
  if (!m_worker.ParseContextSelection(m_context, m_contextReplacement)) {
    return false;
  }
  vector<string> components;
  if (!m_worker.ListComponents(components, m_filter)) {
    ProjMgrLogger::Error("processing components list failed");
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
  if (!m_worker.ParseContextSelection(m_context, m_contextReplacement)) {
    return false;
  }
  vector<string> configFiles;
  if (!m_worker.ListConfigs(configFiles, m_filter)) {
    ProjMgrLogger::Error("processing config list failed");
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
  if (!m_worker.ParseContextSelection(m_context, m_contextReplacement)) {
    return false;
  }
  vector<string> dependencies;
  if (!m_worker.ListDependencies(dependencies, m_filter)) {
    ProjMgrLogger::Error("processing dependencies list failed");
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
    ProjMgrLogger::Error("processing contexts list failed");
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
  if (!m_worker.ParseContextSelection(m_context, m_contextReplacement)) {
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
  if (!m_csolutionFile.empty()) {
    // Parse all input files and create contexts
    if (!PopulateContexts()) {
      return false;
    }
  }
  // Parse context selection
  if (!m_worker.ParseContextSelection(m_context, m_contextReplacement)) {
    return false;
  }
  // Get layers
  vector<string> layers;
  if (!m_worker.ListLayers(layers, m_clayerSearchPath)) {
    return false;
  }
  for (const auto& layer : layers) {
    cout << layer << endl;
  }
  return true;
}

bool ProjMgr::RunCodeGenerator(void) {
  // Check input options
  if (m_codeGenerator.empty()) {
    ProjMgrLogger::Error("generator identifier was not specified");
    return false;
  }
  // Parse all input files and create contexts
  if (!PopulateContexts()) {
    return false;
  }
  // Parse context selection
  if (!m_worker.ParseContextSelection(m_context, m_contextReplacement)) {
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
  if (!m_worker.ParseContextSelection(m_context, m_contextReplacement)) {
    return false;
  }
  vector<ToolchainItem> toolchains;
  bool ret = m_worker.ListToolchains(toolchains);
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
  return ret;
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
      ProjMgrLogger::Error(cdefaultFile, "multiple cdefault files were found");
    }
    return false;
  }
  m_cdefaultFile = cdefaultFile;
  return true;
}
