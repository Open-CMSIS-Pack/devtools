#ifndef RteInstance_H
#define RteInstance_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteInstance.h
* @brief CMSIS RTE data model
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
class RteProject;
class RteFileInstance;
class RteTarget;
class RteProject;

/**
 * @brief class to store settings per project target for an owning item: component, file, pack
*/
class RteInstanceTargetInfo : public RteItem
{
public:

  /**
   * @brief enumerator for option type
  */
  enum RteOptType {
    MEMOPT,  // memory option
    COPT,    // C/C++ compiler option
    ASMOPT,  // assembler option
    OPTCOUNT // number of option types for internal use in loops
  };

  /**
   * @brief default constructor
  */
  RteInstanceTargetInfo(RteItem* parent);
  /**
   * @brief copy constructor
   * @param info pointer to RteInstanceTargetInfo to copy from
  */
  RteInstanceTargetInfo(RteInstanceTargetInfo* info);

  /**
   * @brief constructor from attributes map
   * @param attributes std::map with name-value pairs
  */
  RteInstanceTargetInfo(const std::map<std::string, std::string>& attributes);

  /**
   * @brief check if item is excluded from target
   * @return true if excluded
  */
  bool IsExcluded() const { return m_bExcluded; }

  /**
   * @brief set item to be excluded from target
   * @param excluded flag to exclude
   * @return true if the setting has changed
  */
  bool SetExcluded(bool excluded);

  /**
   * @brief check if the owning item is included in library build
   * @return true if included in library build
  */
  bool IsIncludeInLib() const { return m_bIncludeInLib; }

  /**
   * @brief set to include the owning item in library build
   * @param include flag to include in library build
   * @return true if the setting has changed
  */
  bool SetIncludeInLib(bool include);

  /**
   * @brief get component instance count
   * @return number of component instances
  */
  int GetInstanceCount() const { return m_instanceCount; }

  /**
   * @brief set component instance count
   * @param count component instance count to set
  */
  void SetInstanceCount(int count);

  /**
   * @brief get version matching mode
   * @return VersionCmp::MatchMode value
  */
  VersionCmp::MatchMode GetVersionMatchMode() const { return m_VersionMatchMode; }

  /**
   * @brief set version matching mode
   * @param mode VersionCmp::MatchMode value
   * @return true if the setting has changed
  */
  bool SetVersionMatchMode(VersionCmp::MatchMode mode);

  /**
   * @brief copy settings from another RteInstanceTargetInfo
   * @param other reference to RteInstanceTargetInfo to copy settings from
  */
  void CopySettings(const RteInstanceTargetInfo& other);

  /**
   * @brief get memory options
   * @return memory options as a reference to RteItem
  */
  const RteItem& GetMemOpt() const { return m_memOpt; }

  /**
   * @brief get C/C++ compiler options
   * @return C/C++ compiler options as a reference to RteItem
  */
  const RteItem& GetCOpt() const { return m_cOpt; }
  /**
   * @brief get assembler options
   * @return assembler options as a reference to RteItem
  */
  const RteItem& GetAsmOpt() const { return m_asmOpt; }

  /**
   * @brief get options of specified type (immutable)
   * @param type options type as RteOptType value
   * @return pointer to RteItem containing options, nullptr if no such options are supported
  */
  const RteItem* GetOpt(RteOptType type) const;

  /**
   * @brief get options of specified type (mutable)
   * @param type options type as RteOptType value
   * @return pointer to RteItem containing options, nullptr if no such options are supported
  */
  RteItem* GetOpt(RteOptType type);

  /**
   * @brief check if the instance contains specific options for compiler, assembler or memory
   * @return true if has specific options
  */
  bool HasOptions() const;

  /**
   * @brief create XMLTreeElement object to export this item to XML
   * @param parentElement parent for created XMLTreeElement
   * @param bCreateContent true to create XML content out of children
   * @return created XMLTreeElement
  */
  XMLTreeElement* CreateXmlTreeElement(XMLTreeElement* parentElement, bool bCreateContent = true) const override;

  /**
   * @brief called to construct the item with attributes and child elements
  */
  void Construct() override;

protected:
  /**
   * @brief perform changes in internal data after calls to SetAttributes(), AddAttributes() and ClearAttributes()
  */
   void ProcessAttributes() override;

private:
  bool m_bExcluded;
  bool m_bIncludeInLib;
  int m_instanceCount;
  VersionCmp::MatchMode m_VersionMatchMode;
  RteItem m_memOpt;
  RteItem m_cOpt;
  RteItem m_asmOpt;
};

typedef std::map<std::string, RteInstanceTargetInfo*> RteInstanceTargetInfoMap;

/**
 * @brief class describing an instantiated RteItem (component, file, used pack)
*/
class RteItemInstance : public RteItem
{
public:

  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteItemInstance(RteItem* parent);

  /**
   * @brief virtual destructor
  */
   ~RteItemInstance() override;

  /**
   * @brief initializes this object from supplied RteItem
   * @param item pointer to RteItem to initialize from
  */
  virtual void InitInstance(RteItem* item);

  /**
   * @brief check if this item instance is used by specified target
   * @param targetName target name
   * @return true if used (not excluded)
  */
  bool IsUsedByTarget(const std::string& targetName) const;

  /**
   * @brief check if this item is filtered by specified target
   * @param targetName target name
   * @return true if the instance has RteInstanceTargetInfo for specified target name
  */
  bool IsFilteredByTarget(const std::string& targetName) const;

