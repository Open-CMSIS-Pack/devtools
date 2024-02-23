/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteItemBuilder.h
 *  @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteItemBuilder.h"

#include "RteModel.h"
#include "CprjFile.h"

using namespace std;

RteItemBuilder::RteItemBuilder(RteItem* rootParent, PackageState packState) :
  XmlTreeItemBuilder<RteItem>(),
  m_rootParent(rootParent),
  m_packState(packState),
  m_cprjFile(nullptr)
{
};



RteItem* RteItemBuilder::CreateRootItem(const string& tag)
{
  RteItem* pRoot = nullptr;
  if (tag == "package" || tag == "generator-import") {
    RtePackage* pack = new RtePackage(m_rootParent, m_packState);
    m_packs.push_back(pack);
    pRoot = pack;
  } else if (tag == "cprj") {
    m_cprjFile = new CprjFile(m_rootParent);
    pRoot = m_cprjFile;
  } else {
    pRoot = new RteRootItem(m_rootParent);
  }
  pRoot->SetRootFileName(GetFileName());
  return pRoot;
}

RtePackage* RteItemBuilder::GetPack() const
{
  auto it = m_packs.begin();
  if (it != m_packs.end()) {
    return *it;
  }
  return nullptr;
}

// end of RteItemBuilder.cpp
