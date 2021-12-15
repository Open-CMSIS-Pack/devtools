/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrParser.h"
#include "ProductInfo.h"
#include "RteFsUtils.h"

#include <algorithm>
#include <cxxopts.hpp>
#include <iostream>
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
  convert             Convert cproject.yml in cprj file\n\
  help                Print usage\n\n\
Options:\n\
  -p, --project arg   Input cproject.yml file\n\
  -s, --solution arg  Input csolution.yml file (TBD)\n\
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
        cerr << "projmgr error: csolution file " << manager.m_csolutionFile << " was not found" << endl;
        return 1;
      }
      manager.m_csolutionFilename = fs::path(manager.m_csolutionFile).stem().stem().generic_string();
      manager.m_rootDir = fs::path(fs::canonical(manager.m_csolutionFile, ec)).parent_path().generic_string();
    }
    if (parseResult.count("project")) {
      manager.m_cprojectFile = parseResult["project"].as<string>();
      error_code ec;
      if (!fs::exists(manager.m_cprojectFile, ec)) {
        cerr << "projmgr error: cproject file " << manager.m_cprojectFile << " was not found" << endl;
        return 1;
      }
      manager.m_cprojectFilename = fs::path(manager.m_cprojectFile).stem().stem().generic_string();
      manager.m_rootDir = fs::path(fs::canonical(manager.m_cprojectFile, ec)).parent_path().generic_string();
    }
    if (parseResult.count("filter")) {
      manager.m_filter = parseResult["filter"].as<string>();
    }
    if (parseResult.count("output")) {
      manager.m_outputDir = parseResult["output"].as<string>();
    }
  } catch (cxxopts::OptionException& e) {
    cerr << "projmgr error: parsing command line failed!" << endl << e.what() << endl;
    return 1;
  }

  // Unmatched items in the parse result
  if (!parseResult.unmatched().empty()) {
    cerr << "projmgr error: too many command line arguments!" << endl;
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
      cerr << "projmgr error: list <args> was not specified!" << endl;
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
    } else {
      cerr << "projmgr error: list <args> was not found!" << endl;
      return 1;
    }
  } else if (manager.m_command == "convert") {
    // Process 'convert' command
    if (!manager.RunConvert()) {
      return 1;
    }
  } else {
    cerr << "projmgr error: <command> was not found!" << endl;
    return 1;
  }
  return 0;
}

bool ProjMgr::RunConvert(void) {
  if (m_cprojectFile.empty()) {
    cerr << "projmgr error: cproject.yml file was not specified!" << endl;
    return false;
  }
  ProjMgrProjectItem project;
  if (!m_parser.ParseCproject(m_cprojectFile, project.cproject)) {
    cerr << "projmgr error: parsing '" << m_cprojectFile <<"' failed" << endl;
    return false;
  }
  if (!m_worker.ProcessProject(project, true)) {
    cerr << "projmgr error: processing '" << m_cprojectFile << "' failed" << endl;
    return false;
  }

  if (m_outputDir.empty()) {
    m_outputDir = m_rootDir;
  } else {
    if (!RteFsUtils::Exists(m_outputDir)) {
      RteFsUtils::CreateDirectories(m_outputDir);
    }
  }

  const string& filename = m_outputDir + "/" + project.name + ".cprj";
  if (!m_generator.GenerateCprj(project, filename)) {
    cerr << "projmgr error: " << filename << " file cannot be written!" << endl;
    return false;
  }
  return true;
}

bool ProjMgr::RunListPacks(void) {
  set<string> packs;
  if (!m_worker.ListPacks(m_filter, packs)) {
    cerr << "projmgr error: processing pack list failed" << endl;
    return false;
  }
  for (const auto& pack : packs) {
    cout << pack << endl;
  }
  return true;
}

bool ProjMgr::RunListDevices(void) {
  ProjMgrProjectItem project;
  if (!m_cprojectFile.empty()) {
    if (!m_parser.ParseCproject(m_cprojectFile, project.cproject)) {
      cerr << "projmgr error: parsing '" << m_cprojectFile << "' failed" << endl;
      return false;
    }
  }
  set<string> devices;
  if (!m_worker.ListDevices(project, m_filter, devices)) {
    cerr << "projmgr error: processing devices list failed" << endl;
    return false;
  }
  for (const auto& device : devices) {
    cout << device << endl;
  }
  return true;
}

bool ProjMgr::RunListComponents(void) {
  ProjMgrProjectItem project;
  if (!m_cprojectFile.empty()) {
    if (!m_parser.ParseCproject(m_cprojectFile, project.cproject)) {
      cerr << "projmgr error: parsing '" << m_cprojectFile << "' failed" << endl;
      return false;
    }
  }
  set<string> components;
  if (!m_worker.ListComponents(project, m_filter, components)) {
    cerr << "projmgr error: processing components list failed" << endl;
    return false;
  }
  for (const auto& component : components) {
    cout << component << endl;
  }
  return true;
}

bool ProjMgr::RunListDependencies(void) {
  if (m_cprojectFile.empty()) {
    cerr << "projmgr error: cproject.yml file was not specified!" << endl;
    return false;
  }
  ProjMgrProjectItem project;
  if (!m_parser.ParseCproject(m_cprojectFile, project.cproject)) {
    cerr << "projmgr error: parsing '" << m_cprojectFile << "' failed" << endl;
    return false;
  }
  set<string> dependencies;
  if (!m_worker.ListDependencies(project, m_filter, dependencies)) {
    cerr << "projmgr error: processing dependencies list failed" << endl;
    return false;
  }
  for (const auto& dependency : dependencies) {
    cout << dependency << endl;
  }
  return true;
}
