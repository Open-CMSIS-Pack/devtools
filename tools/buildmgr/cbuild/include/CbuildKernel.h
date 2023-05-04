/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CBUILDKERNEL_H
#define CBUILDKERNEL_H

#include "CbuildCallback.h"
#include "CbuildModel.h"
#include "RteKernelSlim.h"

class CbuildKernel : public RteKernelSlim
{
public:
  CbuildKernel(RteCallback* callback);
  ~CbuildKernel();

  /**
   * @brief get kernel
   * @return pointer to singleton kernel instance
  */
  static CbuildKernel *Get();

  /**
   * @brief destroy kernel
  */
  static void Destroy();

  /**
   * @brief construct RTE Model
   * @param args arguments to contruct RTE Model
   * @return true if RTE Model created successfully, otherwise false
  */
  bool Construct(const CbuildRteArgs& args);

  /**
   * @brief get Model
   * @return pointer to RTE Model
  */
  const CbuildModel* GetModel() {
    return m_model;
  }

  /**
   * @brief get callback
   * @return pointer to callback
  */
  CbuildCallback* GetCallback() const {
    return m_callback;
  }

private:
  CbuildModel    *m_model;
  CbuildCallback *m_callback;
};

#endif /* CBUILDKERNEL_H */
