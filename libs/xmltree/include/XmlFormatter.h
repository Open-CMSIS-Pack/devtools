/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef XMLFORMATTER_H
#define XMLFORMATTER_H

#include "XMLTree.h"

#include <sstream>
#include <string>

/**
 * @brief class to generate XML formatted text from XMLTreeElement items in a buffer of type string
*/
class XmlFormatter {
public:
  /**
   * @brief default constructor
  */
  XmlFormatter();

  /**
   * @brief constructor which generates XML formatted text
   * @param xmlTree pointer to an instance derived from XMLTreeElement
   * @param schemaFile name of XML schema file
   * @param schemaVersion version of XML schema file
  */
  XmlFormatter(XMLTree* xmlTree, const std::string& schemaFile, const std::string& schemaVersion);

  /**
   * @brief destructor
  */
  virtual ~XmlFormatter() { m_xmlContent.clear(); }

  /**
   * @brief generate XML formatted text
   * @param xmlTree pointer to a XMLTree object
   * @param schemaFile name of XML schema file
   * @param schemaVersion version of XML schema file
   * @return string containing instance content as XML formatted text
  */
  virtual std::string Format(XMLTree* xmlTree, const std::string& schemaFile, const std::string& schemaVersion);

  /**
   * @brief generate XML formatted text
   * @param parentElement pointer to an instance of type XMLTreeElement
   * @param schemaFile name of XML schema file
   * @param schemaVersion version of XML schema file
   * @return string containing instance content as XML formatted text
  */
  virtual std::string FormatElement(XMLTreeElement* parentElement,
    const std::string& schemaFile = XmlItem::EMPTY_STRING,
    const std::string& schemaVersion = XmlItem::EMPTY_STRING);

  /**
   * @brief getter for instance content as XML formatted text
   * @return string containing instance content as XML formatted text
  */
  const std::string& GetContent() const { return m_xmlContent; }

  /**
   * @brief convert special characters to XML conformed ones
   * @param input string to be converted
   * @return string with converted special characters
  */
  static std::string ConvertSpecialChars(const std::string& input);

protected:
  virtual void FormatXmlElement(std::ostringstream& xmlStream, XMLTreeElement* element, int level=0);

protected:
  std::string m_xmlContent;
};

#endif /* XMLFORMATTER_H */
