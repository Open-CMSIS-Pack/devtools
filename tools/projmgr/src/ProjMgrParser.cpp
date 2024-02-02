/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
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

bool ProjMgrParser::ParseCdefault(const string& input, bool checkSchema) {
  // Parse solution file
  return ProjMgrYamlParser().ParseCdefault(input, m_cdefault, checkSchema);
}

bool ProjMgrParser::ParseCsolution(const string& input, bool checkSchema, bool frozenPacks) {
  // Parse solution file
  return ProjMgrYamlParser().ParseCsolution(input, m_csolution, checkSchema, frozenPacks);
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

bool ProjMgrParser::ParseGenericClayer(const string& input, bool checkSchema) {
  // Parse generic layer file
  return ProjMgrYamlParser().ParseClayer(input, m_genericClayers, checkSchema);
}

bool ProjMgrParser::ParseCbuildSet(const string& input, bool checkSchema) {
  // Parse cbuild-set file
  return ProjMgrYamlParser().ParseCbuildSet(input, m_cbuildSet, checkSchema);
}

CdefaultItem& ProjMgrParser::GetCdefault(void) {
  return m_cdefault;
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

map<string, ClayerItem>& ProjMgrParser::GetGenericClayers(void) {
  return m_genericClayers;
}

CbuildSetItem& ProjMgrParser::GetCbuildSetItem(void) {
  return m_cbuildSet;
}

// end of ProjMgrParser.cpp
