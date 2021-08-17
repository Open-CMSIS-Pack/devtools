#ifndef RteInstance_H
#define RteInstance_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteInstance.h
* @brief CMSIS RTE Instance in uVision Project
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteItem.h"
#include "RteFile.h"
#include "RteComponent.h"

#include <stdio.h>
#include <map>

class RteModel;
class RteGeneratorModel;
class RteProject;
class RteFileInstance;
class RteTarget;
class RteProject;

class RteInstanceTargetInfo : public RteAttributes
{
public:

  enum RteOptType { MEMOPT, COPT, ASMOPT, OPTCOUNT };

  RteInstanceTargetInfo();
  RteInstanceTargetInfo(RteInstanceTargetInfo* info);
  RteInstanceTargetInfo(const std::map<std::string, std::string>& attributes);

  bool IsExcluded() const { return m_bExcluded; }
  bool SetExcluded(bool excluded);

  bool IsIncludeInLib() const { return m_bIncludeInLib; }
  bool SetIncludeInLib(bool include);


  int GetInstanceCount() const { return m_instanceCount; }
  void SetInstanceCount(int count);

  VersionCmp::MatchMode GetVersionMatchMode() const { return m_VersionMatchMode; }
  bool SetVersionMatchMode(VersionCmp::MatchMode mode);

  void CopySettings(const RteInstanceTargetInfo& other);
  const RteAttributes& GetMemOpt() const { return m_memOpt; }
  const RteAttributes& GetCOpt() const { return m_cOpt; }
  const RteAttributes& GetAsmOpt() const { return m_asmOpt; }
  const RteAttributes* GetOpt(RteOptType type) const;
  bool HasOptions() const;
  // pointer variant to access directly from Component properties dialog
  RteAttributes* GetOpt(RteOptType type);

  XMLTreeElement* CreateXmlTreeElement(XMLTreeElement* parentElement, bool bCreateContent = true) const;
  // process XML child elements
  bool ProcessXmlChildren(XMLTreeElement* xmlElement);

protected:
  virtual void ProcessAttributes() override;

private:
  bool m_bExcluded;
  bool m_bIncludeInLib;
  int m_instanceCount;
  VersionCmp::MatchMode m_VersionMatchMode;
  RteAttributes m_memOpt;
  RteAttributes m_cOpt;
  RteAttributes m_asmOpt;
};

typedef std::map<std::string, RteInstanceTargetInfo*> RteInstanceTargetInfoMap;

class RteItemInstance : public RteItem
{
public:
  RteItemInstance(RteItem* parent);
  virtual ~RteItemInstance() override;

  virtual void InitInstance(RteItem* item);

  // uVision targets
  bool IsUsedByTarget(const std::string& targetName) const;
  bool IsFilteredByTarget(const std::string& targetName) const;
  const RteInstanceTargetInfoMap& GetTargetInfos() const { return m_targetInfos; }
  void SetTargets(const RteInstanceTargetInfoMap& infos); // makes deep copy
  void ClearTargets();
  void PurgeTargets();
  int GetTargetCount() const { return (int)m_targetInfos.size(); }
  int GetInstanceCount(const std::string& targetName) const;
  const std::string& GetFirstTargetName() const;

  virtual bool IsPackageInfo() const { return false; }

  bool SetUseLatestVersion(bool bUseLatest, const std::string& targetName);
  bool SetExcluded(bool excluded, const std::string& targetName);
  bool IsExcluded(const std::string& targetName) const;

  bool SetIncludeInLib(bool include, const std::string& targetName);
  bool IsIncludeInLib(const std::string& targetName) const;

  void CopyTargetSettings(const RteInstanceTargetInfo& other, const std::string& targetName);

  virtual bool IsExcludedForAllTargets(); // check if all targets have excluded flag


  virtual bool IsRemoved() const { return m_bRemoved; }
  virtual void SetRemoved(bool removed) { m_bRemoved = removed; }

  VersionCmp::MatchMode GetVersionMatchMode(const std::string& targetName) const;
  // next three functions return true if m_target is changes
  virtual RteInstanceTargetInfo* AddTargetInfo(const std::string& targetName);
  virtual RteInstanceTargetInfo* AddTargetInfo(const std::string& targetName, const std::string& copyFrom);
  virtual RteInstanceTargetInfo* AddTargetInfo(const std::string& targetName, const std::map<std::string, std::string>& attributes);
  virtual bool RemoveTargetInfo(const std::string& targetName, bool bDelete = true);
  virtual bool RenameTargetInfo(const std::string& oldName, const std::string& newName);

