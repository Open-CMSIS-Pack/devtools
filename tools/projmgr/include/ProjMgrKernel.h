/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRKERNEL_H
#define PROJMGRKERNEL_H

#include "ProjMgrCallback.h"
#include "RteKernel.h"

/**
 * @brief extension to RTE Kernel
*/
class ProjMgrKernel : public RteKernel {
public:
  /**
   * @brief class constructor
  */
  ProjMgrKernel();

  /**
   * @brief class destructor
  */
  virtual ~ProjMgrKernel();

  /**
   * @brief get kernel
   * @return pointer to singleton kernel instance
  */
  static ProjMgrKernel *Get();

  /**
   * @brief destroy kernel
  */
  static void Destroy();

  /**
   * @brief get installed packs
   * @param pdscFiles list of installed packs
   * @return true if executed successfully
  */
  bool GetInstalledPacks(std::list<std::string>& pdscFiles);

  /**
   * @brief load and insert pack into global model
   * @param packs list of loaded packages
   * @param pdscFiles list of packs to be loaded
   * @return true if executed successfully
  */
  bool LoadAndInsertPacks(std::list<RtePackage*>& packs, std::list<std::string>& pdscFiles);

  /**
   * @brief get callback
   * @return pointer to callback
  */
  ProjMgrCallback* GetCallback();

protected:
  XMLTree* CreateXmlTree() const;

private:
  std::unique_ptr<ProjMgrCallback> m_callback;
};

#endif /* PROJMGRKERNEL_H */
