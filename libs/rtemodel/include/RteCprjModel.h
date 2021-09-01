#ifndef RteCprjModel_H
#define RteCprjModel_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteCprjModel.h
* @brief CMSIS RTE for *.cprj files
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteModel.h"

class CprjFile;

/**
 * @brief class containing CMSIS RTE data of cprj project
*/
class RteCprjModel : public RteGeneratorModel
{
public:
  /**
   * @brief constructor
   * @param parent pointer to the parent object
  */
  RteCprjModel(RteItem* parent);

  /**
   * @brief default constructor
  */
  RteCprjModel();

  /**
   * @brief destructor
  */
  virtual ~RteCprjModel() override;

  /**
   * @brief clean up CMSIS RTE data
  */
  virtual void Clear() override;

  /**
   * @brief clean up CMSIS RTE data
  */
  virtual void ClearModel() override;

  /**
   * @brief validate the CMSIS RTE data
   * @return
  */
  virtual bool Validate() override;

  /**
   * @brief getter for CprjFile object associated with this instance
   * @return CprjFile pointer
  */
  CprjFile* GetCprjFile() const;

};

#endif // RteCprjModel_H
