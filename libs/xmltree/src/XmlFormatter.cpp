/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "XMLTree.h"
#include "XmlFormatter.h"

#include <list>
#include <map>

using namespace std;

const string XMLHEADER = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>";
const string SCHEMAATTR = "xmlns:xsi";
const string SCHEMAINSTANCE = "http://www.w3.org/2001/XMLSchema-instance";
const string OWNNAMESPACE = "xsi:noNamespaceSchemaLocation";
const string VERSIONATTR = "schemaVersion";

XmlFormatter::XmlFormatter() {
}

XmlFormatter::XmlFormatter(XMLTree* xmlTree, const string& schemaFile, const string& schemaVersion)
{
  m_Content = XmlFormatter::Format(xmlTree, schemaFile, schemaVersion);
}


string XmlFormatter::FormatElement(XMLTreeElement* rootElement, const string& schemaFile, const string& schemaVersion)
{
  if (!rootElement) {
    return XMLTree::EMPTY_STRING;
  }
  // Add xml header attributes
  rootElement->AddAttribute(SCHEMAATTR, SCHEMAINSTANCE);
  rootElement->AddAttribute(OWNNAMESPACE, schemaFile);
  rootElement->AddAttribute(VERSIONATTR, schemaVersion);
  // Format elements recursively
  ostringstream xmlStream;
  xmlStream << XMLHEADER << EOL_STRING;
  FormatXmlElement(xmlStream, rootElement);
  return xmlStream.str();
}


void XmlFormatter::FormatXmlElement(ostringstream& xmlStream, XMLTreeElement* element, int level)
{
  if(!element) {
    return;
  }
  string indent = GetIndentString(level);

  const string& tag = element->GetTag();
  const string& text = element->GetText();
  xmlStream << indent + '<' << tag;
  map<string, string> attributes = element->GetAttributes();
  for (auto attribute : attributes) {
    xmlStream << ' ';
    xmlStream << attribute.first << "=\"" << EscapeSpecialChars(attribute.second) << "\"";
  }
  if (element->HasChildren()) {
    auto& children = element->GetChildren();
    xmlStream << '>' << EOL_STRING;
    for (auto it = children.begin(); it != children.end(); it++) {
      // insert extra space between children on the level 0
      if ((level == 0) && (it != children.begin())) {
        xmlStream << EOL_STRING;
      }
      FormatXmlElement(xmlStream, *it, level + 1);
    }
    xmlStream << indent << "</" << tag << ">" << EOL_STRING;
  } else if (!text.empty()) {
    xmlStream << '>' << EscapeSpecialChars(text) << "</" << tag << ">" << EOL_STRING;
  } else {
    xmlStream << "/>" << EOL_STRING;
  }
}

string XmlFormatter::ConvertSpecialChars(const string& input)
{
  if(input.empty()) {
    return input;
  }

  const map<string, string> SpecialChars = {{"&amp;","&"},{"&lt;","<"},{"&gt;",">"}, {"&apos;","\'"}, {"&quot;","\""}};
  size_t s;
  string str(input);
  for (auto sc : SpecialChars) {
    s = 0;
    while ((s = str.find(sc.second, s)) != string::npos) {
      str.replace(s, sc.second.length(), sc.first);
      s += sc.first.length();
    }
  }
  return str;
}
