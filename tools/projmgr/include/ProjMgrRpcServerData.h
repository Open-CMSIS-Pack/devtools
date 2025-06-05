/*
 * Copyright (c) 2025 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PROJMGRRPCSERVERDATA_H
#define PROJMGRRPCSERVERDATA_H

#include "RpcInterface.h"

using namespace std;

class RteTarget;
class RteBundle;
class RteComponent;
class RteComponentInstance;
class RteComponentAggregate;
class RteComponentGroup;
class RteItem;
class RpcDataCollector {
public:
  RpcDataCollector(RteTarget* t);

  RteTarget* GetTarget() const { return m_target; }

  void CollectCtClasses(RpcArgs::CtRoot& ctRoot) const;
  void CollectUsedItems(RpcArgs::UsedItems& usedItems) const;

  RpcArgs::Component FromRteComponent(const RteComponent* rteComponent) const;
  RpcArgs::ComponentInstance FromComponentInstance(const RteComponentInstance* rteCi) const;
  RteItem* GetTaxonomyItem(const RteComponentGroup* rteGroup) const;

protected:

  void CollectCtBundles(RpcArgs::CtClass& ctClass, RteComponentGroup* rteClass) const;
  void CollectCtChildren(RpcArgs::CtTreeItem& parent, RteComponentGroup* rteGroup, const string& bundleName) const;
  void CollectCtAggregates(RpcArgs::CtTreeItem& parent, RteComponentGroup* rteGroup, const string& bundleName) const;
  void CollectCtVariants(RpcArgs::CtAggregate& ctAggregate, RteComponentAggregate* rteAggregate) const;

private:
  RteTarget* m_target;
};

#endif // PROJMGRRPCSERVERDATA_
