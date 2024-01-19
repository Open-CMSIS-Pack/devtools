#ifndef XMLTreeSlimString_H
#define XMLTreeSlimString_H
/******************************************************************************/
/** @file  XmlTreeSlimXMLTreeSlimString.h
  * @brief A simple XML interface that reads data into a tree structure
  * @uses XmlReader with default XML_InputSourceReader (const char* only)
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

#include "XMLTreeSlim.h"

class IXmlItemBuilder;

/**
 * @brief XML interface that reads data into a tree structure
*/
class XMLTreeSlimString : public  XMLTreeSlim
{
public:
  /**
   * @brief constructor
   * @param itemBuilder pointer to an instance of type IXmlItemBuilder or NULL if a default item builder of type XMLTreeElementBuilder should be used
   * @param bRedirectErrLog true to redirect errors
   * @param bIgnoreAttributePrefixes true to ignore attribute prefixes
  */
  XMLTreeSlimString(IXmlItemBuilder* itemBuilder = NULL, bool bRedirectErrLog = false, bool bIgnoreAttributePrefixes = true);
};

#endif // XMLTreeSlimString_H

