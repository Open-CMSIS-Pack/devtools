/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CbuildKernel.h"
#include "CbuildCallback.h"

#include "RteKernel.h"
#include "RteValueAdjuster.h"
#include "ErrLog.h"
#include "XMLTreeSlim.h"

#include <iostream>

// Singleton kernel object
static CbuildKernel *theCbuildKernel = 0;

CbuildKernel::CbuildKernel(RteCallback* callback) : RteKernel(callback) {
  m_model = new CbuildModel();
  m_callback = dynamic_cast<CbuildCallback*>(callback);
  if (m_callback) {
    m_callback->SetRteKernel(this);
  }
}

CbuildKernel::~CbuildKernel() {
  if (m_model)
    delete m_model;
  m_model = 0;
  if (m_callback)
    delete m_callback;
  m_callback = 0;
}

CbuildKernel *CbuildKernel::Get() {
  if (!theCbuildKernel) {
    CbuildCallback* cb = new CbuildCallback();
    theCbuildKernel = new CbuildKernel(cb);
  }
  return theCbuildKernel;
}

void CbuildKernel::Destroy() {
  if (theCbuildKernel)
    delete theCbuildKernel;
  theCbuildKernel = 0;
}

class CbuildXmlParser : public XMLTreeSlim
{
public:
  CbuildXmlParser() :XMLTreeSlim(0, true) { m_XmlValueAdjuster = new RteValueAdjuster(); }
  ~CbuildXmlParser() { delete m_XmlValueAdjuster; }
};

XMLTree* CbuildKernel::CreateXmlTree() const
{
  return new CbuildXmlParser();
}

bool CbuildKernel::Construct(const CbuildRteArgs& args) {

  if (m_model->Create(args))
    return true;

  if (ErrLog::Get()->GetErrCnt() == 0) {
    // Construct RTE Model failed
    LogMsg("M607");
  }

  for(auto msg : CbuildKernel::Get()->GetCallback()->GetErrorMessages()) {
    LogMsg("M800", MSG(msg));
  }

  return false;
}
