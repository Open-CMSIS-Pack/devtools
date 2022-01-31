/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrParser.h"
#include "ProjMgrLogger.h"
#include "ProductInfo.h"
#include "RteFsUtils.h"

#include <algorithm>
#include <cxxopts.hpp>
#include <functional>

using namespace std;

static constexpr const char* USAGE = "\
Usage:\n\
  projmgr <command> [<args>] [OPTIONS...]\n\n\
Commands:\n\
  list packs          Print list of installed packs\n\
       devices        Print list of available device names\n\
       components     Print list of available components\n\
       dependencies   Print list of unresolved project dependencies\n\
       contexts       Print list of contexts in a csolution.yml\n\
  convert             Convert cproject.yml or csolution.yml in cprj files\n\
  help                Print usage\n\n\
Options:\n\
  -p, --project arg   Input cproject.yml file\n\
  -s, --solution arg  Input csolution.yml file\n\
  -f, --filter arg    Filter words\n\
  -o, --output arg    Output directory\n\
  -h, --help          Print usage\n\
";

ProjMgr::ProjMgr(void) {
  // Reserved
}

ProjMgr::~ProjMgr(void) {
  // Reserved
}

void ProjMgr::PrintUsage(void) {
  cout << PRODUCT_NAME << " " << VERSION_STRING << " " << COPYRIGHT_NOTICE << " " << endl;
  cout << USAGE << endl;
}

