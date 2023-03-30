/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef RTEMODELREADER_H
#define RTEMODELREADER_H

#include "RteModel.h"
#include "RteValueAdjuster.h"
#include "RteItemBuilder.h"
#include "XMLTreeSlim.h"
#include "ErrLog.h"


class RteModelReaderErrorVistior : public RteVisitor
{
public:
  virtual VISIT_RESULT Visit(RteItem* rteItem);
};


class ValueAdjuster : public RteValueAdjuster
{
public:
  ValueAdjuster() {}

  virtual std::string AdjustPath(const std::string& fileName, int lineNo);

  bool CheckPath(const std::string& fileName, int lineNo);

private:
};


class RteModelReader
{
public:
  RteModelReader(RteGlobalModel& m_rteModel);
  ~RteModelReader();

  bool AddFile(const std::string& fileName);
  bool ReadAll();

private:
  RteGlobalModel& m_rteModel;
  RteItemBuilder m_rteItemBuilder;
  XMLTreeSlim m_xmlTree;
  ValueAdjuster m_valueAdjuster;
};

#endif // RTEMODELREADER_H