  /**
   * @brief get collection of all target infos
   * @return map of target name to RteInstanceTargetInfo pairs
  */
  const RteInstanceTargetInfoMap& GetTargetInfos() const { return m_targetInfos; }

  /**
   * @brief set collection of target infos
   * @param infos map of target name to RteInstanceTargetInfo pairs
  */
  void SetTargets(const RteInstanceTargetInfoMap& infos);

  /**
   * @brief remove all target infos
  */
  void ClearTargets();

  /**
   * @brief remove unused target infos (for those with GetInstanceCount(targetName) ==  0)
  */
  void PurgeTargets();

  /**
   * @brief get number of target infos
   * @return number of stored target infos
  */
  int GetTargetCount() const { return (int)m_targetInfos.size(); }

  /**
   * @brief get instance count for specified target
   * @param targetName target name
   * @return instance count
  */
  int GetInstanceCount(const std::string& targetName) const;

  /**
   * @brief get first target name in the collection.
   * Method is intended for environments where only one target is supported by a project
   * @return first target name in the internal collection
  */
  const std::string& GetFirstTargetName() const;

  /**
   * @brief check if this object contains pack attributes directly rather than in a dedicated child
   * @return true if this item is a pack info, default is false
  */
  virtual bool IsPackageInfo() const { return false; }

  /**
   * @brief set to use the latest available version when resolving a component or a pack
   * @param bUseLatest flag to use latest version
   * @param targetName target name
   * @return true if the setting has changed
  */
  bool SetUseLatestVersion(bool bUseLatest, const std::string& targetName);

  /**
   * @brief set this item to be explicitly excluded from specified target
   * @param excluded flag to exclude
   * @param targetName target name
   * @return true if the setting has changed
  */
  bool SetExcluded(bool excluded, const std::string& targetName);

  /**
   * @brief check if this item is explicitly excluded from specified target
   * @param targetName target name
   * @return true if excluded
  */
  bool IsExcluded(const std::string& targetName) const;

  /**
   * @brief set to include this item in library build for specified target
   * @param include flag to include to library build
   * @param targetName target name
   * @return true if the setting has changed
  */
  bool SetIncludeInLib(bool include, const std::string& targetName);

  /**
   * @brief check if the instance is included into library build for the specified target
   * @param targetName target name
   * @return true if included into library build
  */
  bool IsIncludeInLib(const std::string& targetName) const;

  /**
   * @brief copy target info settings from another target
   * @param other reference to RteInstanceTargetInfo to copy from
   * @param targetName destination target name
  */
  void CopyTargetSettings(const RteInstanceTargetInfo& other, const std::string& targetName);

  /**
   * @brief check if the item is excluded from all targets
   * @return true if all targets have excluded flag
  */
  virtual bool IsExcludedForAllTargets();

  /**
   * @brief check if item is removed, e.g. a config file
   * @return true if removed
  */
  virtual bool IsRemoved() const { return m_bRemoved; }

  /**
   * @brief set/reset removed state
   * @param removed flag to set/reset removed state
  */
  virtual void SetRemoved(bool removed) { m_bRemoved = removed; }

  /**
   * @brief get version matching mode for specified target
   * @param targetName target name
   * @return VersionCmp::MatchMode value
  */
  VersionCmp::MatchMode GetVersionMatchMode(const std::string& targetName) const;

  /**
   * @brief create or reuse existing target info for specified target
   * @param targetName target name
   * @return pointer to RteInstanceTargetInfo, never nullptr
  */
  virtual RteInstanceTargetInfo* AddTargetInfo(const std::string& targetName);

  /**
   * @brief create or reuse existing target info for specified target, copy settings from another target info
   * @param targetName target name
   * @param copyFrom target name to copy from
   * @return pointer to RteInstanceTargetInfo, never nullptr
  */
  virtual RteInstanceTargetInfo* AddTargetInfo(const std::string& targetName, const std::string& copyFrom);

  /**
   * @brief create or reuse existing target info for specified target, initialize it from supplied attributes
   * @param targetName target name
   * @param attributes map with name-value pairs to initialize the target info
   * @return pointer to RteInstanceTargetInfo, never nullptr
  */
  virtual RteInstanceTargetInfo* AddTargetInfo(const std::string& targetName, const std::map<std::string, std::string>& attributes);

  /**
   * @brief remove target info for specified target and delete it if requested
   * @param targetName target name
   * @param bDelete flag to request deletion
   * @return true if the info has been removed
  */
  virtual bool RemoveTargetInfo(const std::string& targetName, bool bDelete = true);

  /**
   * @brief rename target info for specified target (to reflect target rename)
   * @param targetName target name
   * @param newName new target name
   * @return true if the info has been renamed
  */
  virtual bool RenameTargetInfo(const std::string& oldName, const std::string& newName);

  /**
   * @brief return target info for specified target
   * @param targetName target name
   * @return pointer to RteInstanceTargetInfo, nullptr if does not exist
  */
  virtual RteInstanceTargetInfo* GetTargetInfo(const std::string& targetName) const;

  /**
   * @brief return target info for specified target, create new one if does not exist
   * @param targetName target name
   * @return pointer to RteInstanceTargetInfo, never nullptr
  */
  virtual RteInstanceTargetInfo* EnsureTargetInfo(const std::string& targetName);

public:
  /**
   * @brief clear internal data
  */
  void Clear() override;

