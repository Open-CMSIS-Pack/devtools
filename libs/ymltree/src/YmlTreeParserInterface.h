#ifndef YML_TREE_PARSER_INTERFACE_H
#define YML_TREE_PARSER_INTERFACE_H
/******************************************************************************/
/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 /******************************************************************************/

#include "YmlTree.h"

#include "yaml-cpp/yaml.h"

/**
 * @brief internal interface to YML reader
*/
class YmlTreeParserInterface : public XMLTreeParserInterface
{
public:
  /**
   * @brief constructor
   * @param tree pointer to an instance of YmlTree
   * @param bRedirectErrLog true to direct errors
  */
  YmlTreeParserInterface(YmlTree* tree);

  /**
  * constructor
  */
  ~YmlTreeParserInterface() override;

  /**
   * @brief initialize the instance. The default implementation does nothing
   * @return true if initialization is done
  */
  bool Init() override;

  /**
   * @brief clean up instance but do not destroy YAML parser
  */
  void Clear() override;

  /**
   * @brief parse YAML file or buffer containing YAML formatted text
   * @param fileName name of YAML file
   * @param inputString buffer containing YAML formatted text
   * @return true if parsing was successful
  */
  bool Parse(const std::string& fileName, const std::string& inputString) override;

  /**
   * @brief return root node corresponding currently parsed file
   * @return valid YAML::Node if parsing was successful
  */
  const YAML::Node& GetRootNode() const { return m_root; }

protected:
  virtual bool ParseNode(const YAML::Node& node, const std::string& tag);
  virtual bool DoParseNode(const YAML::Node& node, const std::string& tag);
  virtual bool ParseMapNode(const YAML::Node& node);
  virtual bool ParseSequenceNode(const YAML::Node& node);
  virtual bool ParseScalarNode(const YAML::Node& node);
  virtual bool CreateItem(const std::string& tag, int line);

  YAML::Node m_root; // current root node
};

#endif //YML_TREE_PARSER_INTERFACE_H
