/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdConv_H
#define SvdConv_H

#include "SvdOptions.h"
#include "ErrLog.h"

#include <string>
#include <set>


typedef enum SvdErr_t
{
  SVD_ERR_SUCCESS = 0,       // Success
  SVD_ERR_INVALID_PARAM = 1, // Invalid param, e.g. an unallowed NULL pointer
  SVD_ERR_NO_INPATH = 2,     // No input path specified
  SVD_ERR_VERIFY = 4,        // One or more verification actions failed
  SVD_ERR_NOT_FOUND = 8,     // File not found
  SVD_ERR_INTERNAL_ERR = 11  // Internal Error
} SVD_ERR;


class SvdConv {
public:
  SvdConv();
  ~SvdConv();

  int Check(int argc, const char* argv[], const char* envp[]);
  SVD_ERR CheckSvdFile();

protected:
  bool InitMessageTable();

private:
  SvdOptions m_svdOptions;

  static const MsgTable msgTable;
  static const MsgTableStrict msgStrictTable;

};

#endif // SVDConv_H
