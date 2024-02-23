#ifndef RteItemBuilder_H
#define RteItemBuilder_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteItemBuilder.h
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 /******************************************************************************/

#include "XmlTreeItemBuilder.h"

#include "RteItem.h"
#include <list>

class RtePackage;
class CprjFile;

/**
 * @brief class to create RtItem objects
*/
class RteItemBuilder : public XmlTreeItemBuilder<RteItem>
{
public:
  /**
   * @brief default constructor
   * @param rootParent pointer to RteItem to be parent for root items (optional)
   * @param packState PackageState value
  */
  RteItemBuilder(RteItem* rootParent = nullptr, PackageState packState = PackageState::PS_UNKNOWN);

  /**
   * @brief virtual function to create an RteItem specified by tag
   * @param tag name of new tag
   * @return pointer to RteItem
  */
  RteItem* CreateRootItem(const std::string& tag) override;

  /**
   * @brief get collection of created RtePackage items
   * @return list of RtePackage pointers
  */
  const std::list<RtePackage*>& GetPacks() const { return m_packs; }

  /**
   * @brief get first read pack
   * @return pointer to RtePackage
  */
  RtePackage* GetPack() const;

  /**
   * @brief get pointer to created CprjFile item
   * @return pointer to CprjFile
  */
  CprjFile* GetCprjFile() const { return m_cprjFile; }

  /**
   * @brief set pack state
   * @param packState PackageState value
  */
  void SetPackageState(PackageState packState) { m_packState = packState; }

protected:
  RteItem* m_rootParent;
  PackageState m_packState;

  CprjFile* m_cprjFile;
  std::list<RtePackage*> m_packs;
};

#endif // RteItemBuilder_H
