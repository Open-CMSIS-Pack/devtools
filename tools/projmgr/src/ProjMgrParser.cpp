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

bool ProjMgrParser::ParseCsolution(const string& input, CsolutionItem& csolution) {
  return ProjMgrYamlParser().ParseCsolution(input, csolution);
}

bool ProjMgrParser::ParseCproject(const string& input, CprojectItem& cproject) {
  return ProjMgrYamlParser().ParseCproject(input, cproject);
}
