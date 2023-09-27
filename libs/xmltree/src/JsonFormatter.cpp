/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "XMLTree.h"
#include "JsonFormatter.h"

#include <list>
#include <map>

using namespace std;


JsonFormatter::JsonFormatter() {
}

void JsonFormatter::FormatXmlElement(ostringstream& outStream, XMLTreeElement* element, int level)
{
  if (!element) {
    return;
  }

  // top-level element
  outStream << '{' << EOL_STRING;
  FormatXmlElementBody(outStream, element, level + 1, true);
  outStream << EOL_STRING  << '}' << EOL_STRING;
}

 void JsonFormatter::FormatXmlElementBody(ostringstream & outStream, XMLTreeElement* element, int level, bool outputTag)
 {

  string indent = GetIndentString(level);
  const string& text = element->GetText();
  if (outputTag) {
    outStream << indent << "\"" << element->GetTag() << "\": ";
  }
  const map<string, string> attributes = element->GetAttributes(); // copy of attributes
  if (attributes.empty() && !element->HasChildren()) {
    if (!text.empty()) {
      if (!outputTag) {
        outStream << indent;
      }
      outStream << '\"' << text << '\"';
    }
    return;
  }
  if (outputTag) {
    outStream << EOL_STRING;
  }

  outStream << indent << '{' << EOL_STRING;
  if (!attributes.empty()) {
    // calculate number of commas
    std::size_t count = attributes.size() - 1;
    if (element->HasChildren() || !text.empty()) {
      count++;
    }
    for (auto attribute : attributes) {
      outStream << indent << "  \"-" << attribute.first << "\": " << "\"" << attribute.second << "\"";
      if (count > 0) {
        outStream << ',';
      }
      count--;
      outStream << EOL_STRING;
    }
  }
  if (!text.empty()) {
    outStream << indent << "  \"#text\": \"" << text << "\"" << EOL_STRING;
  } else if (element->HasChildren()) {
      vector< pair<string, vector<XMLTreeElement*> > > sortedChildren;
      CollectSortedChildren(element, sortedChildren);
      std::size_t count = sortedChildren.size() - 1;
      for (auto [k, v] : sortedChildren) {
        FormatXmlElements(outStream, k, v, level + 1);
        if (count > 0) {
          outStream << ',';
        }
        count--;
        outStream << EOL_STRING;
      }
  }

  outStream << indent << '}';
}


void JsonFormatter::FormatXmlElements(ostringstream& outStream, const string& tag, const std::vector<XMLTreeElement*>& elements, int level)
{
  if (elements.empty()) {
    return;
  }

  string indent = GetIndentString(level);
  bool bSingleElement = elements.size() == 1;
  if (!bSingleElement) {
    outStream << indent << '\"' << tag << "\": ";
    outStream << '[' << EOL_STRING;
  }
  std::size_t count = elements.size() - 1;
  for (auto element : elements) {
    FormatXmlElementBody(outStream, element, level + 1, bSingleElement);
    if (count > 0) {
      outStream << ',';
      outStream << EOL_STRING;
    }
    count--;
  }
  if (!bSingleElement) {
    outStream << EOL_STRING;
    outStream << indent << ']';
  }
}

// end of JsonFormatter.cpp