  /**
   * @brief called to construct the item with attributes and child elements
  */
  void Construct() override;

  /**
   * @brief append a new child at the end of the list
   * @param child pointer to a new child of template type RteItem
   * @return pointer to the new child
  */
  RteItem* AddChild(RteItem* child) override;

  /**
   * @brief create a new instance of type RteItem
   * @param tag name of tag
   * @return pointer to instance of type RteItem
  */
  RteItem* CreateItem(const std::string& tag) override;

  /**
   * @brief get pack attributes
   * @return pack attributes as RteItem reference
  */
  virtual const RteItem& GetPackageAttributes() const { return m_packageAttributes; }
  /**
   * @brief set pack attributes
   * @param attr attributes to set as XmlItem reference
   * @return true if changed
  */
  virtual bool SetPackageAttributes(const XmlItem& attr) { return m_packageAttributes.SetAttributes(attr); }

  /**
   * @brief get pointer to resolved RtePackage
   * @return pointer to RtePackage if resolved, nullptr otherwise
  */
   RtePackage* GetPackage() const override;

  /**
   * @brief get pointer to RteComponent
   * @return pointer to RteComponent, nullptr by default
  */
   RteComponent* GetComponent() const override { return nullptr;}

  /**
   * @brief get resolved component, even if this item is not filtered by specified target
   * @param targetName target name
   * @return pointer to resolved RteComponent, nullptr if not resolved
  */
  virtual RteComponent* GetComponent(const std::string& targetName) const;

  /**
   * @brief get resolved component, only if this item is filtered by specified target
   * @param targetName target name
   * @return pointer to resolved RteComponent, nullptr if not resolved
  */
  virtual RteComponent* GetResolvedComponent(const std::string& targetName) const;

  /**
   * @brief get originating pack ID
   * @param withVersion flag to get full (true) or common (false) pack ID
   * @return full or common pack ID constructed from underlying pack info
  */
   std::string GetPackageID(bool withVersion) const override;

  /**
   * @brief get originating pack URL
   * @return "url" attribute value from underlying pack info
  */
   const std::string& GetURL() const override;

  /**
   * @brief get pack vendor
   * @return pack vendor from underlying pack info
  */
   const std::string& GetVendorString() const override;

  /**
   * @brief get pack vendor
   * @return pack vendor from underlying pack info
  */
  virtual const std::string& GetPackageVendorName() const;

  /**
   * @brief get pointer to specified RteTarget
   * @param targetName target name
   * @return pointer to RteTarget if found, nullptr otherwise
  */
  virtual RteTarget* GetTarget(const std::string& targetName) const;

public:

  /**
   * @brief get pointer to RteComponentInstance for specified target
   * @param targetName target name
   * @return pointer to RteComponentInstance if item is found and used in the target, nullptr otherwise
  */
  virtual RteComponentInstance* GetComponentInstance(const std::string& targetName) const;

  /**
   * @brief get effectively used resolved pack for specified target
   * @param targetName target name
   * @return pointer to RtePackage if found, nullptr otherwise
  */
  virtual RtePackage* GetEffectivePackage(const std::string& targetName) const;

  /**
   * @brief get effectively used pack ID for specified target (is available even if pack is not resolved)
   * @param targetName target name
   * @return pack ID string
  */
  virtual std::string GetEffectivePackageID(const std::string& targetName) const;

protected:

  /**
   * @brief check if this item provides content to be stored in XML format
   * @return true
  */
   bool HasXmlContent() const override { return true; }

  /**
   * @brief creates child element for supplied XMLTreeElement
   * @param parentElement XMLTreeElement to generate content for
  */
   void CreateXmlTreeElementContent(XMLTreeElement* parentElement) const override;

protected:
  RteItem m_packageAttributes;
  RteInstanceTargetInfoMap m_targetInfos;
  bool m_bRemoved;
};

/**
 * @brief info about pack used in the project
*/
class RtePackageInstanceInfo : public RteItemInstance
{
public:

  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RtePackageInstanceInfo(RteItem* parent) : RteItemInstance(parent) {};

  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
   * @param packId full pack ID
  */
  RtePackageInstanceInfo(RteItem* parent, const std::string& packId);

  /**
   * @brief set pack ID and set corresponding attributes
   * @param packId full pack ID
  */
  void SetPackId(const std::string& packId);

  /**
   * @brief get resolved pack for specified target
   * @param targetName target name
   * @return pointer to RtePackage if resolved, nullptr otherwise
  */
   RtePackage* GetEffectivePackage(const std::string& targetName) const override;

  /**
   * @brief construct package ID out of attributes
   * @return constructed pack ID
  */
   std::string ConstructID() override;

  /**
   * @brief get pack ID
   * @param withVersion flag to include pack version to the ID
   * @return full or common pack ID
  */
   std::string GetPackageID(bool withVersion) const override;

  /**
   * @brief get common (family) pack ID
   * @return pack ID without version
  */
  virtual const std::string& GetCommonID() const { return m_commonID; }

  /**
   * @brief get pack attributes
   * @return reference to this
  */
   const RteItem& GetPackageAttributes() const override { return *this; }

  /**
   * @brief set pack attributes
   * @param attr pack attributes to set
   * @return true if attribute values have changed
  */
   bool SetPackageAttributes(const XmlItem& attr) override { return SetAttributes(attr); }

  /**
   * @brief check if this object contains pack attributes directly rather than in a dedicated child
   * @return true
  */
   bool IsPackageInfo() const override { return true; }

