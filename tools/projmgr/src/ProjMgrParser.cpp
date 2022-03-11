/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrParser.h"
#include "ProjMgrYamlParser.h"

#include <iostream>
#include <string>

using namespace std;

// Parser class for public interfacing
// ParseCsolution and ParseCproject are forwarded to the implementation class

ProjMgrParser::ProjMgrParser(void) {
  // Reserved
}

ProjMgrParser::~ProjMgrParser(void) {
  // Reserved
}

bool ProjMgrParser::ParseCsolution(const string& input, bool checkSchema) {
  // Parse solution file
  return ProjMgrYamlParser().ParseCsolution(input, m_csolution, checkSchema);
}

bool ProjMgrParser::ParseCproject(const string& input, bool checkSchema, bool single) {
  // Parse project
  return ProjMgrYamlParser().ParseCproject(
    input, m_csolution, m_cprojects, single, checkSchema);
}

bool ProjMgrParser::ParseClayer(const string& input, bool checkSchema) {
  // Parse layer file
  return ProjMgrYamlParser().ParseClayer(input, m_clayers, checkSchema);
}

CsolutionItem& ProjMgrParser::GetCsolution(void) {
  return m_csolution;
}

map<string, CprojectItem>& ProjMgrParser::GetCprojects(void) {
  return m_cprojects;
}

map<string, ClayerItem>& ProjMgrParser::GetClayers(void) {
  return m_clayers;
}
