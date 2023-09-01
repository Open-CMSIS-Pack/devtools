#ifndef RteCprjProject_H
#define RteCprjProject_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteCprjProject.h
* @brief CMSIS RTE for *.cprj files
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteProject.h"

class RteModel;
class CprjFile;

/**
 * @brief class representing a cprj project
*/
class RteCprjProject : public RteProject
{
public:
  /**
   * @brief constructor
   * @param cprjModel pointer to CprjFile object containing CMSIS RTE data
  */
  RteCprjProject(CprjFile* cprjFile);

  /**
   * @brief destructor
  */
   ~RteCprjProject() override;


  /**
   * @brief getter for cprj file associated with this project
   * @return CprjFile pointer
  */
  CprjFile* GetCprjFile() const;

  /**
   * @brief getter for attribute "info" specified in cprj file
   * @return RteItem pointer
  */
  RteItem*  GetCprjInfo() const;

  /**
   * @brief get internal map of component ID to cprj component element
   * @return map of component ID string to RteItem pointer
  */
  const std::map<std::string, RteItem*>& GetCprjComponentMap() const { return m_idToCprjComponents; }

  /**
   * @brief get cprj component element for supplied component
   * @param id component ID
   * @return pointer RteItem* representing cprj component, nullptr if not found
  */
  RteItem* GetCprjComponent(const std::string& id) const;

  /**
   * @brief create target based on cprj description
  */
   void Initialize() override;

   /**
    * @brief get all packs required in the specified target
    * @param packs collection of package IDs mapped to RtePackage pointers to fill
    * @param targetName target name
   */
   void GetRequiredPacks(RtePackageMap& packs, const std::string& targetName) const override;

public:
  /**
   * @brief set toolchain for the project
   * @param toolchain name of the toolchain
   * @return true if toolchain is supported
  */
  bool SetToolchain(const std::string& toolchain, const std::string& toolChainVersion = RteUtils::EMPTY_STRING);

  /**
   * @brief getter for the selected toolchain
   * @return name of toolchain
  */
  std::string GetToolchain() { return m_toolchain; };

  /**
   * @brief getter for toolchain version
   * @return toolchain version string
  */
  std::string GetToolchainVersion() { return m_toolchainVersion; }

  /**
   * @brief apply selected components to CPRJ project
  */
  void ApplySelectedComponents();

protected:
  void FillToolchainAttributes(XmlItem &attributes) const;
   RteTarget* CreateTarget(RteModel* filteredModel, const std::string& name, const std::map<std::string, std::string>& attributes) override;
   void PropagateFilteredPackagesToTargetModel(const std::string& targetName) override;
   RteComponentInstance* AddCprjComponent(RteItem* item, RteTarget* target) override;
  void ApplySelectedComponentsToCprjFile();
  void ApplyComponentFilesToCprjFile(RteComponentInstance* ci, RteItem* cprjComponent);

protected:
  CprjFile* m_cprjFile; // data read from .cprj file
  std::string m_toolchain;        // toolchain to use: AC6, AC5, ARMCC, GCC
  std::string m_toolchainVersion; // toolchain version
  std::map<std::string, RteItem*> m_idToCprjComponents; // component ID mapped to RteItem in CPRJ project

};

#endif // RteCprjProject_H
