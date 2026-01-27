/*
 * Copyright (c) 2025 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PROJMGRRPCSERVERDATA_H
#define PROJMGRRPCSERVERDATA_H

#include "RpcInterface.h"

#include <optional>
#include <list>
#include <set>

using namespace std;

using PackReferenceVector = std::vector<RpcArgs::PackReference>;

class RteTarget;
class RteBundle;
class RteComponent;
class RteComponentInstance;
class RteComponentAggregate;
class RteComponentGroup;
class RteDevice;
class RteDeviceItem;
class RteBoard;
class RteItem;
class RteModel;
class RpcDataCollector {
public:
  RpcDataCollector(RteTarget* t, RteModel* m = nullptr);

  RteTarget* GetTarget() const { return m_target; }

  void CollectCtClasses(RpcArgs::CtRoot& ctRoot) const;
  void CollectUsedComponents(vector< RpcArgs::ComponentInstance>& usedComponents) const;

  std::set<std::string> GetUsedPacks() const;

  void CollectDeviceList(RpcArgs::DeviceList& deviceList, const std::string& namePattern, const std::string& vendor) const;
  void CollectDeviceInfo(RpcArgs::DeviceInfo& deviceInfo, const std::string& id) const;

  void CollectBoardList(RpcArgs::BoardList& boardList, const std::string& namePattern, const std::string& vendor) const;
  void CollectBoardInfo(RpcArgs::BoardInfo& boardInfo, const std::string& id) const;

  RpcArgs::Device FromRteDevice(RteDeviceItem* rteDevice, bool bIncludeProperties) const;
  RpcArgs::Board  FromRteBoard(RteBoard* rteBoard, bool bIncludeProperties) const;

  RpcArgs::Component FromRteComponent(const RteComponent* rteComponent) const;
  RpcArgs::ComponentInstance FromComponentInstance(const RteComponentInstance* rteCi) const;
  RteItem* GetTaxonomyItem(const RteComponentGroup* rteGroup) const;

  std::optional<RpcArgs::Options> OptionsFromRteItem(const RteItem* item) const;
  std::string ResultStringFromRteItem(const RteItem* item) const;

protected:
  void CollectBoardDevices(vector<RpcArgs::Device>& boardDevices, RteBoard* rteBoard, bool bMounted, std::list<RteDevice*>& processedDevices) const;
  void CollectCtBundles(RpcArgs::CtClass& ctClass, RteComponentGroup* rteClass) const;
  void CollectCtChildren(RpcArgs::CtTreeItem& parent, RteComponentGroup* rteGroup, const string& bundleName) const;
  void CollectCtAggregates(RpcArgs::CtTreeItem& parent, RteComponentGroup* rteGroup, const string& bundleName) const;
  void CollectCtVariants(RpcArgs::CtAggregate& ctAggregate, RteComponentAggregate* rteAggregate) const;

private:
  RteTarget* m_target; // target for context-specific data
  RteModel* m_model;   // RTE model for global data
};

#endif // PROJMGRRPCSERVERDATA_
