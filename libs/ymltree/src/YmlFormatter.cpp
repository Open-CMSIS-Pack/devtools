/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "YmlFormatter.h"
#include "RteUtils.h"

#include "yaml-cpp/yaml.h"

#include <list>
#include <map>

using namespace std;


static void EmitElement(YAML::Emitter& emitter, XMLTreeElement* element)
{
  const string& tag = element->GetTag();
  if (!tag.empty() && tag != RteUtils::DASH_STRING) {
    emitter << tag;
  }
  // sequence element
  auto& attributes = element->GetAttributes();
  const string& text = element->GetText();
  auto& children = element->GetChildren();
  if (attributes.empty() && children.empty()) {
    if(!text.empty()) {
      emitter << text;
    } else {
      emitter << YAML::Null;
    }
    return;
  }

  if (element->GetFirstChild(RteUtils::DASH_STRING)) { //sequence
    emitter << YAML::BeginSeq;
    for (auto child : element->GetChildren()) {
      EmitElement(emitter, child);
    }
    emitter << YAML::EndSeq;
    return;
  }
   // treat everything else as a map
  emitter << YAML::BeginMap;
  if(!attributes.empty()) {
    for(auto [k, v] : attributes) {
      emitter << YAML::Key << k;
      if(!v.empty()) {
        emitter << YAML::Value << v;
      } else {
        emitter << YAML::Null;
      }
    }
  }

  if (!text.empty()) {
    emitter << YAML::Key << "_text_" << YAML::Value << text;
  }

  if (element->HasChildren()) {
    for (auto child : element->GetChildren()) {
      EmitElement(emitter, child);
    }
  }
  emitter << YAML::EndMap;
}

YmlFormatter::YmlFormatter()  {

}

std::string YmlFormatter::FormatElement(XMLTreeElement* rootElement,
  const std::string& schemaFile,
  const std::string& schemaVersion)
{
  if (!rootElement) {
    return RteUtils::EMPTY_STRING;
  }
  YAML::Emitter emitter;
  emitter.SetNullFormat(YAML::EmptyNull);
  emitter << YAML::BeginMap;
  if (rootElement->GetChildCount() > 1 && rootElement->GetTag().empty()) {
    for (auto child : rootElement->GetChildren()) {
      EmitElement(emitter, child);
    }
  } else {
    EmitElement(emitter, rootElement);
  }
  emitter << YAML::EndMap;
  return emitter.c_str();
}

// end of YmlFormatter.cpp
