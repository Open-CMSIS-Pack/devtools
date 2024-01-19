#ifndef XMLTreeSlim_H
#define XMLTreeSlim_H
/******************************************************************************/
/** @file  XmlTreeSlim.h
  * @brief A simple XML interface that reads data into a tree structure
  * @uses XmlReader
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

/**
 * @brief a simple XML interface that reads data into a tree structure
*/
class XMLTreeSlim : public  XMLTree
{
public:
  /**
   * @brief constructor
   * @param itemBuilder pointer to an instance of type IXmlItemBuilder or NULL if a default item builder of type XMLTreeElementBuilder should be used
   * @param bRedirectErrLog true to direct errors
   * @param bIgnoreAttributePrefixes true to ignore attribute prefixes otherwise false
  */
  XMLTreeSlim(IXmlItemBuilder* itemBuilder = NULL, bool bRedirectErrLog = false, bool bIgnoreAttributePrefixes = true);

protected:
  XMLTreeParserInterface* CreateParserInterface() override;
  bool m_bRedirectErrLog;
  bool m_bIgnoreAttributePrefixes;

};

#endif // XMLTreeSlim_H

