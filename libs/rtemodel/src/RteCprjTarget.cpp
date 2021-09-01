/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteCprjTarget.cpp
* @brief CMSIS RTE for *.cprj files
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteCprjTarget.h"

#include "RteProject.h"
#include "RteInstance.h"
#include "RteModel.h"

using namespace std;

RteCprjTarget::RteCprjTarget(RteItem* parent, RteModel* filteredModel,
  const string& name, const map<string, string>& attributes) :
  RteTarget(parent, filteredModel, name, attributes)
{
}

RteCprjTarget::~RteCprjTarget()
{
  RteCprjTarget::Clear();
}


void RteCprjTarget::Clear()
{
  RteTarget::Clear();
}

// End of RteCprjTarget.cpp
