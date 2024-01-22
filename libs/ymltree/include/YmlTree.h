#ifndef YMLTree_H
#define YMLTree_H
/******************************************************************************/
/** @file  YmlTree.h
  * @brief A simple YAML interface that reads data into a tree structure
*/
/******************************************************************************/
/*
  * The reader should be kept semantics-free:
  * no special processing based on tag, attribute or value string
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "XMLTree.h"

class IXmlItemBuilder;
class ISchemaChecker;
class YmlTreeParserInterface;
/**
 * @brief a simple YAML interface that reads data into a tree structure
*/
class YmlTree : public  XMLTree
{
public:
  /**
   * @brief constructor
   * @param itemBuilder pointer to an instance of type IXmlItemBuilder or NULL if a default item builder of type XMLTreeElementBuilder should be used
   * @param bRedirectErrLog true to direct errors
   * @param bIgnoreAttributePrefixes true to ignore attribute prefixes otherwise false
  */
  YmlTree(IXmlItemBuilder* itemBuilder = nullptr);

protected:
  /**
   * @brief creates underling YML parser interface
   * @return XMLTreeParserInterface;
  */
  XMLTreeParserInterface* CreateParserInterface() override;

  /**
   * @brief return underling YAML parser interface
   * @return YmlTreeParserInterface
  */
  YmlTreeParserInterface* GetYmlParserInterface() const { return m_pYmlInterface; }

  /**
   * @brief to provide direct access to YAML data is needed
  */
  YmlTreeParserInterface* m_pYmlInterface;
};

#endif // YMLTree_H

