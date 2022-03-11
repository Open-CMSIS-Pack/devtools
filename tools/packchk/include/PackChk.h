/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PACKCHK_H
#define PACKCHK_H

#include "RteModel.h"
#include "RtePackage.h"
#include "PackOptions.h"

#include "ErrLog.h"

#include <string>
#include <set>


class PackChk {
public:
  PackChk();
  ~PackChk();

  int Check(int argc, const char* argv[], const char* envp[]);

  const RteGlobalModel& GetModel() { return m_rteModel; }

protected:
  bool InitMessageTable();
  bool CheckPackage();
  bool CreatePacknameFile(const std::string& filename, RtePackage* pKg);

private:
  CPackOptions m_packOptions;
  RteGlobalModel m_rteModel;

  static const MsgTable msgTable;
  static const MsgTableStrict msgStrictTable;
};

#endif // PACKCHK_H