  /**
   * @brief get pack URL
   * @return value of "url" attribute
  */
   const std::string& GetURL() const override { return GetAttribute("url"); }

  /**
    * @brief get resolved pack for specified target
    * @param targetName target name
    * @return pointer to RtePackage if resolved, nullptr otherwise
  */
  RtePackage* GetResolvedPack(const std::string& targetName) const;

  /**
   * @brief set resolved back for specified target
   * @param pack pointer to resolved RtePackage
   * @param targetName target name
  */
  void SetResolvedPack(RtePackage* pack, const std::string& targetName);

  /**
   * @brief resolve packs for all targets
   * @return true if resolved for all targets
  */
  bool ResolvePack();

  /**
   * @brief resolve pack for specified target
   * @param targetName target name
   * @return true if resolved
  */
  bool ResolvePack(const std::string& targetName);

  /**
   * @brief clear pointers to resolved packs for all targets
  */
  void ClearResolved();

protected:
   void ProcessAttributes() override;

protected:
  std::map<std::string, RtePackage*> m_resolvedPacks;
  std::string m_commonID;
};

/**
 * @brief class containing information about *.gpdsc file used in project
*/
class RteGpdscInfo : public RteItemInstance
{
public:

  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
   * @param gpdscPack pointer to RtePackage representing gpdsc if available
  */
  RteGpdscInfo(RteItem* parent, RtePackage* gpdscPack = nullptr);

  /**
   * @brief virtual destructor
  */
  ~RteGpdscInfo() override;

  /**
   * @brief get absolute path to *.gpdsc
   * @return absolute path to *.gpdsc file including filename
  */
  std::string GetAbsolutePath() const;

  /**
   * @brief return associated generator if any
   * @return pointer to RteGenerator
  */
  RteGenerator* GetGenerator() const { return m_generator; }

  /**
   * @brief set associated generator if any
   * @param pointer to RteGenerator
  */
  void SetGenerator(RteGenerator* generator) { m_generator = generator; }

  /**
   * @brief get files to add to project when using the generator
   * @return pointer to RteFileContainer containing file items, nullptr if no files need to be added
  */
  RteFileContainer* GetProjectFiles() const;

  /**
   * @brief get associated generator model
   * @return pointer to generator RtePackage
  */
  RtePackage* GetGpdscPack() const { return m_gpdscPack; }

  /**
   * @brief set associated generator model
   * @param gpdscPack pointer to gpdsc RtePackage
  */
  void SetGpdscPack(RtePackage* gpdscPack);

  /**
   * @brief get pack attributes
   * @return reference to this
  */
   const RteItem& GetPackageAttributes() const override { return *this; }

  /**
   * @brief set pack attributes
   * @param attr pack attributes
   * @return true if changed
  */
   bool SetPackageAttributes(const XmlItem& attr) override { return SetAttributes(attr); }

  /**
   * @brief check if this object contains pack attributes directly rather than in a dedicated child
   * @return true
  */
   bool IsPackageInfo() const override { return true; }


protected:
  RtePackage* m_gpdscPack; // generator pack
  RteGenerator* m_generator; // generator item
};

class RteBoard;
/**
 * @brief info about board assigned to a target
*/
class RteBoardInfo : public RteItemInstance
{
public:

  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteBoardInfo(RteItem* parent);

  /**
   * @brief virtual destructor
  */
   ~RteBoardInfo() override;

  /**
   * @brief clear internal data
  */
   void Clear() override;

  /**
   * @brief clear pointer to resolved board
  */
  void ClearResolved();

  /**
   * @brief get actual board description
   * @return pointer to RteBoard
  */
  RteBoard* GetBoard() const { return m_board; }

  /**
   * @brief initialize info
   * @param board pointer to RteBoard
  */
  void Init(RteBoard* board);

public:
  /**
   * @brief initialize info
   * @param item pointer to RteItem to initialize from
  */
   void InitInstance(RteItem* item) override;

  /**
   * @brief construct board ID
   * @return constructed board ID by calling GetDisplayName()
  */
   std::string ConstructID() override;

  /**
   * @brief get board display name
   * @return string constructed from attribute values in the format "Bname (Bversion)"
  */
   std::string GetDisplayName() const override;

  /**
   * @brief get board name
   * @return "Bname" attribute value
  */
   const std::string& GetName() const override { return GetAttribute("Bname"); }

  /**
   * @brief get board version from its revision
   * @return board revision
  */
   const std::string& GetVersionString() const override { return GetRevision(); }

  /**
   * @brief get board revision
   * @return revision string
  */
  const std::string& GetRevision() const;

  /**
   * @brief get board vendor string
   * @return vendor string
  */
   const std::string& GetVendorString() const override { return GetAttribute("Bvendor"); }


  /**
   * @brief resolve board for all targets
  */
  void ResolveBoard();

  /**
   * @brief resolve board for a specified target
   * @param targetName target name
   * @return pointer to RteBoard
  */
  RteBoard* ResolveBoard(const std::string& targetName);

  /**
   * @brief get cached result of resolving board
   * @param targetName target name
   * @return RteItem::ConditionResult value
  */
  ConditionResult GetResolveResult(const std::string& targetName) const;

  /**
   * @brief get the original board's pack
   * @return pointer to RtePackage if resolved, nullptr otherwise
  */
   RtePackage* GetPackage() const override;

