/*
 * Copyright (c) 2020-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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

void AbstractFormatter::CollectSortedChildren(XMLTreeElement* element, std::vector< std::pair<std::string, std::vector<XMLTreeElement*> > >& sortedChildren)
{
  if (!element || !element->HasChildren()) {
    return;
  }
  auto& children = element->GetChildren();
  for (auto child : children) {
    const string& tag = child->GetTag();
    auto it = sortedChildren.begin();
    for (; it != sortedChildren.end(); it++) {
      if (it->first == tag) {
        break;
      }
    }
    if (it == sortedChildren.end()) {
      it = sortedChildren.insert(sortedChildren.end(), make_pair(tag, vector<XMLTreeElement*>()));
    }
    it->second.push_back(child);
  }
}



// end of AbstractFormatter.cpp
