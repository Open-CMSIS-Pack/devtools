/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrXmlParser.h"

using namespace std;

ProjMgrXmlParser::ProjMgrXmlParser() : XMLTreeSlim(0, true) {
  m_adjuster = make_unique<RteValueAdjuster>(false);
  SetXmlValueAdjuster(m_adjuster.get());
}

ProjMgrXmlParser::~ProjMgrXmlParser() {
  if (m_adjuster) {
    m_adjuster.reset();
  }
}
