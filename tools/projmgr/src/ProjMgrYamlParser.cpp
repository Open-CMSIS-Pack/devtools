/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrYamlParser.h"

#include <iostream>
#include <string>

using namespace std;

ProjMgrYamlParser::ProjMgrYamlParser(void) {
  // Reserved
}

ProjMgrYamlParser::~ProjMgrYamlParser(void) {
  // Reserved
}

bool ProjMgrYamlParser::ParseCsolution(const string& input, CsolutionItem& csolution) {
  // TODO
  return true;
}

bool ProjMgrYamlParser::ParseCproject(const string& input, CprojectItem& cproject) {
  try {
    YAML::Node root = YAML::LoadFile(input);
    YAML::Node projectNode = root["project"];

    map<const string, string&> projectChildren = {
      {"name", cproject.name},
      {"description", cproject.description},
      {"toolchain", cproject.toolchain},
      {"output-type", cproject.outputType}
    };
    for (const auto& item : projectChildren) {
      ParseString(projectNode, item.first, item.second);
    }
    ParseTarget(projectNode, cproject.target);
    ParsePackages(projectNode, cproject.packages);
    ParseComponents(projectNode, cproject.components);
    ParseGroups(projectNode, cproject.groups);

  } catch (YAML::Exception& e) {
    cerr << "projmgr error: check YAML file!" << endl << e.what() << endl;
    return false;
  }

  return true;
}

void ProjMgrYamlParser::ParseTarget(YAML::Node parent, TargetItem& target) {
  if (parent["target"].IsDefined()) {
    YAML::Node targetNode = parent["target"];
    map<const string, string&> targetChildren = {
      {"board", target.board},
      {"device", target.device}
    };
    for (const auto& item : targetChildren) {
      ParseString(targetNode, item.first, item.second);
      ParseProcessor(targetNode, target.processor);
    }
  }
}

void ProjMgrYamlParser::ParsePackages(YAML::Node parent, vector<string>& packages) {
  if (parent["packages"].IsDefined()) {
    YAML::Node packagesNode = parent["packages"];
    for (const auto& packagesEntry : packagesNode) {
      string packageItem;
      ParseString(packagesEntry, "package", packageItem);
      packages.push_back(packageItem);
    }
  }
}

void ProjMgrYamlParser::ParseString(YAML::Node parent, const string& key, string& value) {
  if (parent[key].IsDefined()) {
    value = parent[key].as<string>();
    if (parent[key].Type() == YAML::NodeType::Null) {
      value = "";
    }
  }
}

void ProjMgrYamlParser::ParseVector(YAML::Node parent, const string& key, vector<string>& value) {
  if (parent[key].IsSequence()) {
    value = parent[key].as<vector<string>>();
  }
}

void ProjMgrYamlParser::ParseProcessor(YAML::Node parent, ProcessorItem& processor) {
  if (parent["processor"].IsDefined()) {
    YAML::Node processorNode = parent["processor"];
    map<const string, string&> processorChildren = {
      {"endian", processor.endian},
      {"fpu", processor.fpu},
      {"mpu", processor.mpu},
      {"dsp", processor.dsp},
      {"mve", processor.mve},
      {"trustzone", processor.trustzone},
      {"secure", processor.secure}
    };
    for (const auto& item : processorChildren) {
      ParseString(processorNode, item.first, item.second);
    }
  }
}

void ProjMgrYamlParser::ParseComponents(YAML::Node parent, vector<ComponentItem>& components) {
  if (parent["components"].IsDefined()) {
    YAML::Node componentsNode = parent["components"];
    for (const auto& componentEntry : componentsNode) {
      ComponentItem componentItem;
      ParseString(componentEntry, "component", componentItem.component);

      // TODO: Parse toolchain settings (warnings, debug, misc, includes, defines)

      components.push_back(componentItem);
    }
  }
}

void ProjMgrYamlParser::ParseMisc(YAML::Node parent, MiscItem& misc) {
  ParseVector(parent, "misc", misc.all);
  ParseVector(parent, "misc-as", misc.as);
  ParseVector(parent, "misc-c", misc.c);
  ParseVector(parent, "misc-cpp", misc.cpp);
}

void ProjMgrYamlParser::ParseFiles(YAML::Node parent, vector<FileNode>& files) {
  if (parent["files"].IsDefined()) {
    YAML::Node filesNode = parent["files"];
    FileNode fileNode;
    for (const auto& fileEntry : filesNode) {
      ParseString(fileEntry, "file", fileNode.file);
      ParseString(fileEntry, "category", fileNode.category);

      // TODO: Parse toolchain settings (warnings, debug, misc, includes, defines)

    }
    files.push_back(fileNode);
  }
}

void ProjMgrYamlParser::ParseGroups(YAML::Node parent, vector<GroupNode>& groups) {
  if (parent["groups"].IsDefined()) {
    YAML::Node groupsNode = parent["groups"];
    for (const auto& groupEntry : groupsNode) {
      GroupNode groupNode;
      ParseString(groupEntry, "group", groupNode.group);

      // TODO: Parse toolchain settings (warnings, debug, misc, includes, defines)

      ParseFiles(groupEntry, groupNode.files);
      ParseGroups(groupEntry, groupNode.groups);
      groups.push_back(groupNode);
    }
  }
}
