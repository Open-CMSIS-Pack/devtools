/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRGENERATOR_H
#define PROJMGRGENERATOR_H

#include "ProjMgrWorker.h"
#include "XMLTree.h"

/**
 * @brief projmgr generator class responsible for formatting and creating xml file according to cprj standard
*/
class ProjMgrGenerator {
public:
  /**
   * @brief class constructor
  */
  ProjMgrGenerator(void);

  /**
   * @brief class destructor
  */
  ~ProjMgrGenerator(void);

  /**
   * @brief generate cprj file
   * @param project reference to project item
   * @param filename cprj file
   * @return true if executed successfully
  */
  bool GenerateCprj(ContextItem& context, const std::string& filename);

protected:
  void GenerateCprjCreated(XMLTreeElement* element);
  void GenerateCprjInfo(XMLTreeElement* element, const std::string& description);
  void GenerateCprjPackages(XMLTreeElement* element, const std::map<std::string, RtePackage*>& packages);
  void GenerateCprjCompilers(XMLTreeElement* element, const ToolchainItem& toolchain);
  void GenerateCprjTarget(XMLTreeElement* element, const ContextItem& context);
  void GenerateCprjComponents(XMLTreeElement* element, const ContextItem& context);
  void GenerateCprjGroups(XMLTreeElement* element, const std::vector<GroupNode>& groups, const std::string& compiler);
  void GenerateCprjMisc(XMLTreeElement* element, const std::vector<MiscItem>& misc, const std::string& compiler);
  void GenerateCprjLinkerScript(XMLTreeElement* element, const std::string& compiler, const std::string& linkerScript);
  void GenerateCprjVector(XMLTreeElement* element, const std::vector<std::string>& vec, std::string tag);

  static void SetAttribute(XMLTreeElement* element, const std::string& name, const std::string& value);
  static const std::string GetStringFromVector(const std::vector<std::string>& vector, const char* delimiter);
  static const std::string GetLocalTimestamp();
  static const std::string GetCategory(const std::string& file);
  static bool WriteXmlFile(const std::string& file, XMLTree* tree);
};

#endif  // PROJMGRGENERATOR_H
