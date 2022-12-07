/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
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
  csolution [-V] [--version] [-h] [--help]\n\
            <command> [<arg>] [OPTIONS...]\n\n\
 Commands:\n\
   list packs            Print list of used packs from the pack repository\n\
        boards           Print list of available board names\n\
        devices          Print list of available device names\n\
        components       Print list of available components\n\
        dependencies     Print list of unresolved project dependencies\n\
        contexts         Print list of contexts in a csolution.yml\n\
        generators       Print list of code generators of a given context\n\
        layers           Print list of available, referenced and compatible layers\n\
   convert               Convert *.csolution.yml input file in *.cprj files\n\
   run                   Run code generator\n\n\
 Options:\n\
   -s, --solution arg    Input csolution.yml file\n\
   -c, --context arg     Input context name <cproject>[.<build-type>][+<target-type>]\n\
   -f, --filter arg      Filter words\n\
   -g, --generator arg   Code generator identifier\n\
   -m, --missing         List only required packs that are missing in the pack repository\n\
   -l, --load arg        Set policy for packs loading [latest|all|required]\n\
   -e, --export arg      Set suffix for exporting <context><suffix>.cprj retaining only specified versions\n\
   -n, --no-check-schema Skip schema check\n\
   -U, --no-update-rte   Skip creation of RTE directory and files\n\
   -o, --output arg      Output directory\n\n\
Use 'csolution <command> -h' for more information about a command.\
";

ProjMgr::ProjMgr(void) : m_checkSchema(false), m_updateRteFiles(true) {
  m_worker.SetParser(&m_parser);
}

ProjMgr::~ProjMgr(void) {
  // Reserved
}

bool ProjMgr::PrintUsage(const map<string, vector<cxxopts::Option>>& cmdOptionsDict,
  const std::string& cmd, const std::string& subCmd) {
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
  string program = ORIGINAL_FILENAME + string(" ") + cmd +
    (subCmd.empty() ? "" : (string(" ") + subCmd));
  cxxopts::Options options(program);
  for (auto& option : cmdOptionsDict.at(filter)) {
    options.add_option(filter, option);
  }
  cout << options.help() << endl;
  return true;
}

void ProjMgr::ShowVersion(void) {
  cout << ORIGINAL_FILENAME << " " << VERSION_STRING << " " << COPYRIGHT_NOTICE << endl;
}