  /**
   * @brief get the original board's pack ID
   * @param withVersion flag to return full (true) or common (false) pack ID
   * @return full or common pack ID
  */
   std::string GetPackageID(bool withVersion) const override;

protected:
  RteBoard* m_board; // in contrast to other instance classes, it can only be one resolution
};


/**
 * @brief info about a component or API instantiated in a project
*/
class RteComponentInstance : public RteItemInstance
{
public:

  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteComponentInstance(RteItem* parent);

  /**
   * @brief virtual destructor
  */
   ~RteComponentInstance() override;


  /**
   * @brief clear internal data
  */
   void Clear() override;

  /**
   * @brief initialize info from component
   * @param c pointer to RteComponent
  */
  void Init(RteComponent* c);

  /**
   * @brief check if component version matching mode is "fixed"
   * @return true if component version matching mode is "fixed"
  */
  bool IsVersionMatchFixed() const;

  /**
   * @brief check if component version matching mode is "latest"
   * @return true if component version matching mode is "latest"
  */
  bool IsVersionMatchLatest() const;

  /**
   * @brief get collection of resolved components per target
   * @return map of target name to RteComponent pointer pairs
  */
  const RteComponentMap& GetResolvedComponents() const { return m_resolvedComponents; }

  /**
   * @brief check if this component instance equals to another one
   * @param ci pointer to RteComponentInstance to compare with
   * @return true if equal
  */
  bool Equals(RteComponentInstance* ci) const;

  /**
   * @brief check if this component instance is modified
   * @return true if the instance has an internal copy that is not equal to this
  */
  bool IsModified() const;
  /**
   * @brief check if the instance has a copy
   * @return true if im_copy member is not a nullptr
  */
  bool HasCopy() const { return m_copy != nullptr; }

  /**
   * @brief get internal copy
   * @return pointer to RteComponentInstance (m_copy member)
  */
  RteComponentInstance* GetCopy() const { return m_copy; }
  /**
   * @brief get internal copy
   * @return pointer to RteComponentInstance (m_copy member)
  */
  RteComponentInstance* GetCopyInstance() const { return m_copy; }

  /**
   * @brief make a copy of this member and assign to m_copy member, delete previous if exists
   * @return pointer to created RteComponentInstance (m_copy member)
  */
  RteComponentInstance* MakeCopy();

  /**
   * @brief get API instance associated with this component instance
   * @return pointer to RteComponentInstance representing API associated with this component instance
  */
  RteComponentInstance* GetApiInstance() const;

public:

  /**
   * @brief get full component name to display
   * @return string in the format Cvendor.Cbundle::Cclass:Cgroup[:Csub]:Cvariant:Cversion
  */
  virtual std::string GetFullDisplayName() const;

 /**
   * @brief get component name to display
   * @return string in the format Cvendor.Cbundle::Cclass:Cgroup[:Csub]
  */
   std::string GetDisplayName() const override;
  /**
   * @brief get short component name to display
   * @return string in the format Cgroup[:Csub]
  */
  virtual std::string GetShortDisplayName() const;

  /**
   * @brief get component instance version string (can differ from resolved component)
   * @return version string
  */
   const std::string& GetVersionString() const override;

  /**
   * @brief construct component ID
   * @return constructed ID string
  */
   std::string ConstructID() override;

  /**
   * @brief check if this component instance has the same aggregate ID as supplied one
   * @param aggregateId component aggregate ID to compare to
   * @return true if equal
  */
  bool HasAggregateID(const std::string& aggregateId) const;

  /**
   * @brief get vendor
   * @return component vendor string
  */
   const std::string& GetVendorString() const override;

  /**
   * @brief check if the component is removed and not used by any target
   * @return true if removed
  */
   bool IsRemoved() const override;

  /**
   * @brief set this component as being removed from all project targets
   * @param removed flag to indicate removed state
  */
   void SetRemoved(bool removed) override;

  /**
   * @brief get path to doc file if any (for active target)
   * @return absolute path to doc file or URL, empty string if component is not resolved
  */
   std::string GetDocFile() const override;

  /**
   * @brief check if component instance has specific flags for different project targets
   * @return true if target specific
  */
  bool IsTargetSpecific() const;

  /**
   * @brief set/reset target-specific flag
   * @param bSet flag to indicate if component instance has specific flags for different project targets
   * @return true if the flag has changed
  */
  bool SetTargetSpecific(bool bSet);

  /**
   * @brief set component variant to use
   * @param variant Cvariant attribute value to set
   * @return true if Cvariant attribute has changed
  */
  bool SetVariant(const std::string& variant);

  /**
   * @brief set component version to use
   * @param variant Cversion attribute value to set
   * @return true if Cversion attribute has changed
  */
  bool SetVersion(const std::string& version);

  /**
   * @brief get actual component if resolved, this pointer otherwise
   * @param targetName target name to resolve component
   * @return resolved component if available for specified target, this pointer otherwise
  */
  RteItem* GetEffectiveItem(const std::string& targetName) const;

  /**
   * @brief get actual RtePackage from resolved component, potential component or RteModel via pack ID
   * @param targetName target name to resolve component
   * @return pointer to RtePackage of the resolved component, or potential one, or installed pack with instance pack ID
  */
   RtePackage* GetEffectivePackage(const std::string& targetName) const override;

  /**
   * @brief get display name of resolved component, otherwise own display name
   * @param targetName target name to resolve component
   * @return display name of the component resolved for given target, own name otherwise
  */
  virtual std::string GetEffectiveDisplayName(const std::string& targetName) const;


