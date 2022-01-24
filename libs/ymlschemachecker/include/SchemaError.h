/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SCHEMAERROR_H
#define SCHEMAERROR_H

#include <string>
#include <list>

struct SchemaError
{
  SchemaError(const std::string& errMsg, int line, int column) :
    m_line(line), m_col(column), m_Msg(errMsg) { }

  int         m_line;
  int         m_col;
  std::string m_Msg;
};

typedef std::list<SchemaError> SchemaErrors;

#endif // SCHEMAERROR_H
