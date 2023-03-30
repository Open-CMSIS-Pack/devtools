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

using namespace std;

// Singleton kernel object
static unique_ptr<ProjMgrKernel> theProjMgrKernel = 0;

ProjMgrKernel::ProjMgrKernel() :
  RteKernelSlim()
{
  m_callback = make_unique<ProjMgrCallback>();
  SetRteCallback(m_callback.get());
  m_callback.get()->SetRteKernel(this);
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

bool ProjMgrKernel::GetInstalledPacks(std::list<std::string>& pdscFiles, bool latest) {
  // Find pdsc files from the CMSIS_PACK_ROOT
  list<string> installedPdscFiles;
  RteFsUtils::GetPackageDescriptionFiles(installedPdscFiles, theProjMgrKernel->GetCmsisPackRoot(), 3);

  // Filter only latest versions according to CMSIS_PACK_ROOT file tree
  if (latest) {
    map<string, map<string, map<string, string>>> installedPacks;
    for (const auto& pdsc : installedPdscFiles) {
      fs::path pdscPath = fs::path(pdsc);
      const auto& versionPath = pdscPath.parent_path();
      const auto& namePath = versionPath.parent_path();
      const auto& vendorPath = namePath.parent_path();
      installedPacks[vendorPath.filename().generic_string()][namePath.filename().generic_string()][versionPath.filename().generic_string()] = pdsc;
    }
    for (const auto& [vendor, packs] : installedPacks) {
      for (const auto& [pack, versions] : packs) {
        string latestVersion;
        for (const auto& [version, pdsc] : versions) {
          if (VersionCmp::Compare(latestVersion, version) <= 0) {
            latestVersion = version;
          }
        }
        pdscFiles.push_back(versions.at(latestVersion));
      }
    }
  } else {
    pdscFiles.splice(pdscFiles.end(), installedPdscFiles);
  }

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
  std::list<RtePackage*> newPacks;
  pdscFiles.sort();
  pdscFiles.unique();
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
      newPacks.push_back(pack);
    }
  }

  RteGlobalModel* globalModel = theProjMgrKernel->GetGlobalModel();
  if (!globalModel) {
    return false;
  }

  globalModel->InsertPacks(newPacks);
  packs.insert(packs.end(), newPacks.begin(), newPacks.end());
  return true;
}
