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
   * @param bool generate non-locked cprj versions
   * @return true if executed successfully
  */
  bool GenerateCprj(ContextItem& context, const std::string& filename, bool nonLocked = false);

protected:
  void GenerateCprjCreated(XMLTreeElement* element);
  void GenerateCprjInfo(XMLTreeElement* element, const std::string& description);
  void GenerateCprjPackages(XMLTreeElement* element, const ContextItem& context, bool nonLocked = false);
  void GenerateCprjCompilers(XMLTreeElement* element, const ContextItem& context);
  void GenerateCprjTarget(XMLTreeElement* element, const ContextItem& context);
  void GenerateCprjComponents(XMLTreeElement* element, const ContextItem& context, bool nonLocked = false);
  void GenerateCprjGroups(XMLTreeElement* element, const std::vector<GroupNode>& groups, const std::string& compiler);
  void GenerateCprjOptions(XMLTreeElement* element, const BuildType& buildType);
  void GenerateCprjMisc(XMLTreeElement* element, const std::vector<MiscItem>& miscVec);
  void GenerateCprjMisc(XMLTreeElement* element, const MiscItem& misc);
  void GenerateCprjLinkerOptions(XMLTreeElement* element, const std::string& compiler, const LinkerContextItem& linker);
  void GenerateCprjVector(XMLTreeElement* element, const std::vector<std::string>& vec, std::string tag);

  static void SetAttribute(XMLTreeElement* element, const std::string& name, const std::string& value);
  static const std::string GetStringFromVector(const std::vector<std::string>& vector, const char* delimiter);
  static const std::string GetLocalTimestamp();
  static bool WriteXmlFile(const std::string& file, XMLTree* tree);
};

#endif  // PROJMGRGENERATOR_H
