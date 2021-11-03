/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Validate.h"

/**
 * @brief class constructor
 * @param rteModel RTE model to run on
 * @param packOptions configured program options
*/
Validate::Validate(RteGlobalModel& rteModel, CPackOptions& packOptions) :
  m_rteModel(rteModel),
  m_packOptions(packOptions)
{
}

/**
 * @brief class destructor
*/
Validate::~Validate()
{
}