  virtual RteInstanceTargetInfo* GetTargetInfo(const std::string& targetName) const;
  virtual RteInstanceTargetInfo* EnsureTargetInfo(const std::string& targetName);

  // RteItem overrides
public:
  virtual void Clear() override;

  virtual const RteAttributes& GetPackageAttributes() const { return m_packageAttributes; }
  virtual bool SetPackageAttributes(const RteAttributes& attr) { return m_packageAttributes.SetAttributes(attr); }
  virtual RtePackage* GetPackage() const override; // returns original RtePackage
  virtual RteComponent* GetComponent() const override { return nullptr;}
  virtual RteComponent* GetComponent(const std::string& targetName) const; // returns original RteComponent from global collection
  virtual RteComponent* GetResolvedComponent(const std::string& targetName) const; // returns resolved RteComponent for given target
  virtual std::string GetPackageID(bool withVersion) const override;
  virtual const std::string& GetURL() const override;
  virtual const std::string& GetVendorString() const override;
  virtual const std::string& GetPackageVendorName() const;
  virtual RteTarget* GetTarget(const std::string& targetName) const;


public:
  virtual RteComponentInstance* GetComponentInstance(const std::string& targetName) const;
  virtual RtePackage* GetEffectivePackage(const std::string& targetName) const;
  virtual std::string GetEffectivePackageID(const std::string& targetName) const;

protected:
  virtual bool HasXmlContent() const override { return true; }
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement) override;
  virtual void CreateXmlTreeElementContent(XMLTreeElement* parentElement) const override;

  // uVision project related members
protected:
  RteAttributes m_packageAttributes;
  RteInstanceTargetInfoMap m_targetInfos;
  bool m_bRemoved;
};

class RtePackageInstanceInfo : public RteItemInstance
{
public:
  RtePackageInstanceInfo(RteItem* parent) : RteItemInstance(parent) {};

  virtual RtePackage* GetEffectivePackage(const std::string& targetName) const override;
  virtual std::string ConstructID() override;
  virtual std::string GetPackageID(bool withVersion) const override;
  virtual const std::string& GetCommonID() const { return m_commonID; }
  virtual const RteAttributes& GetPackageAttributes() const override { return *this; }
  virtual bool SetPackageAttributes(const RteAttributes& attr) override { return SetAttributes(attr); }
  virtual bool IsPackageInfo() const override { return true; }
  virtual const std::string& GetURL() const override { return GetAttribute("url"); }

  RtePackage* GetResolvedPack(const std::string& targetName) const; // returns resolved RtePackage for given target
  void SetResolvedPack(RtePackage* pack, const std::string& targetName);
  bool ResolvePack();
  bool ResolvePack(const std::string& targetName);
  void ClearResolved();

protected:
  virtual void ProcessAttributes() override;


protected:
  std::map<std::string, RtePackage*> m_resolvedPacks;
  std::string m_commonID;
};

class RteGpdscInfo : public RteItemInstance
{
public:
  RteGpdscInfo(RteItem* parent, RteGeneratorModel* model = 0);
  ~RteGpdscInfo() override;
  std::string GetAbsolutePath() const;

  virtual RteGeneratorModel* GetGeneratorModel() const { return m_model; }
  void SetGeneratorModel(RteGeneratorModel* model) { m_model = model; }

  virtual const RteAttributes& GetPackageAttributes() const override { return *this; }
  virtual bool SetPackageAttributes(const RteAttributes& attr) override { return SetAttributes(attr); }
  virtual bool IsPackageInfo() const override { return true; } // still it is a package info


protected:
  RteGeneratorModel* m_model; // model produced by gpdsc pack
};

// board instance describes board assigned to a target
class RteBoard;
class RteBoardInfo : public RteItemInstance
{
public:
  RteBoardInfo(RteItem* parent);

  virtual ~RteBoardInfo() override;
  virtual void Clear() override;
  void ClearResolved();

  RteBoard* GetBoard() const { return m_board; }

  void Init(RteBoard* board);

public:
  virtual void InitInstance(RteItem* item) override;
  virtual std::string ConstructID() override;
  virtual std::string GetDisplayName() const override;
  virtual const std::string& GetName() const override { return GetAttribute("Bname"); }
  virtual const std::string& GetVersionString() const override { return GetAttribute("Bversion"); }
  virtual const std::string& GetVendorString() const override { return GetAttribute("Bvendor"); }
  void ResolveBoard();
  RteBoard* ResolveBoard(const std::string& targetName); // resolves component and returns it