  /**
   * @brief get resolved component for specified target
   * @param targetName target name
   * @return pointer to resolved RteComponent, nullptr if not resolved
  */
  RteComponent* GetComponent(const std::string& targetName) const override
  { return GetResolvedComponent(targetName); }
  /**
   * @brief get resolved component for specified target
   * @param targetName target name to resolve component
   * @return pointer to RteComponent if resolved, nullptr otherwise
  */
  RteComponent* GetResolvedComponent(const std::string& targetName) const override;

  /**
   * @brief set resolved component for specified target
   * @param c pointer to resolved RteComponent if any
   * @param targetName target name to resolve component
  */
  void SetResolvedComponent(RteComponent*c, const std::string& targetName);

  /**
   * @brief get a component that can resolve this instance if its pack is not filtered-out
   * @param targetName target name to resolve component
   * @return pointer to RteComponent if could be potentially resolved, nullptr otherwise
  */
  RteComponent* GetPotentialComponent(const std::string& targetName) const;

  /**
   * @brief set a component that can resolve this instance if its pack is not filtered-out
   * @param c pointer to potentially resolved RteComponent if any
   * @param targetName target name to resolve component
  */
  void SetPotentialComponent(RteComponent*c, const std::string& targetName);

  /**
   * @brief find resolved and potential components for all targets
  */
  void ResolveComponent();

  /**
   * @brief clear all resolved and potentially resolved components for all targets
  */
  void ClearResolved();

  /**
   * @brief get component resolution result for given target
   * @param targetName target name to resolve component
   * @return RteItem::ConditionResult value
  */
  ConditionResult GetResolveResult(const std::string& targetName) const;

  /**
   * @brief resolve component for specified target
   * @param targetName target name to resolve component
   * @return pointer to RteComponent if resolved, nullptr otherwise
  */
  RteComponent* ResolveComponent(const std::string& targetName);


protected:
  RteComponentMap m_resolvedComponents;
  RteComponentMap m_potentialComponents; // potentially available components (packs are not selected)
  RteComponentInstance* m_copy;
};

/**
 * @brief class that aggregates component instances based on their aggregate ID
*/
class RteComponentInstanceAggregate : public RteItem
{
public:
  /**
   * @brief default constructor
   * @param parent pointer to RteItem parent
  */
  RteComponentInstanceAggregate(RteItem* parent = nullptr);

  /**
   * @brief virtual destructor
  */
   ~RteComponentInstanceAggregate() override;

public:

  /**
   * @brief clear internal data
  */
   void Clear() override;

  /**
   * @brief get instance aggregate display name
   * @return value of Csub attribute if available, Cgroup otherwise
  */
   std::string GetDisplayName() const override;

  /**
   * @brief get cached full display name
   * @return string in the format Cvendor.Cbundle::Cclass:Cgroup[:Csub]
  */
  virtual std::string GetFullDisplayName() const { return m_fullDisplayName; }

public:

  /**
   * @brief check if this aggregate ID matches the supplied one
   * @param aggregateId aggregate ID to compare to
   * @return true if equal
  */
  bool HasAggregateID(const std::string& aggregateId) const;

  /**
   * @brief check if aggregate contains specified component instance
   * @param ci pointer to RteComponentInstance
   * @return true if contains
  */
  bool HasComponentInstance(RteComponentInstance* ci) const;

  /**
   * @brief get component instance for specified target
   * @param targetName target name
   * @return pointer to RteComponentInstance if used in the target, nullptr otherwise
  */
  RteComponentInstance* GetComponentInstance(const std::string& targetName) const;

  /**
   * @brief get component aggregate of resolved component for specified target
   * @param targetName target name
   * @return pointer to RteComponentAggregate if resolved, nullptr otherwise
  */
  RteComponentAggregate* GetComponentAggregate(const std::string& targetName) const;

  /**
   * @brief check if component instance or its copy is not resolved
   * @param targetName target name to resolve component
   * @param bCopy true if the copy must be checked
   * @return true if component is not resolved for specified target
  */
  bool IsUnresolved(const std::string& targetName, bool bCopy = false) const;

  /**
   * @brief check if aggregate is supported by given target
   * @param targetName target name
   * @return true if supported
  */
  bool IsFilteredByTarget(const std::string& targetName) const;

  /**
   * @brief check if aggregate is used by target
   * @param targetName target name
   * @return true if supported and not excluded
  */
  bool IsUsedByTarget(const std::string& targetName) const;

  /**
   * @brief check if component is explicitly excluded from the target
   * @param targetName target name
   * @return true if excluded
  */
  bool IsExcluded(const std::string& targetName) const;

  /**
   * @brief check if aggregate contains only target-specific instances
   * @return true if all instances are target specific
  */
  bool IsTargetSpecific() const;

  /**
   * @brief check if common setting could be applicable to all instances
   * @return true if all members can support all targets
  */
  bool AllowsCommonSettings() const;

  /**
   * @brief add component instance to aggregate members
   * @param ci pointer to RteComponentInstance to add
  */
  void AddComponentInstance(RteComponentInstance* ci);

  /**
   * @brief check if the aggregate has a modified instance
   * @return true if modified
  */
  bool IsModified() const;

  /**
   * @brief get the modified component instance
   * @return pointer to the modified instance if any (can only be one)
  */
  RteComponentInstance* GetModifiedInstance() const;

