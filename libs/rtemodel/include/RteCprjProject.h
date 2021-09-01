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

class RteCprjModel;
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
   * @param cprjModel pointer to an RteCprjModel object containing CMSIS RTE data
  */
  RteCprjProject(RteCprjModel* cprjModel);

  /**
   * @brief destructor
  */
  virtual ~RteCprjProject() override;

  /**
   * @brief getter for the RteCprjModel object containing CMSIS RTE data associated with this project
   * @return RteCprjModel pointer
  */
  RteCprjModel* GetCprjModel() const { return m_cprjModel; }

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

public:
  /**
   * @brief clean up the project
  */
  virtual void Clear() override;

  /**
   * @brief create target based on cprj description
  */
  virtual void Initialize() override;

public:
  /**
   * @brief set toolchain for the project
   * @param toolchain name of the toolchain
   * @return true if toolchain is supported
  */
  bool SetToolchain(const std::string& toolchain);

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

protected:
  void FillToolchainAttributes(RteAttributes &attributes) const;
  virtual RteTarget* CreateTarget(RteModel* filteredModel, const std::string& name, const std::map<std::string, std::string>& attributes) override;
  virtual void PropagateFilteredPackagesToTargetModel(const std::string& targetName) override;

protected:
  RteCprjModel* m_cprjModel; // model read from .cprj file
  std::string m_toolchain;        // toolchain to use: AC6, AC5, ARMCC, GCC
  std::string m_toolchainVersion; // toolchain version

};

#endif // RteCprjProject_H
