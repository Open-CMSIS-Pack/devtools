/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRKERNEL_H
#define PROJMGRKERNEL_H

#include "ProjMgrCallback.h"
#include "RteKernelSlim.h"

/**
 * @brief extension to RTE Kernel
*/
class ProjMgrKernel : public RteKernelSlim {
public:
  /**
   * @brief class constructor
  */
  ProjMgrKernel();

  /**
   * @brief class destructor
  */
  ~ProjMgrKernel() override;

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
   * @brief get callback
   * @return pointer to callback
  */
  ProjMgrCallback* GetCallback() const;

private:
  std::unique_ptr<ProjMgrCallback> m_callback;
};

#endif /* PROJMGRKERNEL_H */
