#ifndef RteKernelSlim_H
#define RteKernelSlim_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteSlimKernel.h
* @brief CMSIS RTE Data Model : simple Kernel to be used for testing

  @notes:
    - this file is supposed to be used as include only,
    - it is not linked to RteModel.lib.
    - requires linking of XMLTreeSlim.lib
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteKernel.h"
#include "RteValueAdjuster.h"
#include "RteItemBuilder.h"

#include "XMLTreeSlim.h"

// use simple XMLTree parser

/**
 * @brief header-only class that uses simple XMLTree parser to parse cprj project and pdsc files
*/
class RteXmlTreeSlim : public XMLTreeSlim
{
public:
  /**
   * @brief constructor
  */
  RteXmlTreeSlim(IXmlItemBuilder* itemBuilder) : XMLTreeSlim(itemBuilder, true, false) {
    SetXmlValueAdjuster(new RteValueAdjuster(false));
  }

  /**
   * @brief virtual destructor
  */
   ~RteXmlTreeSlim() override { delete m_XmlValueAdjuster; }
};

/**
 * @brief using XMLTree parser this class exposes cprj project which consumes CMSIS RTE data
*/
class RteKernelSlim : public RteKernel
{
public:
  /**
   * @brief constructor
   * @param callback RteCallback pointer
   * @param globalModel RteGlobalModel pointer
  */
  RteKernelSlim(RteCallback* callback = nullptr, RteGlobalModel* globalModel = nullptr) : RteKernel(callback, globalModel) {}

  /**
   * @brief constructor
   * @param callback RteCallback pointer
  */
  RteKernelSlim(RteGlobalModel* globalModel) : RteKernel(nullptr, globalModel) {}


protected:
   XMLTree* CreateXmlTree(IXmlItemBuilder* itemBuilder) const override { return new RteXmlTreeSlim(itemBuilder);}
};

#endif // RteKernelSlim_H
