/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef YMLFORMATTER_H
#define YMLFORMATTER_H

#include "AbstractFormatter.h"
#include "YmlTree.h"


/**
 * @brief class to generate XML formatted text from XMLTreeElement items in a buffer of type string
*/
class YmlFormatter : public AbstractFormatter
{
public:
  /**
   * @brief default constructor
  */
  YmlFormatter();

  /**
   * @brief generate formatted text
   * @param rootElement pointer to an instance of type XMLTreeElement
   * @param schemaFile name of a schema file (not used)
   * @param schemaVersion version of a schema file (not used)
   * @return string containing instance content as XML formatted text
  */
  std::string FormatElement(XMLTreeElement* rootElement,
    const std::string& schemaFile = XmlItem::EMPTY_STRING,
    const std::string& schemaVersion = XmlItem::EMPTY_STRING) override;

};

#endif /* YMLFORMATTER_H */
