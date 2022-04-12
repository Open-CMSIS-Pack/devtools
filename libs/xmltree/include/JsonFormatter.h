/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef JSONFORMATTER_H
#define JSONFORMATTER_H

#include "AbstractFormatter.h"
#include "XMLTree.h"

#include <vector>

/**
 * @brief class to generate XML formatted text from XMLTreeElement items in a buffer of type string
*/
class JsonFormatter : public AbstractFormatter
{
public:
  /**
   * @brief default constructor
  */
  JsonFormatter();

protected:
  void FormatXmlElement(std::ostringstream& outStream, XMLTreeElement* element, int level=0  ) override;
  void FormatXmlElementBody(std::ostringstream& outStream, XMLTreeElement* element, int level, bool outputTag);

  void FormatXmlElements(std::ostringstream& outStream, const std::string& tag, const std::vector<XMLTreeElement*>& elements, int level);
  static void CollectSortedChildern(XMLTreeElement* element, std::vector< std::pair<std::string, std::vector<XMLTreeElement*> > >& sortedChildren);

};

#endif /* JSONFORMATTER_H */