  ConditionResult GetResolveResult(const std::string& targetName) const;
  virtual RtePackage* GetPackage() const override; // returns original RtePackage
  virtual std::string GetPackageID(bool withVersion) const override;

protected:
  RteBoard* m_board; // in contrast to other instance classes, it can only be one resolution
};



// component instance only describes the component, not it files
class RteComponentInstance : public RteItemInstance
{
public:
  RteComponentInstance(RteItem* parent);

  virtual ~RteComponentInstance() override;
  virtual void Clear() override;

  void Init(RteComponent* c);

  bool IsVersionMatchFixed() const;
  bool IsVersionMatchLatest() const;

  const RteComponentMap& GetResolvedComponents() const { return m_resolvedComponents; }

  bool Equals(RteComponentInstance* ci) const;
  bool IsModified() const;
  bool HasCopy() const { return m_copy != NULL; }
  RteComponentInstance* GetCopy() const { return m_copy; }
  RteComponentInstance* MakeCopy();
  RteComponentInstance* GetCopyInstance() const { return m_copy; }
  RteComponentInstance* GetApiInstance() const;

public:
  virtual bool Construct(XMLTreeElement* xmlElement) override;
  virtual std::string GetFullDisplayName() const;
  virtual std::string GetAggregateDisplayName() const;
  virtual std::string GetDisplayName() const override;
  virtual std::string GetShortDisplayName() const;
  virtual const std::string& GetVersionString() const override;

  virtual std::string ConstructID() override;
  virtual std::string GetComponentUniqueID(bool withVersion) const override;
  bool HasAggregateID(const std::string& aggregateId) const;

  virtual const std::string& GetVendorString() const override;
  virtual bool IsRemoved() const override;
  virtual void SetRemoved(bool removed) override;
  virtual std::string GetDocFile() const override; // returns absolute path to doc file if any (for active target)

  bool IsTargetSpecific() const;
  bool SetTargetSpecific(bool bSet);
  bool SetVariant(const std::string& variant);
  bool SetVersion(const std::string& version);

  RteItem* GetEffectiveItem(const std::string& targetName) const; // returns resolved component or this
  virtual RtePackage* GetEffectivePackage(const std::string& targetName) const override;
  virtual std::string GetEffectiveDisplayName(const std::string& targetName) const;


  virtual RteComponent* GetResolvedComponent(const std::string& targetName) const override; // returns resolved RteComponent for given target
  void SetResolvedComponent(RteComponent*c, const std::string& targetName);

  RteComponent* GetPotentialComponent(const std::string& targetName) const; // returns potentially resolvable RteComponent for given target
  void SetPotentialComponent(RteComponent*c, const std::string& targetName);
  void ResolveComponent();
  void ClearResolved();

  ConditionResult GetResolveResult(const std::string& targetName) const;
  RteComponent* ResolveComponent(const std::string& targetName); // resolves component and returns it


protected:
  RteComponentMap m_resolvedComponents;
  RteComponentMap m_potentialComponents; // potentially available components (packs are not selected)
  RteComponentInstance* m_copy;
};

// class that accumulates component instances based on their aggregate ID
class RteComponentInstanceAggregate : public RteItem
{
public:
  RteComponentInstanceAggregate(RteItem* parent = NULL);
  virtual ~RteComponentInstanceAggregate() override;

public:
  virtual void Clear() override;
  virtual std::string GetDisplayName() const override;
  virtual std::string GetFullDisplayName() const { return m_fullDisplayName; }

public:
  bool HasAggregateID(const std::string& aggregateId) const;
  bool HasComponentInstance(RteComponentInstance* ci) const;
  RteComponentInstance* GetComponentInstance(const std::string& targetName) const;
  RteComponentAggregate* GetComponentAggregate(const std::string& targetName) const;

  bool IsUnresolved(const std::string& targetName, bool bCopy = false) const;
  bool IsFilteredByTarget(const std::string& targetName) const; // supported if any of children supports the target
  bool IsUsedByTarget(const std::string& targetName) const; // supported if any of children is used by
  bool IsExcluded(const std::string& targetName) const; // is explicitly excluded
  bool IsTargetSpecific() const; // every (==any) member is target specific
  bool AllowsCommonSettings() const;// only true all members can support all targets
  void AddComponentInstance(RteComponentInstance* ci);

