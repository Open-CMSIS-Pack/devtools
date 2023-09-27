/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef XMLFORMATTER_H
#define XMLFORMATTER_H

#include "AbstractFormatter.h"
#include "XMLTree.h"

/**
 * @brief class to generate XML formatted text from XMLTreeElement items in a buffer of type string
*/
class XmlFormatter : public AbstractFormatter
{
public:
  /**
   * @brief default constructor
   * @param bInsertEmptyLines insert extra space between children on the level 0
  */
  XmlFormatter(bool bInsertEmptyLines = true);

  /**
   * @brief constructor which generates XML formatted text
   * @param xmlTree pointer to an instance derived from XMLTreeElement
   * @param schemaFile name of XML schema file
   * @param schemaVersion version of XML schema file
   * @param bInsertEmptyLines insert extra space between children on the level 0
  */
  XmlFormatter(XMLTree* xmlTree, const std::string& schemaFile, const std::string& schemaVersion, bool bInsertEmptyLines = true);

  /**
   * @brief generate XML formatted text
   * @param parentElement pointer to an instance of type XMLTreeElement
   * @param schemaFile name of XML schema file
   * @param schemaVersion version of XML schema file
   * @return string containing instance content as XML formatted text
  */
  std::string FormatElement(XMLTreeElement* parentElement,
    const std::string& schemaFile = XmlItem::EMPTY_STRING,
    const std::string& schemaVersion = XmlItem::EMPTY_STRING) override;


  /**
  * @brief convert special characters to conformed ones
  * @param input string to be converted
  * @return string with converted special characters, default does nothing
 */
  std::string EscapeSpecialChars(const std::string& input) override { return ConvertSpecialChars(input); }

 /**
   * @brief convert special characters to XML conformed ones
   * @param input string to be converted
   * @return string with converted special characters
  */
  static std::string ConvertSpecialChars(const std::string& input);

  static const std::string XMLHEADER;

protected:
  void FormatXmlElement(std::ostringstream& xmlStream, XMLTreeElement* element, int level=0) override;

  bool m_bInsertEmptyLines;
};

#endif /* XMLFORMATTER_H */