int ProjMgr::RunProjMgr(int argc, char **argv) {

  ProjMgr manager;

  // Command line options
  cxxopts::Options options(ORIGINAL_FILENAME);
  cxxopts::ParseResult parseResult;

  try {
    options.add_options()
      ("command", "", cxxopts::value<string>())
      ("args", "", cxxopts::value<string>())
      ("p,project", "", cxxopts::value<string>())
      ("s,solution", "", cxxopts::value<string>())
      ("f,filter", "", cxxopts::value<string>())
      ("o,output", "", cxxopts::value<string>())
      ("h,help", "")
      ;
    options.parse_positional({ "command", "args"});

    parseResult = options.parse(argc, argv);

    if (parseResult.count("command")) {
      manager.m_command = parseResult["command"].as<string>();
    } else {
      // No command was given, print usage and return success
      manager.PrintUsage();
      return 0;
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
    if (parseResult.count("project")) {
      manager.m_cprojectFile = parseResult["project"].as<string>();
      error_code ec;
      if (!fs::exists(manager.m_cprojectFile, ec)) {
        ProjMgrLogger::Error(manager.m_cprojectFile, "cproject file was not found");
        return 1;
      }
      manager.m_cprojectFile = fs::canonical(manager.m_cprojectFile, ec).generic_string();
      manager.m_rootDir = fs::path(manager.m_cprojectFile).parent_path().generic_string();
    }
    if (parseResult.count("filter")) {
      manager.m_filter = parseResult["filter"].as<string>();
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

  // Parse commands
  if ((manager.m_command == "help") || parseResult.count("help")) {
    // Print usage
    manager.PrintUsage();
    return 0;
  } else if (manager.m_command == "list") {
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
    } else {
      ProjMgrLogger::Error("list <args> was not found");
      return 1;
    }
  } else if (manager.m_command == "convert") {
    // Process 'convert' command
    if (!manager.RunConvert()) {
      return 1;
    }
  } else {
    ProjMgrLogger::Error("<command> was not found");
    return 1;
  }
  return 0;
}

bool ProjMgr::RunConvert(void) {
  if (!m_csolutionFile.empty()) {
    // Parse csolution
    if (!m_parser.ParseCsolution(m_csolutionFile)) {
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
      if (!m_parser.ParseCproject(cprojectFile)) {
        return false;
      }
    }
  } else if (!m_cprojectFile.empty()) {
    // Parse single cproject
    if (!m_parser.ParseCproject(m_cprojectFile, true)) {
      return false;
    }
  } else {
    ProjMgrLogger::Error("input yml files were not specified");
    return false;
  }

  // Parse clayers
  for (const auto& cproject : m_parser.GetCprojects()) {
    string const& cprojectFile = cproject.first;
    for (const auto& clayer : cproject.second.clayers) {
      error_code ec;
      string const& clayerFile = fs::canonical(fs::path(cprojectFile).parent_path().append(clayer.layer), ec).generic_string();
      if (clayerFile.empty()) {
        ProjMgrLogger::Error(clayer.layer, "clayer file was not found");
        return false;
      }
      if (!m_parser.ParseClayer(clayerFile)) {
        return false;
      }
    }
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

  // Process contexts
  for (auto& context : m_worker.GetContexts()) {
    if (!m_worker.ProcessContext(context.second, true)) {
      ProjMgrLogger::Error("processing context '" + context.first + "' failed");
      return false;
    }
  }

  // Process project dependencies
  for (auto& context : m_worker.GetContexts()) {
    if (!m_worker.ProcessProjDeps(context.second, m_outputDir)) {
      ProjMgrLogger::Error("processing project dependencies for '" + context.first + "' failed");
      return false;
    }
  }

  // Generate Cprjs
  for (auto& context : m_worker.GetContexts()) {
    error_code ec;
    const string& directory = m_outputDir.empty() ? context.second.cprojectDir + "/" + context.second.rteDir : m_outputDir + "/" + context.first;
    const string& filename = fs::weakly_canonical(directory + "/" + context.first + ".cprj", ec).generic_string();
    RteFsUtils::CreateDirectories(directory);
    if (m_generator.GenerateCprj(context.second, filename)) {
      ProjMgrLogger::Info(filename, "file generated successfully");
    } else {
      ProjMgrLogger::Error(filename, "file cannot be written");
      return false;
    }
    if (!m_worker.CopyContextFiles(context.second, directory, m_outputDir.empty())) {
      ProjMgrLogger::Error("files cannot be copied into output directory '" + directory + "'");
      return false;
    }
  }
  return true;
}

bool ProjMgr::RunListPacks(void) {
  set<string> packs;
  if (!m_worker.ListPacks(m_filter, packs)) {
    ProjMgrLogger::Error("processing pack list failed");
    return false;
  }
  for (const auto& pack : packs) {
    cout << pack << endl;
  }
  return true;
}

bool ProjMgr::RunListDevices(void) {
  if (!m_cprojectFile.empty()) {
    if (!m_parser.ParseCproject(m_cprojectFile, true)) {
      return false;
    }
    ContextItem context;
    ContextDesc descriptor;
    if (!m_worker.AddContexts(m_parser, descriptor, m_cprojectFile)) {
      ProjMgrLogger::Error(m_cprojectFile, "adding project context failed");
      return false;
    }
  }
  set<string> devices;
  if (!m_worker.ListDevices(m_filter, devices)) {
    ProjMgrLogger::Error("processing devices list failed");
    return false;
  }
  for (const auto& device : devices) {
    cout << device << endl;
  }
  return true;
}

bool ProjMgr::RunListComponents(void) {
  if (!m_cprojectFile.empty()) {
    if (!m_parser.ParseCproject(m_cprojectFile, true)) {
      return false;
    }
    ContextDesc descriptor;
    if (!m_worker.AddContexts(m_parser, descriptor, m_cprojectFile)) {
      return false;
    }
  }
  set<string> components;
  if (!m_worker.ListComponents(m_filter, components)) {
    ProjMgrLogger::Error("processing components list failed");
    return false;
  }
  for (const auto& component : components) {
    cout << component << endl;
  }
  return true;
}

bool ProjMgr::RunListDependencies(void) {
  if (m_cprojectFile.empty()) {
    ProjMgrLogger::Error("cproject.yml file was not specified");
    return false;
  }
  if (!m_parser.ParseCproject(m_cprojectFile, true)) {
    return false;
  }
  ContextDesc descriptor;
  if (!m_worker.AddContexts(m_parser, descriptor, m_cprojectFile)) {
    return false;
  }
  set<string> dependencies;
  if (!m_worker.ListDependencies(m_filter, dependencies)) {
    ProjMgrLogger::Error("processing dependencies list failed");
    return false;
  }
  for (const auto& dependency : dependencies) {
    cout << dependency << endl;
  }
  return true;
}

bool ProjMgr::RunListContexts(void) {
  if (m_csolutionFile.empty()) {
    ProjMgrLogger::Error("csolution.yml file was not specified");
    return false;
  }
  if (!m_parser.ParseCsolution(m_csolutionFile)) {
    return false;
  }
  for (const auto& cproject : m_parser.GetCsolution().cprojects) {
    error_code ec;
    string const& cprojectFile = fs::canonical(m_rootDir + "/" + cproject, ec).generic_string();
    if (!m_parser.ParseCproject(cprojectFile)) {
      return false;
    }
  }
  for (auto& descriptor : m_parser.GetCsolution().contexts) {
    error_code ec;
    const string& cprojectFile = fs::path(descriptor.cproject).is_absolute() ?
      descriptor.cproject : fs::canonical(m_rootDir + "/" + descriptor.cproject, ec).generic_string();
    if (!m_worker.AddContexts(m_parser, descriptor, cprojectFile)) {
      return false;
    }
  }
  set<string> contexts;
  if (!m_worker.ListContexts(m_filter, contexts)) {
    ProjMgrLogger::Error("processing contexts list failed");
    return false;
  }
  for (const auto& context : contexts) {
    cout << context << endl;
  }
  return true;
}