  /**
   * @brief check if component can have multiple instances
   * @return cached flag indicating that component can have multiple instances
  */
   bool HasMaxInstances() const override {return m_bHasMaxInstances; }

private:
  std::string m_fullDisplayName;
  bool m_bHasMaxInstances;
};


/**
 * @brief class to support Cclass-Cgroup-Csub tree hierarchy.
*/
class RteComponentInstanceGroup : public RteItem
{
public:

  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteComponentInstanceGroup(RteItem* parent);


  /**
   * @brief virtual destructor
  */
   ~RteComponentInstanceGroup() override;

  /**
   * @brief get API instance associated with this group (Cgroup level)
   * @return pointer to RteComponentInstance if associated and available, nullptr otherwise
  */
  RteComponentInstance* GetApiInstance() const { return m_apiInstance; }

  /**
   * @brief check if the group contains one and only one component instance aggregate
   * @return true if the group contains single RteComponentInstance pointer as a child
  */
  bool HasSingleAggregate() const;

  /**
   * @brief check if the group contains instance aggregates or sub-groups with unresolved components
   * @param targetName target name to resolve components
   * @param bCopy true if working copies should be checked, not the instance originals
   * @return true if at least one component is not resolved
  */
  bool HasUnresolvedComponents(const std::string& targetName, bool bCopy = false) const;

  /**
   * @brief get condition result for this item (least from children)
   * @param context condition evaluation context
   * @return cached ConditionResult
  */
  ConditionResult GetConditionResult(RteConditionContext* context) const override;

  /**
   * @brief check if group or its sub-groups contain instance aggregates used by specified target
   * @param targetName target name
   * @return true if at least one instance aggregate is used buy the target
  */
  bool IsUsedByTarget(const std::string& targetName) const;

  /**
   * @brief get the single component instance aggregate (end-leaf level)
   * @return pointer to the single RteComponentInstanceAggregate if exist, nullptr otherwise
  */
  RteComponentInstanceAggregate* GetSingleComponentInstanceAggregate() const;

  /**
   * @brief find component instance aggregate with given aggregate ID
   * @param aggregateId component aggregate ID
   * @return pointer to RteComponentInstanceAggregate if found, nullptr otherwise
  */
  RteComponentInstanceAggregate* GetComponentInstanceAggregate(const std::string& aggregateId) const;

  /**
   * @brief find component instance aggregate containing supplied component instance
   * @param ci RteComponentInstance pointer to search for
   * @return pointer to RteComponentInstanceAggregate if found, nullptr otherwise
  */
  RteComponentInstanceAggregate* GetComponentInstanceAggregate(RteComponentInstance* ci) const;

  /**
   * @brief find component instance group containing supplied component instance
   * @param ci RteComponentInstance pointer to search for
   * @return pointer to RteComponentInstanceGroup if found, nullptr otherwise
  */
  RteComponentInstanceGroup* GetComponentInstanceGroup(RteComponentInstance* ci) const;

  /**
   * @brief get collection of sub-groups
   * @return map of name to RteComponentInstanceGroup pointer pairs
  */
  const std::map<std::string, RteComponentInstanceGroup*>& GetGroups() const { return m_groups; }

  /**
   * @brief get sub-group for the given name
   * @param name group name
   * @return pointer to RteComponentInstanceGroup if found, nullptr otherwise
  */
  RteComponentInstanceGroup* GetGroup(const std::string& name) const;

  /**
   * @brief get sub-group for the given name, create one if it does not exists
   * @param name group name
   * @return pointer to RteComponentInstanceGroup , never nullptr
  */
  RteComponentInstanceGroup* EnsureGroup(const std::string& name);

  /**
   * @brief add component instance to the tree
   * @param ci RteComponentInstance pointer to add
  */
  void AddComponentInstance(RteComponentInstance* ci);

  /**
   * @brief get collection of instance aggregates in this group
   * @param aggregates std::set of RteComponentInstanceAggregate pointers to fill
  */
  void GetInstanceAggregates(std::set<RteComponentInstanceAggregate*>& aggregates) const;

  /**
   * @brief get collection of modified instance aggregates
   * @param modifiedAggregates  std::set of RteComponentInstanceAggregate pointers to fill
  */
  void GetModifiedInstanceAggregates(std::set<RteComponentInstanceAggregate*>& modifiedAggregates) const;

public:

  /**
   * @brief clear internal data
  */
   void Clear() override;

  /**
   * @brief get group display name
   * @return
  */
   std::string GetDisplayName() const override;

private:
  std::map<std::string, RteComponentInstanceGroup*> m_groups; // sub-groups
  RteComponentInstance* m_apiInstance; // api instance for the Cgroup level
};

/**
 * @brief class to handle files instantiated in the project
*/
class RteFileInstance : public RteItemInstance
{
public:

  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */

  RteFileInstance(RteItem* parent);

public:
  /**
   * @brief initialize the file instance
   * @param f pointer to the original RteFile
   * @param deviceName device name used in the target
   * @param instanceIndex instance index, can be > 0  for multi-instance components
   * @param rteFolder the "RTE" folder path used for placing files
  */
  void Init(RteFile* f, const std::string& deviceName, int instanceIndex, const std::string& rteFolder);

  /**
   * @brief update file instance
   * @param f pointer to the original RteFile
   * @param bUpdateComponent update information about component this file belongs to
  */
  void Update(RteFile* f, bool bUpdateComponent);

  /**
   * @brief check if this file is a config one
   * @return true if "attr" attribute value is "config"
  */
  bool IsConfig() const;

