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
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "XMLTree.h"

class IXmlItemBuilder;

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
  YmlTree(IXmlItemBuilder* itemBuilder = NULL);
};

#endif // YMLTree_H

