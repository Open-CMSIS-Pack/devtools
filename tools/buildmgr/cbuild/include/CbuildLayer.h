/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CBUILDLAYER_H
#define CBUILDLAYER_H

#include "Cbuild.h"
#include "XMLTreeSlim.h"

#include <string>
#include <sstream>
#include <set>
#include <map>

/**
 * @brief CPRJ schema definitions
*/
#define SCHEMA_FILE "CPRJ.xsd"    // XML schema file name
#define SCHEMA_VERSION "2.0.0"    // XML schema version

/**
 * @brief xml_elements struct
*/
struct xml_elements
{
  bool isLayer;
  XMLTreeElement* root;
  XMLTreeElement* created;
  XMLTreeElement* info;
  XMLTreeElement* layers;
  XMLTreeElement* layer;
  XMLTreeElement* packages;
  XMLTreeElement* compilers;
  XMLTreeElement* target;
  XMLTreeElement* components;
  XMLTreeElement* files;
};

/**
 * @brief CbuildLayer
 *        contains all layer operations
*/
class CbuildLayer {
public:
  CbuildLayer(void);
  ~CbuildLayer(void);

  /**
   * @brief extract layer from project
   * @param args layer arguments for extraction
   * @return true if successfully extracted, otherwise false
  */
  bool Extract(const CbuildLayerArgs& args);

  /**
   * @brief compose new project from layer files
   * @param args contains info about layer(s) used to create a project
   * @return true if project successfully created, otherwise false
  */
  bool Compose(const CbuildLayerArgs& args);

  /**
   * @brief add layer(s) to project
   * @param args arguments contains info about layer(s)
            which needs to be added to project
   * @return true if layer(s) added successfully, otherwise false
  */
  bool Add(const CbuildLayerArgs& args);

  /**
   * @brief remove layer from project
   * @param args arguments contains info about layer(s)
            which needs to be removed from project
   * @return true if layer(s) removed successfully, otherwise false
  */
  bool Remove(const CbuildLayerArgs& args);

  /**
   * @brief parse xml File (.cprj or .clayer)
   * @param file file path to be parsed
   * @param layerName name of the layer to be parsed
   * @return true if initialization is successfull, otherwise false
  */
  bool InitXml(const std::string &file, std::string* layerName=NULL);

  /**
   * @brief format xml content, conditionally save backup and write xml file
   * @param file path to the output file
   * @param tree information to be written into the file
   * @param saveBackup if true save the original file with extension '.bak'
   * @return true if xml file is written successfully, otherwise false
  */
  bool WriteXmlFile(const std::string &file, XMLTree* tree, const bool saveBackup=false);

  /**
   * @brief initialize header (tool and timestamp) information
   * @param file path to the input project file
   * @return true if successfull
  */
  bool InitHeaderInfo(const std::string &file);

  /**
   * @brief get tree
   * @return pointer to tree
  */
  XMLTree* GetTree(void) const;

  /**
   * @brief get cprj elements
   * @return pointer to elements
  */
  xml_elements* GetElements(void) const;

  /**
   * @brief get timestamp information
   * @return timestamp in string
  */
  std::string GetTimestamp(void) const;

  /**
   * @brief get tool information (name + version)
   * @return tool information in string
  */
  std::string GetTool(void) const;

protected:
  static bool GetSections(const XMLTree* tree, xml_elements* elements, std::string* layerName);
  static void CopyElement(XMLTreeElement* destination, const XMLTreeElement* origin, const bool create=true);
  static void CopyMatchedChildren(const XMLTreeElement* origin, XMLTreeElement* destination, const std::string& layer, const std::string& parentLayer="");
  static void RemoveMatchedChildren(const std::string& layer, XMLTreeElement* item);
  static void CopyNestedGroups (XMLTreeElement* destination, const XMLTreeElement* origin);
  static void GetArgsFromChild(const XMLTreeElement* element, const std::string& reference, std::set<std::string>& argList);
  static std::set<std::string> SplitArgs(const std::string& args);
  static std::string MergeArgs(const std::set<std::string>& reference);
  static std::set<std::string> RemoveArgs(const std::set<std::string>& remove, const std::set<std::string>& reference);
  static std::set<std::string> GetDiff(const std::set<std::string>& actual, const std::set<std::string>& reference);
  static int GetSectionNumber (const std::string& tag);
  static bool CompareSections (const XMLTreeElement* first, const XMLTreeElement* second);
  bool ConstructModel (const XMLTreeElement* cprj, const CbuildLayerArgs& args);

  std::string m_cprjFile;
  std::string m_cprjPath;
  std::string m_tool;
  std::string m_timestamp;

  XMLTree* m_cprjTree = NULL;
  xml_elements* m_cprj = NULL;

  std::map<std::string, XMLTree*> m_layerTree;
  std::map<std::string, xml_elements*> m_layer;

  std::map<std::string, std::set<std::string>> m_layerFiles;
  std::map<std::string, std::set<std::string>> m_layerPackages;
  std::list<std::string> m_readmeFiles;
};

#endif /* CBUILDLAYER_H */