  /**
   * @brief check if a new version of a config file is available (for specified target)
     RTE folder path used for placing files is taken from target's parent project
   * @param targetName target name
   * @return true if newer version of config file is available
  */
  int HasNewVersion(const std::string& targetName) const;

  /**
   * @brief check if a new version of a config file is available (for any target)
     RTE folder path used for placing files is taken from target's parent project
   * @return true if newer version of config file is available
  */
  int HasNewVersion() const;

  /**
   * @brief creates an info string of the config file (for specified target)
     RTE folder path used for placing files is taken from target's parent project
   * @param targetName target name
   * @param relativeTo calculate relative path to the supplied if not empty, otherwise to project
   * @return info string
  */
  std::string GetInfoString(const std::string& targetName, const std::string& relativeTo = RteUtils::EMPTY_STRING) const;

  /**
   * @brief get file category
   * @return RteFile::Category value
  */
  RteFile::Category GetCategory() const;

  /**
   * @brief get file scope
   * @return RteFile::Scope value
  */
  RteFile::Scope GetScope() const;

  /**
   * @brief get file language
   * @return RteFile::Language value
  */
  RteFile::Language GetLanguage() const;

  /**
   * @brief get zero-based file instance index
   * @return instance index
  */
  int GetInstanceIndex() const { return m_instanceIndex; }

  /**
   * @brief get file instance name
   * @return filename as it appears in the project, includes index
  */
  const std::string& GetInstanceName() const { return m_instanceName; }

  /**
   * @brief get the original filename as described in the pack
   * @return the original filename
  */
  const std::string& GetOriginalFileName() const { return GetName(); }

  /**
   * @brief get file display name
   * @return string in the format "filename comment"
  */
   std::string GetDisplayName() const override;

  /**
   * @brief get comment to show next to file in a project view
   * @return file comment containing originating component name
  */
  virtual std::string GetFileComment() const;

  /**
   * @brief get header file comment to show next to file name in an editor's context menu
   * @return header file comment containing originating component name
  */
  virtual std::string GetHeaderComment() const;

  /**
   * @brief get filename without path
   * @return filename
  */
  const std::string& GetFileName() const { return m_fileName; }

  /**
   * @brief get absolute path to the file
   * @return absolute path
  */
  std::string GetAbsolutePath() const;

  /**
   * @brief get include path to be added to compiler command line
   * @return include path for -I compiler option
  */
  std::string GetIncludePath() const;

  /**
   * @brief get header filename relative to include path
   * @return relative filename
  */
  std::string GetIncludeFileName() const;

  /**
   * @brief get component version
   * @return component version string
  */
  const std::string& GetComponentVersionString() const { return m_componentAttributes.GetVersionString(); }

  /**
   * @brief get the original file resolved to this instance for specified target
   * rteFolder is taken from target's parent project
   * @param targetName target name to resolve file
   * @param
   * @return pointer to RteFile if resolved, nullptr otherwise
  */
  RteFile* GetFile(const std::string& targetName) const;

  /**
   * @brief copy a config file from pack location to the designated project directory
   * @param f pointer to RteFile to copy
   * @param bMerge flag indicating to merge it to the existing one (if exists), otherwise overwrite
   * @return true is successful
  */
  bool Copy(RteFile* f, bool bMerge);

public:

  /**
   * @brief create a backup copy of a config file
   * @param bDeleteExisting delete the source file, i.e. perform move operation
   * @return unique backup filename if successful, empty string otherwise
  */
  std::string Backup(bool bDeleteExisting);

  /**
   * @brief return file instance ID
   * @return instance name
  */
   const std::string& GetID() const override { return m_instanceName; }

  /**
   * @brief get component instance this file belongs to
   * @param targetName target name to resolve component
   * @return pointer to RteComponentInstance
  */
   RteComponentInstance* GetComponentInstance(const std::string& targetName) const override;

  /**
   * @brief get component ID this file belongs to
   * @param withVersion flag to include component version to ID
   * @return component ID to use in a filtered list
  */
   std::string GetComponentID(bool withVersion) const override;

  /**
   * @brief get full component ID including originating pack
   * @return full  component ID
  */
   std::string GetComponentUniqueID() const override;

  /**
   * @brief get component aggregate ID
   * @param withVersion
   * @return
  */
   std::string GetComponentAggregateID() const override;

  /**
   * @brief get file version
   * @return file version string if present, otherwise component's version
  */
   const std::string& GetVersionString() const override;

  /**
   * @brief get component vendor
   * @return component vendor string
  */
   const std::string& GetVendorString() const override;

  /**
   * @brief get component bundle name
   * @return component bundle name
  */
   const std::string& GetCbundleName() const override;

  /**
   * @brief get name of a file group this file instance belongs to in the project
   * @return project group name
  */
   std::string GetProjectGroupName() const override;

   /**
    * @brief called to construct the item with attributes and child elements
   */
   void Construct() override;

protected:

  /**
   * @brief creates child element for supplied XMLTreeElement
   * @param parentElement XMLTreeElement to generate content for
  */
   void CreateXmlTreeElementContent(XMLTreeElement* parentElement) const override;

protected:
  int m_instanceIndex; // zero-base instance index for multi-instance components, -1 otherwise
  std::string m_instanceName; // file name relative to project directory
  std::string m_fileName; // file name without path
  RteItem m_componentAttributes;
};

#endif // RteInstance_H
