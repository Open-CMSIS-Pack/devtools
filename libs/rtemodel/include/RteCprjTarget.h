#ifndef RteCprjTarget_H
#define RteCprjTarget_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteCprjTarget.h
* @brief CMSIS RTE for *.cprj files
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteTarget.h"

/**
 * @brief class representing target specified in cprj project
*/
class RteCprjTarget : public RteTarget
{
public:
  /**
   * @brief constructor
   * @param parent pointer to the parent of this instance
   * @param filteredModel CMSIS RTE data model filtered for this target
   * @param name target name
   * @param attributes collection of target attributes
  */
  RteCprjTarget(RteItem* parent, RteModel* filteredModel, const std::string& name,
    const std::map<std::string, std::string>& attributes);

  /**
   * @brief destructor
  */
   ~RteCprjTarget() override;

public:
  /**
   * @brief clean up this object
  */
   void Clear() override;
};

#endif // RteCprjTarget_H
