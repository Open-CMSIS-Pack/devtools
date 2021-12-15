/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRYAMLPARSER_H
#define PROJMGRYAMLPARSER_H

#include "ProjMgrParser.h"
#include "yaml-cpp/yaml.h"

/**
  * @brief projmgr parser yaml implementation class, directly coupled to underlying yaml-cpp external library
*/
class ProjMgrYamlParser {
public:
  /**
   * @brief class constructor
  */
  ProjMgrYamlParser(void);

  /**
   * @brief class destructor
  */
  ~ProjMgrYamlParser(void);

  /**
   * @brief parse cproject
   * @param input cproject.yml file
   * @param cproject reference to cproject struct
  */
  bool ParseCproject(const std::string& input, CprojectItem& cproject);

  /**
   * @brief parse csolution
   * @param input csolution.yml file
   * @param csolution reference to csolution struct
  */
  bool ParseCsolution(const std::string& input, CsolutionItem& csolution);

protected:
  void ParseComponents(YAML::Node parent, std::vector<ComponentItem>& components);
  void ParseFiles(YAML::Node parent, std::vector<FileNode>& files);
  void ParseGroups(YAML::Node parent, std::vector<GroupNode>& groups);
  void ParseMisc(YAML::Node parent, MiscItem& misc);
  void ParsePackages(YAML::Node parent, std::vector<std::string>& packages);
  void ParseProcessor(YAML::Node parent, ProcessorItem& processor);
  void ParseTarget(YAML::Node parent, TargetItem& target);
  void ParseString(YAML::Node parent, const std::string& key, std::string& value);
  void ParseVector(YAML::Node parent, const std::string& key, std::vector<std::string>& value);
};

#endif  // PROJMGRYAMLPARSER_H
