/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "XMLTree.h"
#include "XmlFormatter.h"

#include <string>
#include <sstream>
#include <list>
#include <map>

using namespace std;

const string XMLHEADER = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>";
const string SCHEMAATTR = "xmlns:xsi";
const string SCHEMAINSTANCE = "http://www.w3.org/2001/XMLSchema-instance";
const string OWNNAMESPACE = "xsi:noNamespaceSchemaLocation";
const string VERSIONATTR = "schemaVersion";
const string IDENTATION = "  ";
const string EOL = "\n";

XmlFormatter::XmlFormatter() {
}

XmlFormatter::XmlFormatter(XMLTree* xmlTree, const string& schemaFile, const string& schemaVersion)
{
  m_xmlContent = XmlFormatter::Format(xmlTree, schemaFile, schemaVersion);
}

string XmlFormatter::Format(XMLTree* xmlTree, const string& schemaFile, const string& schemaVersion)
{
  XMLTreeElement* rootElement = xmlTree->GetRoot() ? xmlTree->GetRoot()->GetFirstChild() : nullptr;
  if (!rootElement) {
    return XMLTree::EMPTY_STRING;
  }
  return FormatElement(rootElement, schemaFile, schemaVersion);
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
  xmlStream << XMLHEADER << EOL;
  FormatXmlElement(xmlStream, rootElement);
  return xmlStream.str();
}


void XmlFormatter::FormatXmlElement(ostringstream& xmlStream, XMLTreeElement* element, int level)
{
  if(!element) {
    return;
  }
  string identation;
  int i = level;
  for( i = level; i > 0 ; i--) {
    identation += IDENTATION;
  }

  string tag = element->GetTag();
  string text = element->GetText();
  xmlStream << identation + '<' << tag;
  map<string, string> attributes = element->GetAttributes();
  for (auto attribute : attributes) {
    xmlStream << ' ';
    xmlStream << attribute.first << "=\"" << ConvertSpecialChars(attribute.second) << "\"";
  }
  if (element->HasChildren()) {
    list<XMLTreeElement*> children = element->GetChildren();
    xmlStream << '>' << EOL;
    for (auto it = children.begin(); it != children.end(); it++) {
      // insert extra space between children on the level 0
      if ((level == 0) && (it != children.begin())) {
        xmlStream << EOL;
      }
      FormatXmlElement(xmlStream, *it, level + 1);
    }
    xmlStream << identation << "</" << tag << ">" << EOL;
  } else if (!text.empty()) {
    xmlStream << '>' << ConvertSpecialChars(text) << "</" << tag << ">" << EOL;
  } else {
    xmlStream << "/>" << EOL;
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