int ProjMgr::RunProjMgr(int argc, char **argv) {
  ProjMgr manager;

  // Command line options
  cxxopts::Options options(ORIGINAL_FILENAME);
  cxxopts::ParseResult parseResult;

  cxxopts::Option solution("s,solution", "Input csolution.yml file", cxxopts::value<string>());
  cxxopts::Option context("c,context", "Input context name <cproject>[.<build-type>][+<target-type>]", cxxopts::value<string>());
  cxxopts::Option filter("f,filter", "Filter words", cxxopts::value<string>());
  cxxopts::Option help("h,help", "Print usage");
  cxxopts::Option generator("g,generator", "Code generator identifier", cxxopts::value<string>());
  cxxopts::Option load("l,load", "Set policy for packs loading [latest|all|required]", cxxopts::value<string>());
  cxxopts::Option missing( "m,missing", "List only required packs that are missing in the pack repository", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option schemaCheck( "n,no-check-schema", "Skip schema check", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option noUpdateRte( "U,no-update-rte", "Skip creation of RTE directory and files", cxxopts::value<bool>()->default_value("false"));
  cxxopts::Option output("o,output", "Output directory", cxxopts::value<string>());
  cxxopts::Option version("V,version", "Print version");
  cxxopts::Option exportSuffix("e,export", "Set suffix for exporting <context><suffix>.cprj retaining only specified versions", cxxopts::value<string>());

  // command options dictionary
  map<string, vector<cxxopts::Option>> optionsDict = {
    {"convert",           {solution, context, output, load, exportSuffix, schemaCheck, noUpdateRte}},
    {"run",               {solution, generator, context, load, schemaCheck}},
    {"list packs",        {solution, context, filter, missing, load, schemaCheck}},
    {"list boards",       {solution, context, filter, load, schemaCheck}},
    {"list devices",      {solution, context, filter, load, schemaCheck}},
    {"list components",   {solution, context, filter, load, schemaCheck}},
    {"list dependencies", {solution, context, filter, load, schemaCheck}},
    {"list contexts",     {solution, filter, schemaCheck}},
    {"list generators",   {solution, context, load, schemaCheck}},
    {"list layers",       {solution, context, load, schemaCheck}},
  };

  try {
    options.add_options("", {
      {"command", "", cxxopts::value<string>()},
      {"args", "", cxxopts::value<string>()},
      solution, context, filter, generator,
      load, missing, schemaCheck, noUpdateRte, output,
      help, version, exportSuffix
    });
    options.parse_positional({ "command", "args"});

    parseResult = options.parse(argc, argv);
    manager.m_checkSchema = !parseResult.count("n");
    manager.m_worker.SetCheckSchema(manager.m_checkSchema);
    manager.m_missingPacks = parseResult.count("m");
    manager.m_updateRteFiles = !parseResult.count("no-update-rte");

    if (parseResult.count("command")) {
      manager.m_command = parseResult["command"].as<string>();
    }
    else if (parseResult.count("version")) {
      manager.ShowVersion();
      return 0;
    }
    else {
      // No command was given, print usage and return success
      return manager.PrintUsage(optionsDict, "", "") ? 0 : 1;
    }

    if (parseResult.count("args")) {
      manager.m_args = parseResult["args"].as<string>();
    }
    if (parseResult.count("solution")) {
      manager.m_csolutionFile = parseResult["solution"].as<string>();
      error_code ec;
      if (!fs::exists(manager.m_csolutionFile, ec)) {
        ProjMgrLogger::Error(manager.m_csolutionFile, "csolution file was not found");
        return 1;
      }
      manager.m_csolutionFile = fs::canonical(manager.m_csolutionFile, ec).generic_string();
      manager.m_rootDir = fs::path(manager.m_csolutionFile).parent_path().generic_string();
    }
    if (parseResult.count("context")) {
      manager.m_context = parseResult["context"].as<string>();
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
    if (parseResult.count("export")) {
      manager.m_export = parseResult["export"].as<string>();
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

  // Parse cdefault
  if (manager.GetCdefaultFile()) {
    if (!manager.m_parser.ParseCdefault(manager.m_cdefaultFile, manager.m_checkSchema)) {
      return false;
    }
  }

  if (parseResult.count("help")) {
    return manager.PrintUsage(optionsDict, manager.m_command, manager.m_args) ? 0 : 1;
  }

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
    }
    else {
      ProjMgrLogger::Error("list <args> was not found");
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

bool ProjMgr::PopulateContexts(void) {
  if (!m_csolutionFile.empty()) {
    // Parse csolution
    if (!m_parser.ParseCsolution(m_csolutionFile, m_checkSchema)) {
      return false;
    }
    // Parse cprojects
    for (const auto& cproject : m_parser.GetCsolution().cprojects) {
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

  // Parse clayers
  for (const auto& cproject : m_parser.GetCprojects()) {
    string const& cprojectFile = cproject.first;
    for (const auto& clayer : cproject.second.clayers) {
      if (clayer.layer.empty()) {
        continue;
      }
      error_code ec;
      string const& clayerFile = fs::canonical(fs::path(cprojectFile).parent_path().append(clayer.layer), ec).generic_string();
      if (clayerFile.empty()) {
        ProjMgrLogger::Error(clayer.layer, "clayer file was not found");
        return false;
      }
      if (!m_parser.ParseClayer(clayerFile, m_checkSchema)) {
        return false;
      }
    }
  }

  // Set output directory
  m_worker.SetOutputDir(m_outputDir);

  // Set load packs policy
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

  // Add contexts
  for (auto& descriptor : m_parser.GetCsolution().contexts) {
    error_code ec;
    const string& cprojectFile = fs::path(descriptor.cproject).is_absolute() ?
      descriptor.cproject : fs::canonical(m_rootDir + "/" + descriptor.cproject, ec).generic_string();
    if (!m_worker.AddContexts(m_parser, descriptor, cprojectFile)) {
      return false;
    }
  }

  return true;
}

bool ProjMgr::RunConvert(void) {
  // Parse all input files and populate contexts inputs
  if (!PopulateContexts()) {
    return false;
  }
  // Parse context selection
  if (!m_worker.ParseContextSelection(m_context)) {
    return false;
  }
  // Get context pointers
  map<string, ContextItem>* contexts = nullptr;
  m_worker.GetContexts(contexts);

  // Initialize model
  if (!m_worker.InitializeModel()) {
    return false;
  }
  // Process contexts
  bool error = false;
  vector<ContextItem*> processedContexts;
  for (auto& [contextName, contextItem] : *contexts) {
    if (!m_worker.IsContextSelected(contextName)) {
      continue;
    }
    if (!m_worker.ProcessContext(contextItem, true, true, m_updateRteFiles)) {
      ProjMgrLogger::Error("processing context '" + contextName + "' failed");
      error = true;
    } else {
      processedContexts.push_back(&contextItem);
    }
  }
  // Generate Cprjs
  for (auto& contextItem : processedContexts) {
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
      error_code ec;
      const string& filename = fs::weakly_canonical(contextItem->directories.cprj + "/" + contextItem->name + m_export + ".cprj", ec).generic_string();
      if (m_generator.GenerateCprj(*contextItem, filename, true)) {
        ProjMgrLogger::Info(filename, "export file generated successfully");
      } else {
        ProjMgrLogger::Error(filename, "export file cannot be written");
        return false;
      }
    }
  }
  // Generate cbuild files
  if (!processedContexts.empty()) {
    if (!m_emitter.GenerateCbuild(m_parser, processedContexts, m_outputDir)) {
      return false;
    }
  }

  if (error) {
    return false;
  }
  return true;
}

bool ProjMgr::RunListPacks(void) {
  if (!m_csolutionFile.empty()) {
    // Parse all input files and create contexts
    if (!PopulateContexts()) {
      return false;
    }
  }
  // Parse context selection
  if (!m_worker.ParseContextSelection(m_context)) {
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
  if (!m_worker.ParseContextSelection(m_context)) {
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
  if (!m_worker.ParseContextSelection(m_context)) {
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
  if (!m_worker.ParseContextSelection(m_context)) {
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

bool ProjMgr::RunListDependencies(void) {
  // Parse all input files and create contexts
  if (!PopulateContexts()) {
    return false;
  }
  // Parse context selection
  if (!m_worker.ParseContextSelection(m_context)) {
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
  if (!m_worker.ListContexts(contexts, m_filter)) {
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
  if (!m_worker.ParseContextSelection(m_context)) {
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
  if (!m_worker.ParseContextSelection(m_context)) {
    return false;
  }
  // Get layers
  vector<string> layers;
  if (!m_worker.ListLayers(layers)) {
    return false;
  }
  for (const auto& layers : layers) {
    cout << layers << endl;
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
  if (!m_worker.ParseContextSelection(m_context)) {
    return false;
  }
  // Run code generator
  if (!m_worker.ExecuteGenerator(m_codeGenerator)) {
    return false;
  }
  return true;
}

bool ProjMgr::GetCdefaultFile(void) {
  error_code ec;
  string exePath = RteUtils::ExtractFilePath(CrossPlatformUtils::GetExecutablePath(ec), true);
  if (ec) {
    return false;
  }
  vector<string> searchPaths = { m_rootDir, exePath + "/../etc/", exePath + "/../../etc/" };
  string cdefaultFile;
  if (!RteFsUtils::FindFileRegEx(searchPaths, ".*\\.cdefault\\.yml", cdefaultFile)) {
    if (!cdefaultFile.empty()) {
      ProjMgrLogger::Error(cdefaultFile, "multiple cdefault.yml files were found");
    }
    return false;
  }
  m_cdefaultFile = cdefaultFile;
  return true;
}
