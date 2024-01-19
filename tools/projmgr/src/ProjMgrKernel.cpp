/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProductInfo.h"
#include "ProjMgrKernel.h"
#include "ProjMgrCallback.h"
#include "ProjMgrXmlParser.h"

#include "RteFsUtils.h"
#include "RteKernel.h"

#include "CrossPlatformUtils.h"

#include <iostream>

using namespace std;

// Singleton kernel object
static unique_ptr<ProjMgrKernel> theProjMgrKernel = 0;

ProjMgrKernel::ProjMgrKernel() :
  RteKernelSlim()
{
  m_callback = make_unique<ProjMgrCallback>();
  SetRteCallback(m_callback.get());
  m_callback.get()->SetRteKernel(this);

  XmlItem attributes;
  attributes.AddAttribute("name", ORIGINAL_FILENAME);
  attributes.AddAttribute("version", VERSION_STRING);
  SetToolInfo(attributes);
  std::error_code ec;
  string exePath = RteUtils::ExtractFilePath( CrossPlatformUtils::GetExecutablePath(ec), true);
  if (!ec) {
    string cmsisToolboxDir = RteFsUtils::MakePathCanonical(exePath + "..");
    SetCmsisToolboxDir(cmsisToolboxDir);
  }

}

ProjMgrKernel::~ProjMgrKernel() {
  if (m_callback) {
    m_callback.reset();
  }
}

ProjMgrCallback* ProjMgrKernel::GetCallback() const {
  return m_callback.get();
}

ProjMgrKernel *ProjMgrKernel::Get() {
  if (!theProjMgrKernel) {
    theProjMgrKernel = make_unique<ProjMgrKernel>();
  }
  return theProjMgrKernel.get();
}

void ProjMgrKernel::Destroy() {
  if (theProjMgrKernel) {
    theProjMgrKernel.reset();
  }
}
