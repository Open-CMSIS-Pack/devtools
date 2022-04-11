/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "XMLTree.h"
#include "AbstractFormatter.h"

using namespace std;

const string AbstractFormatter::EOL_STRING = "\n";

AbstractFormatter::AbstractFormatter() {
}

string AbstractFormatter::Format(XMLTree* xmlTree, const string& schemaFile, const string& schemaVersion)
{
  XMLTreeElement* rootElement = xmlTree && xmlTree->GetRoot() ? xmlTree->GetRoot()->GetFirstChild() : nullptr;
  if (!rootElement) {
    return XMLTree::EMPTY_STRING;
  }
  return FormatElement(rootElement, schemaFile, schemaVersion);
}

string AbstractFormatter::FormatElement(XMLTreeElement* rootElement, const string& schemaFile, const string& schemaVersion)
{
  if (!rootElement) {
    return XMLTree::EMPTY_STRING;
  }
  // Format elements recursively
  ostringstream xmlStream;
  FormatXmlElement(xmlStream, rootElement);
  return xmlStream.str();
}

string AbstractFormatter::GetIndentString(int level) const
{
  if (level <= 0) {
    return XMLTree::EMPTY_STRING;
  }

  return string(((std::size_t)level) << 1, ' ');
}


// end of AbstractFormatter.cpp
