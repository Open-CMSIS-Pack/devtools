/******************************************************************************/
/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "YmlTreeParserInterface.h"
#include "RteFsUtils.h"

#include "SchemaChecker.h"
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
 // does nothing for underlying YAML Parser
}

bool YmlTreeParserInterface::ValidateSchema(const std::string& fileName)
{
  const string& schemaFile = m_tree->GetSchemaFileName();
  if (schemaFile.empty()) {
    return true; // no validation requested
  }
  if (!RteFsUtils::Exists(schemaFile)) {
    stringstream ss;
    ss << "Warning: schema file '" << schemaFile << "' not found, file cannot be validated";
    m_errorStrings.push_back(ss.str());
    m_nWarnings++;
    return true;  // still success
  }
  SchemaErrors errList;
  bool result = SchemaChecker::Validate(fileName, schemaFile, errList);
  for (auto& err : errList) {
    stringstream ss;
    ss << err.m_file << "(" << err.m_line << "," << err.m_col << "): error: " << err.m_msg;
    m_errorStrings.push_back(ss.str());
    m_nErrors++;
  }
  return result;
}


bool YmlTreeParserInterface::Parse(const std::string& fileName, const std::string& inputString)
{
  bool success = true;
  m_errorStrings.clear();
  m_nErrors = 0;
  m_nWarnings = 0;

  m_xmlFile = RteFsUtils::MakePathCanonical(fileName);
  try {
    YAML::Node root;
    if (!inputString.empty()) {
      root = YAML::Load(inputString);
    } else {
      if (!ValidateSchema(m_xmlFile)) {
        return false;
      }
      root = YAML::LoadFile(m_xmlFile);
    }
    success = ParseNode(root, RteUtils::EMPTY_STRING);

  } catch (YAML::Exception& e) {
    stringstream ss;
    ss << fileName << "(" << (e.mark.line + 1) << "," << (e.mark.column + 1) << "):" << e.msg;
    m_errorStrings.push_back(ss.str());
    m_nErrors++;
    success = false;
  }
  m_xmlFile = "";
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
    if (val.IsScalar() && builder->HasRoot()) {
      builder->AddAttribute(tag, val.as<string>());
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
    return false;
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
