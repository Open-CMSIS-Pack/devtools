/******************************************************************************/
/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "YmlTreeParserInterface.h"
#include "RteFsUtils.h"

#include <sstream>

using namespace std;
using namespace YAML;


YmlTreeParserInterface::YmlTreeParserInterface(YmlTree* tree) :
  XMLTreeParserInterface(tree)
{
}

YmlTreeParserInterface::~YmlTreeParserInterface()
{
}

bool YmlTreeParserInterface::Init()
{
  // does nothing for underlying YAML Parser
  return true;
}


void YmlTreeParserInterface::Clear() // does not destroy YAML parser!
{
  try {
    m_root.reset();
  } catch(YAML::Exception& e) {
    // do nothing
  }
}


bool YmlTreeParserInterface::Parse(const std::string& fileName, const std::string& inputString)
{
  bool success = true;
  m_errorStrings.clear();
  m_nErrors = 0;
  m_nWarnings = 0;

  m_xmlFile = RteFsUtils::MakePathCanonical(fileName);
  try {
    if (!inputString.empty()) {
      m_root = YAML::Load(inputString);
    } else {
      m_root = YAML::LoadFile(m_xmlFile);
    }
    success = ParseNode(m_root, RteUtils::EMPTY_STRING);

  } catch (YAML::Exception& e) {
    stringstream ss;
    ss << fileName << "(" << (e.mark.line + 1) << "," << (e.mark.column + 1) << "):" << e.msg;
    m_errorStrings.push_back(ss.str());
    m_nErrors++;
    success = false;
  }
  m_xmlFile.clear();
  return success;
}

bool YmlTreeParserInterface::ParseNode(const YAML::Node& node, const string& tag)
{
  bool success = true;
  IXmlItemBuilder* builder = m_tree->GetXmlItemBuilder();
  builder->PreCreateItem();
  if (!tag.empty() || (!builder->HasRoot() && node.size() > 1)) {
    success = builder->CreateItem(tag);
  }
  if (success) {
    builder->SetLineNumber(node.Mark().line + 1); // make one-based
    success = DoParseNode(node, tag);
    builder->AddItem();
  }
  builder->PostCreateItem(success);
  return success;
}

bool YmlTreeParserInterface::ParseMapNode(const YAML::Node& node)
{
  IXmlItemBuilder* builder = m_tree->GetXmlItemBuilder();
  for (auto it : node) {
    auto& key = it.first;
    auto& val = it.second;
    const string tag = key.as<string>();
    if ((val.IsScalar() || val.IsNull()) && builder->HasRoot()) {
      const string value = val.IsScalar() ? val.as<string>() : "";
      builder->AddAttribute(tag, value);
    } else {
      if(!ParseNode(val, tag)) {
        return false;
      }
    }
  }
  return true;
}

bool YmlTreeParserInterface::ParseSequenceNode(const YAML::Node& node)
{
  for (auto it : node) {
    if (!ParseNode(it, RteUtils::DASH_STRING)) {
      return false;
    }
  }
  return true;
}

bool YmlTreeParserInterface::ParseScalarNode(const YAML::Node& node)
{
  m_tree->GetXmlItemBuilder()->SetText(node.as<string>());
  return true;
}


bool YmlTreeParserInterface::DoParseNode(const YAML::Node& node, const string& tag)
{
  NodeType::value type = node.Type();
  switch (type) {
  case NodeType::Scalar:
    return ParseScalarNode(node);

  case NodeType::Sequence:
    return ParseSequenceNode(node);

  case NodeType::Map:
    return ParseMapNode(node);

  case NodeType::Null:
  case NodeType::Undefined:
  default:
    break;
  };
  return true;
}

bool YmlTreeParserInterface::CreateItem(const std::string& tag, int line)
{
  IXmlItemBuilder* builder = m_tree->GetXmlItemBuilder();
  if (!builder->CreateItem(tag)) {
    return false;
  }
  builder->SetLineNumber(line + 1); // make one-based
  return true;
}

// end of YmlTreeParserInterface.cpp
