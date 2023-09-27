/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ABSTRACT_FORMATTER_H
#define ABSTRACT_FORMATTER_H

#include "XMLTree.h"

#include <sstream>
#include <string>
#include <vector>

/**
 * @brief base abstract class to generate formatted text from XMLTreeElement items in a buffer of type string
*/
class AbstractFormatter {
public:
  /**
   * @brief default constructor
  */
  AbstractFormatter();

  /**
    * @brief virtual destructor
   */
  virtual ~AbstractFormatter() { m_Content.clear(); }

  /**
  * @brief getter for instance content as formatted text
  * @return string containing instance content as formatted text
  */
  const std::string& GetContent() const { return m_Content; }

  /**
 * @brief generate formatted text
 * @param xmlTree pointer to a XMLTree object
 * @param schemaFile name of a schema file
 * @param schemaVersion version of a schema file
 * @return string containing instance content as XML formatted text
*/
  virtual std::string Format(XMLTree* xmlTree, const std::string& schemaFile, const std::string& schemaVersion);

  /**
   * @brief generate formatted text
   * @param rootElement pointer to an instance of type XMLTreeElement
   * @param schemaFile name of a schema file
   * @param schemaVersion version of a schema file
   * @return string containing instance content as XML formatted text
  */
  virtual std::string FormatElement(XMLTreeElement* rootElement,
    const std::string& schemaFile = XmlItem::EMPTY_STRING,
    const std::string& schemaVersion = XmlItem::EMPTY_STRING);

  /**
   * @brief convert special characters to conformed ones
   * @param input string to be converted
   * @return string with converted special characters, default does nothing
  */
  virtual std::string EscapeSpecialChars(const std::string& input) { return input; }

  /**
   * @brief return indentation string corresponding to specified level
  */
  virtual std::string GetIndentString(int level) const;

protected:
  virtual void FormatXmlElement(std::ostringstream& outStream, XMLTreeElement* element, int level = 0) {}; // default does nothing
  static void CollectSortedChildren(XMLTreeElement* element, std::vector< std::pair<std::string, std::vector<XMLTreeElement*> > >& sortedChildren);

protected:
  std::string m_Content;

public:
  static const std::string EOL_STRING;

};

#endif /* ABSTRACT_FORMATTER_H */