  bool IsModified() const;
  RteComponentInstance* GetModifiedInstance() const; // can be only one

  bool HasMaxInstances() const override {return m_bHasMaxInstances; }
private:
  std::string m_fullDisplayName;
  bool m_bHasMaxInstances;
};


// class-group-subgroup tree - parallel hierarchy
// category item contains class or group or subgroup name in m_tag member and component racks in m_children
class RteComponentInstanceGroup : public RteItem
{
public:
  RteComponentInstanceGroup(RteItem* parent);
  ~RteComponentInstanceGroup() override;

  RteComponentInstance* GetApiInstance() const { return m_apiInstance; }

  bool HasSingleAggregate() const;
  bool HasUnresolvedComponents(const std::string& targetName, bool bCopy = false) const;
  bool IsUsedByTarget(const std::string& targetName) const;

  RteComponentInstanceAggregate* GetSingleComponentInstanceAggregate() const; // returns a single aggregate if any
  RteComponentInstanceAggregate* GetComponentInstanceAggregate(const std::string& aggregateId) const;
  RteComponentInstanceAggregate* GetComponentInstanceAggregate(RteComponentInstance* ci) const;
  RteComponentInstanceGroup* GetComponentInstanceGroup(RteComponentInstance* ci) const;

  const std::map<std::string, RteComponentInstanceGroup*>& GetGroups() const { return m_groups; }
  RteComponentInstanceGroup* GetGroup(const std::string& name) const;
  RteComponentInstanceGroup* EnsureGroup(const std::string& name); // add subgroup if does not exist
  void AddComponentInstance(RteComponentInstance* ci);

  void GetInstanceAggregates(std::set<RteComponentInstanceAggregate*>& aggregates) const;
  void GetModifiedInstanceAggregates(std::set<RteComponentInstanceAggregate*>& modifiedAggregates) const;

public:
  virtual void Clear() override;
  virtual std::string GetDisplayName() const override;

private:
  std::map<std::string, RteComponentInstanceGroup*> m_groups;
  RteComponentInstance* m_apiInstance;
};


class RteFileInstance : public RteItemInstance
{
public:
  RteFileInstance(RteItem* parent);

public:
  void Init(RteFile* f, const std::string& deviceName, int instanceIndex);
  void Update(RteFile* f, bool bUpdateComponent);

  bool IsConfig() const;
  int HasNewVersion(const std::string& targetName) const; // has new version for given target
  int HasNewVersion() const; // has new version for any target

  RteFile::Category GetCategory() const;

  int GetInstanceIndex() const { return m_instanceIndex; }
  const std::string& GetInstanceName() const { return m_instanceName; }
  const std::string& GetOriginalFileName() const { return GetName(); }

  virtual std::string GetDisplayName() const override;
  virtual std::string GetFileComment() const; // used in project view
  virtual std::string GetHeaderComment() const; // used in editor menu

  const std::string& GetFileName() const { return m_fileName; } // returns filename without path
  std::string GetAbsolutePath() const;
  std::string GetIncludePath() const;
  std::string GetIncludeFileName() const;


  const std::string& GetComponentVersionString() const { return m_componentAttributes.GetVersionString(); }

  RteFile* GetFile(const std::string& targetName) const;
  bool Copy(RteFile* f, bool bMerge);

public:
  std::string Backup(bool bDeleteExisting);
  virtual const std::string& GetID() const override { return m_instanceName; } // TODO: reuse m_ID
  virtual RteComponentInstance* GetComponentInstance(const std::string& targetName) const override;
  virtual std::string GetComponentID(bool withVersion) const override; // to use in filtered list
  virtual std::string GetComponentUniqueID(bool withVersion) const override;
  virtual std::string GetComponentAggregateID() const override;
  virtual const std::string& GetVersionString() const override;
  virtual const std::string& GetVendorString() const override;
  virtual const std::string& GetCbundleName() const override;
  virtual std::string GetProjectGroupName() const override;

  virtual bool Construct(XMLTreeElement* xmlElement) override;

protected:
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement) override;
  virtual void CreateXmlTreeElementContent(XMLTreeElement* parentElement) const override;

protected:
  int m_instanceIndex; // zero-base instance index for multi-instance components, -1 otherwise
  std::string m_instanceName; // file name relative to project directory
  std::string m_fileName; // file name without path
  RteAttributes m_componentAttributes;
};

#endif // RteInstance_H
