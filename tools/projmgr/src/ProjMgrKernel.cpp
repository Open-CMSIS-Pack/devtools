/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgrKernel.h"
#include "ProjMgrCallback.h"
#include "ProjMgrXmlParser.h"

#include "RteFsUtils.h"
#include "RteKernel.h"

#include <iostream>
#include <iterator>

using namespace std;

// Singleton kernel object
static unique_ptr<ProjMgrKernel> theProjMgrKernel = 0;

ProjMgrKernel::ProjMgrKernel() {
  m_callback = make_unique<ProjMgrCallback>();
  SetRteCallback(m_callback.get());
  m_callback.get()->SetRteKernel(this);
}

ProjMgrKernel::~ProjMgrKernel() {
  if (m_callback) {
    m_callback.reset();
  }
}

ProjMgrCallback* ProjMgrKernel::GetCallback() {
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

XMLTree* ProjMgrKernel::CreateXmlTree() const
{
  // RteKernel has the pointer ownership
  unique_ptr<ProjMgrXmlParser> xmlParser = make_unique<ProjMgrXmlParser>();
  return xmlParser.release();
}

bool ProjMgrKernel::GetInstalledPacks(std::list<std::string>& pdscFiles) {
  // Find pdsc files from the CMSIS_PACK_ROOT
  RteFsUtils::GetPackageDescriptionFiles(pdscFiles, theProjMgrKernel->GetCmsisPackRoot(), 3);

  // Find pdsc files from local repository
  list<string> localPdscUrls;
  if (!GetLocalPacksUrls(theProjMgrKernel->GetCmsisPackRoot(), localPdscUrls)) {
    return false;
  }
  for (const auto& localPdscUrl : localPdscUrls) {
    list<string> localPdscFiles;
    RteFsUtils::GetPackageDescriptionFiles(localPdscFiles, localPdscUrl, 1);
    // Insert local pdsc files first
    pdscFiles.insert(pdscFiles.begin(), localPdscFiles.begin(), localPdscFiles.end());
  }

  return true;
}

bool ProjMgrKernel::LoadAndInsertPacks(std::list<RtePackage*>& packs, std::list<std::string>& pdscFiles) {
  for (const auto& pdscFile : pdscFiles) {
    RtePackage* pack = theProjMgrKernel->LoadPack(pdscFile);
    if (!pack) {
      return false;
    }
    bool loaded = false;
    for (const auto& loadedPack : packs) {
      if (pack->GetPackageID() == loadedPack->GetPackageID()) {
        loaded = true;
        break;
      }
    }
    if (!loaded) {
      packs.push_back(pack);
    }
  }

  RteGlobalModel* globalModel = theProjMgrKernel->GetGlobalModel();
  if (!globalModel) {
    return false;
  }

  globalModel->InsertPacks(packs);
  return true;
}
