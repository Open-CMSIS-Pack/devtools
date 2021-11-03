/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VALIDATE_H
#define VALIDATE_H

#include "PackOptions.h"
#include "RteModelReader.h"

class Validate
{
public:
  Validate(RteGlobalModel& rteModel, CPackOptions& packOptions);
  ~Validate();

protected:
  RteGlobalModel& GetModel() { return m_rteModel; }
  CPackOptions& GetOptions() { return m_packOptions; }

private:
  RteGlobalModel& m_rteModel;
  CPackOptions& m_packOptions;
};

#endif // VALIDATE_H
