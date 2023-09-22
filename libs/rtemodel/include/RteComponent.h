#ifndef RteComponent_H
#define RteComponent_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteComponent.h
* @brief CMSIS RTE Data Model
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

class RteFile;
class RteTarget;
class RteFileContainer;
class RtePackage;
class RteApi;
class RteBundle;
class RteFileInstance;
class RteComponentGroup;
class RteComponentAggregate;
class RteComponentInstance;
class RteGenerator;
class RteDependencySolver;

/**
 * @brief class to represent CMSIS software component, corresponds to <component> element in *.pdsc file.
*/
class RteComponent : public RteItem
{
public:

 /**
  * @brief constructor
  * @param parent pointer to RteItem parent
 */
  RteComponent(RteItem* parent);

  /**
   * @brief virtual destructor
  */
   ~RteComponent() override;

  /**
   * @brief get number of files described in this component
   * @return number of <file> elements
  */
  size_t GetFileCount() const;
  /**
   * @brief get <files> element
   * @return pointer to RteFileContainer representing container for RteFile items
  */
  RteFileContainer* GetFileContainer() const { return m_files; }

  /**
   * @brief filter component files for given target
   * @param target pointer to RteTarget providing filter context and storing filtered files
  */
  void FilterFiles(RteTarget* target);

  /**
   * @brief get generator associated with this component
   * @return pointer to RteGenerator if associated, nullptr otherwise
  */
  RteGenerator* GetGenerator() const;

  /**
   * @brief get *.gpdsc file associated with the component
   * @param target pointer to RteTarget providing working directory
   * @return absolute pathname with expanded key sequences, empty string if component is not associated with any generator
  */
  std::string GetGpdscFile(RteTarget* target) const;

public:
  /**
   * @brief check if this item represents <api> element
   * @return false, the base supports only <component>
  */
   bool IsApi() const override { return false; }

  /**
   * @brief check if this component implements an API
   * @param target pointer to RteTarget for filtering context
   * @return true if the component has "Capiversion" attribute or target's model contains API declaration with the same Cclass and Cgroup
  */
  virtual bool HasApi(RteTarget* target) const;

  /**
   * @brief check if API declaration exists for the specified target
   * @param target pointer to RteTarget for filtering context
   * @return ConditionResult value:
   * IGNORED (component has no API),
   * FULFILLED (API is found),
   * MISSING_API (no API is found),
   * MISSING_API_VERSION (required API version not found)
  */
  virtual ConditionResult IsApiAvailable(RteTarget* target) const;

  /**
   * @brief get API declaration item
   * @param target pointer to RteTarget for filtering context
   * @param matchVersion flag if to match exact API version
   * @return pointer to RteApi if found, nullptr otherwise
  */
  virtual RteApi* GetApi(RteTarget* target, bool matchVersion) const;

  /**
   * @brief check if this component represents a Device.Startup component
   * @return true if Cclass == "Device" and Cgroup=="Startup" and Csub is empty
  */
  virtual bool IsDeviceStartup() const;

  /**
   * @brief check if component is device dependent:
   * associated condition contains expressions that evaluate "Dname" attribute
   * @return true if device dependent
  */
   bool IsDeviceDependent() const override;

  /**
   * @brief create a new instance of type RteItem
   * @param tag name of tag
   * @return pointer to instance of type RteItem
  */
  RteItem* CreateItem(const std::string& tag) override;

  /**
   * @brief called to construct the item with attributes and child elements
  */
  void Construct() override;

  /**
   * @brief clear internal data and children
  */
   void Clear() override;

  /**
   * @brief validate component after construction, in particular checks for duplicated component declarations
   * @return true if validated sucessfully
  */
   bool Validate() override;

  /**
   * @brief get parent component bundle if any
   * @return pointer to parent RteBundle, nullptr if component does not belong to a bundle
  */
  virtual RteBundle* GetParentBundle() const;

  /**
   * @brief return component name (reserved for future)
   * @return "Cname" attribute value, currently empty string
  */
   const std::string& GetName() const override;

  /**
   * @brief get component display name to be used in the tree views
   * @return "Csub" attribute value if set, otherwise "Cgroup" attribute value
  */
   std::string GetDisplayName() const override;

  /**
   * @brief get full component name to display
   * @return string in the format Cvendor[.Cbundle]::Cclass:Cgroup[:Csub][:Cvariant]:Cversion
  */
  virtual std::string GetFullDisplayName() const;

  /**
   * @brief get "Cclass" attribute value from this item or parent bundle
   * @return "Cclass" attribute value
  */
   const std::string& GetCclassName() const override;

  /**
   * @brief get "Cbundle" attribute value from this item or parent bundle
   * @return "Cbundle" attribute value, empty if component does not belong to a bundle
  */
   const std::string& GetCbundleName() const override;

  /**
   * @brief get component version string from this item or parent bundle
   * @return version string
  */
   const std::string& GetVersionString() const override;

  /**
   * @brief get component vendor string from this item or parent( bundle, component container or pack)
   * @return vendor string
  */
   const std::string& GetVendorString() const override;

  /**
   * @brief get path to component documentation
   * @return an URL or absolute filename to doc file if any
  */
   std::string GetDocFile() const override;

  /**
   * @brief get parent component
   * @return this since components cannot be nested
  */
   RteComponent* GetComponent() const override { return const_cast<RteComponent*>(this); }

  /**
   * @brief get cached result of evaluating associated condition
   * @param context pointer to RteConditionContext
   * @return RteItem::ConditionResult value
  */
   ConditionResult GetConditionResult(RteConditionContext* context) const override;

  /**
   * @brief insert this component in the supplied model
   * @param model pointer to RteModel
  */
   void InsertInModel(RteModel* model) override;

  /**
   * @brief check if this component dominates over the supplied one
   * @param that another component to check against
   * @return true if package of this component has attribute 'dominate'
   * if package of that component also has attribute 'dominate', return true if this component version is higher.
  */
  bool Dominates(RteComponent *that) const;

  /**
   * @brief construct name for component pre-include header file (prefix "Pre_Include")
   * @return header file name for component pre-include
  */
  virtual std::string ConstructComponentPreIncludeFileName() const;


protected:
  /**
   * @brief construct component ID
   * @return constructed component ID string
  */
   std::string ConstructID() override;

  /**
   * @brief item corresponding <files> element
  */
  RteFileContainer* m_files;
};

/**
 * @brief class to represent CMSIS software component API declaration, corresponds to <api> element in *.pdsc file.
*/
class RteApi : public RteComponent
{
public:
  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteApi(RteItem* parent);

  /**
   * @brief check if this API is exclusive
   * @return true if exclusive
  */
  bool IsExclusive() const;

public:
  /**
   * @brief validate API item after construction
   * @return true if validation is successful
  */
   bool Validate() override;

  /**
   * @brief check if this item represents <api> element
   * @return true
  */
   bool IsApi() const override { return true; }

  /**
   * @brief get API version string
   * @return "Capiversion" attribute value
  */
   const std::string& GetVersionString() const override { return GetAttribute("Capiversion"); }

  /**
   * @brief return full display name
   * @return string in format  'Cvendor::Cclass:Cgroup:Cversion (API)'
  */
   std::string GetFullDisplayName() const override;

  /**
   * @brief get API ID
   * @return API ID
  */
   std::string GetComponentUniqueID() const override;

  /**
   * @brief get API ID
   * @param withVersion ignored argument, treated as false
   * @return API ID
  */
   std::string GetComponentID(bool withVersion) const override;
};

class RteComponentClass;
/**
 * @brief class representing <components> element in *.pdsc and *.cprj files. Base class for RteBundle, RteComponentClass and RteComponentGroup classes.
*/
class RteComponentContainer : public RteItem
{
public:
  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteComponentContainer(RteItem* parent);

public:

  /**
   * @brief get component vendor string from this item or parent pack
   * @return vendor string
  */

   const std::string& GetVendorString() const override;

  /**
   * @brief get pointer to RteComponentClass item containing this container
   * @return return pointer to parent RteComponentClass item if any, nullptr otherwise
  */
  virtual RteComponentClass* GetComponentClass() const;

  /**
   * @brief get selected bundle name
   * @return selected bundle name
  */
  virtual const std::string& GetSelectedBundleName() const;

  /**
   * @brief get bundle ID without version of currently selected bundle
   * @return string in format Cvendor.Cbundle
  */
  virtual const std::string& GetSelectedBundleShortID() const;

  /**
   * @brief get parent target item
   * @return pointer to parent RteTarget item if any, nullptr otherwise
  */
  virtual RteTarget* GetTarget() const;

  /**
   * @brief get a tool string to be displayed in component selection view
   * @return tooltip string (default implementation returns empty string)
  */
  virtual std::string GetToolTip() const { return EMPTY_STRING; }

  /**
   * @brief create a new instance of type RteItem
   * @param tag name of tag
   * @return pointer to instance of type RteItem
  */
  RteItem* CreateItem(const std::string& tag) override;
};

/**
 * @brief class to represent <bundle> element in *.pdsc file
*/
class RteBundle : public RteComponentContainer
{
public:
  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteBundle(RteItem* parent);

public:
  /**
   * @brief get display name
   * @return bundle display name in format Cvendor.Cbundle
  */
   std::string GetDisplayName() const override;

  /**
   * @brief get full bundle name to display
   * @return bundle ID in the format Cvendor.Cbundle::Cclass:Cversion
  */
  virtual std::string GetFullDisplayName() const;

  /**
   * @brief get version string
   * @return "Cversion" attribute value
  */
   const std::string& GetVersionString() const override;
  /**
   * @brief insert this bundle and its components in the supplied model
   * @param model pointer to RteModel
  */
   void InsertInModel(RteModel* model) override;

protected:
  /**
   * @brief construct bundle ID
   * @return bundle ID string in the format Cvendor.Cbundle::Cclass:Cversion
  */
   std::string ConstructID() override;
};

typedef std::map<std::string, RteBundle*, AlnumCmp::LenGreater> RteBundleMap;


/**
 * @brief class to represent <apis> element in *.pdsc file
*/
class RteApiContainer : public RteItem
{
public:
  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteApiContainer(RteItem* parent);

  /**
   * @brief create a new instance of type RteItem
   * @param tag name of tag
   * @return pointer to instance of type RteItem
  */
  RteItem* CreateItem(const std::string& tag) override;
};

typedef std::map<std::string, RteApi*, VersionCmp::Greater > RteApiMap;
typedef std::map<std::string, RteComponent*> RteComponentMap;
typedef std::map<std::string, RteComponent*, VersionCmp::Greater> RteComponentVersionMap;
typedef std::list<RteComponent*> RteComponentList;

/**
 * @brief class that aggregates different component variants and versions in one selectable entity.
*/
class RteComponentAggregate : public RteComponentContainer
{
public:

  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteComponentAggregate(RteItem* parent);

  /**
   * @brief virtual destructor
  */
   ~RteComponentAggregate() override;

  /**
   * @brief add component to this aggregate
   * @param c pointer to RteComponent to add
  */
  void AddComponent(RteComponent* c);

  /**
   * @brief set component instance to this aggregate
   * @param ci pointer to RteComponentInstance
   * @param count selection count
  */
  void SetComponentInstance(RteComponentInstance* ci, int count);

  /**
   * @brief check if this aggregate contains specified component
   * @param c pointer to RteComponent
   * @return true if found
  */
  bool HasComponent(RteComponent* c) const;

  /**
   * @brief get all components added to this aggregate
   * @return map of variant name to version map
  */
  const std::map<std::string, RteComponentVersionMap>& GetAllComponents() const { return m_components; }

  /**
   * @brief get list of component versions for specified variant
   * @param variant component variant (can be empty)
   * @return map of version to RteComponent pointer pairs ordered by version (descending)
  */
  const RteComponentVersionMap* GetComponentVersions(const std::string& variant) const;

  /**
   * @brief get component instance set to the aggregate
   * @return pointer to RteComponentInstance if set, nullptr otherwise
  */
  RteComponentInstance* GetComponentInstance() const { return m_instance; }

  /**
   * @brief check if this component aggregate matches supplied component instance.
   * Performs wild-card match of attributes.
   * Called during resolving components
   * @param ci pointer to RteComponentInstance
   * @return true if attributes of supplied instance match this aggregate one
  */
  bool MatchComponentInstance(RteComponentInstance* ci) const;

  /**
   * @brief get total number of components added to the aggregate
   * @return number of components in the aggregate
  */
  int GetComponentCount() const;

  /**
   * @brief get number of different component variants in the aggregate
   * @return number of variants
  */
  int GetVariantCount() const { return (int)m_components.size(); }

  /**
   * @brief get number of component versions for given variant
   * @param variant component variant name, corresponds to "Cvariant" attribute value
   * @return number of versions for specified variant
  */
  std::list<std::string> GetVersions(const std::string& variant) const;

  /**
   * @brief get list of component variant names
   * @return list of strings
  */
  std::list<std::string> GetVariants() const;

  /**
   * @brief check if aggregate contains specified component variant
   * @param variant component variant name
   * @return true if aggregate contains specified variant
  */
  bool HasVariant(const std::string& variant) const;

  /**
   * @brief get currently selected variant
   * @return selected variant name
  */
  const std::string& GetSelectedVariant() const { return m_selectedVariant; }

  /**
   * @brief get component variant selected by default
   * @return default variant name
  */
  const std::string& GetDefaultVariant() const { return m_defaultVariant; }

  /**
   * @brief get currently selected component version of currently selected variant
   * @return selected version string
  */
  const std::string& GetSelectedVersion() const { return m_selectedVersion; }

  /**
   * @brief get effectively used component version for selected variant
   * @return selected version if set, the latest one otherwise
  */
  std::string GetEffectiveVersion() const;

  /**
   * @brief get the latest component version for specified variant
   * @param variant component variant name
   * @return latest version if the variant found, empty string otherwise
  */
  std::string GetLatestVersion(const std::string& variant) const;

  /**
   * @brief reset variant selection to the default variant
  */
  void ResetToDefaultVariant();

  /**
   * @brief select component variant
   * @param variant component variant name to select
  */
  void SetSelectedVariant(const std::string& variant);

  /**
   * @brief select version for currently selected component variant
   * @param version component version to select
  */
  void SetSelectedVersion(const std::string& version) { m_selectedVersion = version; }

  /**
   * @brief check if aggregate represents a multi-instance component
   * @return true if multi-instance
  */
   bool HasMaxInstances() const override { return m_bHasMaxInstances || m_maxInstances > 1; }

  /**
   * @brief get maximal number of instances that can be select for the component
   * @return maximal number of instances as integer value
  */
   int GetMaxInstances() const override { return m_maxInstances; }

  /**
   * @brief remove entries in component collection that do not contain a pointer to RteComponent
  */
  void Purge();

public:

  /**
   * @brief check if this component aggregate is empty, i.e. does not contain any entry in component collection
   * @return true if empty
  */
   bool IsEmpty() const override;

  /**
   * @brief check if at least one component passes current filter.
   * In practice all components should have the same filtered state, because they share the same condition
   * @return true if filtered
  */
  virtual bool IsFiltered() const;

  /**
   * @brief check if component aggregate is selected (number of instances)
   * @return selection count: 0 - not selected
  */
  virtual int IsSelected() const { return m_nSelected; }

  /**
   * @brief check if component aggregate is used in project
   * @return true if used
  */
  virtual bool IsUsed() const;

  /**
   * @brief select component aggregate to add to project.
   * Effectively an RteComponent with selected variant and version will be added to the project
   * @param nSel number of component instances to add to project, 0 to remove component from the project
   * @return true if selection state has changed
  */
  virtual bool SetSelected(int nSel);

  /**
   * @brief checks if the aggregate contains at least one component whose attributes match those in the supplied map
   * @param attributes map of key-value pairs to match against component attributes
   * @param bRespectVersion flag to consider Cversion and Capiversion attributes, default is true
   * @return true if at least one component has all attributes found in the supplied map
  */
  bool MatchComponentAttributes(const std::map<std::string, std::string>& attributes, bool bRespectVersion = true) const override;

  /**
   * @brief get short component aggregate display name to use in a tree view
   * @return value of "Csub" attribute if set, "Cgroup" one otherwise
  */
   std::string GetDisplayName() const override;

  /**
   * @brief get full component aggregate display name
   * @return string in the format Cvendor[.Cbundle]::Cclass:Cgroup[:Csub]
  */
  virtual std::string GetFullDisplayName() const { return m_fullDisplayName; }


  /**
   * @brief get active RteComponent, i.e. the one with currently selected variant and version
   * @return pointer to RteComponent
  */
   RteComponent* GetComponent() const override;

  /**
   * @brief get active component description
   * @return component description as string
  */
   const std::string& GetDescription() const override;

  /**
   * @brief get document file of active component
   * @return component document file or URL
  */
   std::string GetDocFile() const override;

  /**
   * @brief get pack containing active component
   * @return pointer to RtePackage
  */
   RtePackage* GetPackage() const override;

  /**
   * @brief get RteComponent item for specified variant and version
   * @param variant component variant name
   * @param version component version
   * @return pointer to RteComponent if found, nullptr otherwise
  */
  RteComponent* GetComponent(const std::string& variant, const std::string& version) const;

  /**
   * @brief search for component matching specified variant and version
   * @param variant component variant
   * @param pattern wild-card pattern to match component version
   * @return pointer to RteComponent if found, nullptr otherwise
  */
  RteComponent* FindComponentMatch(const std::string& variant, const std::string& pattern) const;

  /**
   * @brief search for RteComponent item matching supplied attributes
   * @param attributes std::map with attributes to match
   * @return pointer to RteComponent if found, nullptr otherwise
  */
  RteComponent* FindComponent(const std::map<std::string, std::string>& attributes) const;

  /**
   * @brief get RteComponent with the latest version available for specified variant
   * @param variant component variant name
   * @return pointer to RteComponent if found, nullptr otherwise
  */
  RteComponent* GetLatestComponent(const std::string& variant) const;

  /**
   * @brief clear internal aggregate structure
  */
   void Clear() override;

  /**
   * @brief get full or common pack ID of active component
   * @param withVersion flag to return full (true) or common (false) pack ID
   * @return pack ID of active component
  */
   std::string GetPackageID(bool withVersion = true) const override;

  /**
   * @brief get a tooltip string to be displayed in component selection view
   * @return tooltip string containing full aggregate display name and pack ID of the active component
  */
   std::string GetToolTip() const override;

  /**
   * @brief evaluate condition associated with the component
   * @param context pointer to RteConditionContext to evaluate
   * @return RteItem::ConditionResult, IGNORED if active component has no condition
  */
   ConditionResult Evaluate(RteConditionContext* context) override;

  /**
   * @brief get cached evaluation result for given context
   * @param context pointer to RteConditionContext to obtain cached value from
   * @return evaluation result as RteItem::ConditionResult value
  */
   ConditionResult GetConditionResult(RteConditionContext* context) const override;

  /**
   * @brief collect cached results of evaluating component dependencies by this item
   * @param results map to collect results
   * @param target RteTarget associated with condition context
   * @return overall RteItem::ConditionResult value for this item
  */
   ConditionResult GetDepsResult(std::map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const override;

private:
  std::map<std::string, RteComponentVersionMap> m_components; // key: variant, value: version ordered component std::list (descending);
  RteComponentInstance* m_instance; // assigned RteComponentInstance if used in project
  std::string m_fullDisplayName;
  std::string m_selectedVersion;
  std::string m_selectedVariant;
  std::string m_defaultVariant;
  int m_maxInstances; // maximum available instance count
  bool m_bHasMaxInstances;
  int m_nSelected;
};


/**
 * @brief class to support class-group-subgroup hierarchy for using in RteModel to resolve components and their dependencies as well as to support component selection
*/
class RteComponentGroup : public RteComponentContainer
{
public:
  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteComponentGroup(RteItem* parent);

  /**
   * @brief virtual destructor
  */
   ~RteComponentGroup() override;

  /**
   * @brief get API associated with the group (Cgroup level)
   * @return pointer to RteApi associated with the group if any
  */
  RteApi* GetApi() const { return m_api; }

  /**
   * @brief get API instance associated with the group (Cgroup level)
   * @return pointer to RteComponentInstance representing API associated with the group if any
  */
  RteComponentInstance* GetApiInstance() const { return m_apiInstance; }

  /**
   * @brief check if the group has an associated API (Cgroup level)
   * @return true if the group has API, even if m_api is nullptr
  */
  bool HasApi() const { return m_bHasApi; }

  /**
   * @brief check if group contains one and the only one component aggregate
   * @return true if only one component aggregate is in the group
  */
  bool HasSingleAggregate() const;

  /**
   * @brief get the one and only one component aggregate stored in the group
   * @return pointer RteComponentAggregate if it is the only one in the collection, nullptr otherwise
  */
  RteComponentAggregate* GetSingleComponentAggregate() const;

  /**
   * @brief get component aggregate for specified ID
   * @param aggregateId component aggregate ID
   * @return pointer to RteComponentAggregate if found, nullptr otherwise
  */
  RteComponentAggregate* GetComponentAggregate(const std::string& aggregateId) const;

  /**
   * @brief get aggregate containing supplied component
   * @param c pointer to RteComponent to search for
   * @return pointer to RteComponentAggregate if found, nullptr otherwise
  */
  RteComponentAggregate* GetComponentAggregate(RteComponent* c) const;

  /**
   * @brief find component aggregate matching supplied instance to resolve a component
   * @param ci pointer to RteComponentInstance to match
   * @return pointer to RteComponentAggregate if found, nullptr otherwise
  */
  virtual RteComponentAggregate* FindComponentAggregate(RteComponentInstance* ci) const;

  /**
   * @brief collect selected component aggregates
   * @param selectedAggregates reference to map of RteComponentAggregate pointer to selection count pairs to fill
  */
  void CollectSelectedComponentAggregates(std::map<RteComponentAggregate*, int>& selectedAggregates) const;

  /**
   * @brief collect all aggregates representing components from *.gpdsc files that are not selected
   * @param unSelectedGpdscAggregates reference to set of RteComponentAggregate pointers to fill
  */
  void GetUnSelectedGpdscAggregates(std::set<RteComponentAggregate*>& unSelectedGpdscAggregates) const;

  /**
   * @brief clear selection for all component aggregates by setting selection count to 0
  */
  void ClearSelectedComponents();

  /**
   * @brief set RteComponentInstance pointers to nullptr for all component aggregates
  */
  void ClearUsedComponents();

  /**
   * @brief delete all empty aggregates and groups
  */
  void Purge();

  /**
   * @brief get component group containing specified component
   * @param c pointer to RteComponent to search for
   * @return pointer to RteComponentGroup if found, nullptr otherwise
  */
  RteComponentGroup* GetComponentGroup(RteComponent* c) const;

  /**
   * @brief get collection of all sub-groups in this group
   * @return reference to map of group name to RteComponentGroup pointer pairs
  */
  const std::map<std::string, RteComponentGroup*>& GetGroups() const { return m_groups; }

  /**
   * @brief get sub-group with specified name
   * @param name sub-group name
   * @return pointer to RteComponentGroup if found, nullptr otherwise
  */
  RteComponentGroup* GetGroup(const std::string& name) const;

  /**
   * @brief get sub-group with specified name, creates one if does not exists
   * @param name sub-group name
   * @return pointer to RteComponentGroup, never nullptr
  */
  RteComponentGroup* EnsureGroup(const std::string& name);

  /**
   * @brief add component to the group recursively adding to subgroups and to the corresponding aggregate
   * @param c pointer to RteComponent to add
  */
  void AddComponent(RteComponent* c);

  /**
   * @brief add component instance to the group recursively adding to subgroups and to the corresponding aggregate
   * @param ci pointer to RteComponentInstance to add
  */
  void AddComponentInstance(RteComponentInstance* ci, int count);

  /**
   * @brief get bundle names available in the group (Cclass level)
   * @return map of Cbundle to bundle display name pairs
  */
  const std::map<std::string, std::string>& GetBundleNames() const { return m_bundleNames; }

  /**
   * @brief check if group contains specified bundle name (Cclass level)
   * @param name bundle name to search for
   * @return true if found
  */
  bool HasBundleName(const std::string& name) const;

  /**
   * @brief add bundle name and its ID to the group
   * @param name bundle name
   * @param id bundle ID
  */
  virtual void AddBundleName(const std::string& name, const std::string& id);

  /**
   * @brief check if group or its sub-group(s) contain component aggregates with unresolved components.
   * Component aggregate is unresolved when it contains pointer to an RteComponentInstance, but not to an RteComponent
   * @return true if there are missing components
  */
  virtual bool HasMissingComponents() const;

  /**
   * @brief get tooltip to show in component selection view
   * @return tooltip string with group name and API information if available
  */
   std::string GetToolTip() const override;

public:
  /**
   * @brief check if group is empty
   * @return true if group contains no data: API, sub-groups and aggregates
  */
   bool IsEmpty() const override;

  /**
   * @brief clear internal data
  */
   void Clear() override;

  /**
   * @brief get group display name
   * @return display name depending on level: value of "Cclass", "Cgroup" or "Csub" component attribute
  */
   std::string GetDisplayName() const override;

  /**
   * @brief check if group or at least one of its sub-groups contains selected aggregates
   * @return true if selected
  */
  virtual int IsSelected() const;

  /**
   * @brief check if group is visible for component selection
   * @return true if group contains selected bundle name
  */
  virtual bool IsFiltered() const;

  /**
   * @brief get taxonomy item ID corresponding group's Cclass and/or Croup information
   * @return taxonomy description ID
  */
   std::string GetTaxonomyDescriptionID() const override;

  /**
   * @brief get group description
   * @return description taken from API or taxonomy
  */
   const std::string& GetDescription() const override;

  /**
   * @brief get group document file
   * @return file or URL to documentation taken from API or taxonomy
  */
   std::string GetDocFile() const override;

  /**
   * @brief get API version (Cgroup level)
   * @return API version is available, empty string otherwise
  */
   const std::string& GetApiVersionString() const override;

  /**
   * @brief evaluate conditions associated with components children recursively
   * @param context pointer to RteConditionContext to evaluate
   * @return overall (worst) evaluation result out of all evaluated conditions as ConditionResult value
  */
   ConditionResult Evaluate(RteConditionContext* context) override;


  /**
   * @brief get cached evaluation result for the group for specified context
   * @param context pointer to RteConditionContext caching the values
   * @return overall RteItem::ConditionResult value for this group:
   * UNDEFINED if the group is not filtered or does not have selected components,
   * MISSING if associated API is not available,
   * otherwise the worst result from all components recursively
  */
   ConditionResult GetConditionResult(RteConditionContext* context) const override;

  /**
   * @brief collect cached results of evaluating component dependencies by this item
   * @param results map to collect results
   * @param target RteTarget associated with condition context
   * @return overall RteItem::ConditionResult value for this group:
   * UNDEFINED if the group is not filtered or does not have selected components,
   * MISSING if associated API is not available,
   * otherwise the worst result from all components recursively
  */
   ConditionResult GetDepsResult(std::map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const override;

  /**
   * @brief find recursively component aggregates matching supplied attributes
   * @param componentAttributes component attributes as a reference to XmlItem
   * @param components std::set collection of RteComponentAggregate pointer to fill
   * @return overall matching result as RteItem::ConditionResult value
  */
  virtual ConditionResult GetComponentAggregates(const XmlItem& componentAttributes, std::set<RteComponentAggregate*>& components) const;

  /**
   * @brief adjust component selection in the group when active bundle changes
   * @param newBundleName name of newly selected bundle
  */
  virtual void AdjustComponentSelection(const std::string& newBundleName);

protected:
  /**
   * @brief create new child group
   * @param name group name
   * @return pointer to created RteComponentGroup
  */
  virtual RteComponentGroup* CreateGroup(const std::string& name);

  /**
   * @brief insert component bundle name and ID into internal collection
   * @param name bundle name
   * @param id bundle ID
  */
  virtual void InsertBundleName(const std::string& name, const std::string& id);

protected:
  std::map<std::string, RteComponentGroup*> m_groups; // sub-groups
  bool m_bHasApi; // flag telling that the group has an associated API (even if missing)
  RteApi* m_api;  // associated API (nullptr if not associated or missing)
  RteComponentInstance* m_apiInstance; // API in stance associated with the group
  std::string m_apiVersionString;      // API version
  std::map<std::string, std::string> m_bundleNames; // name-ID
};

/**
 * @brief class representing "Cclass" level of component groups
*/
class RteComponentClass : public RteComponentGroup
{
public:
  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteComponentClass(RteItem* parent);
  /**
   * @brief virtual destructor
  */
   ~RteComponentClass() override;

  /**
   * @brief set active bundle
   * @param name  bundle name
   * @param updateSelection flag to adjust component selection
  */
  void SetSelectedBundleName(const std::string& name, bool updateSelection);
  /**

   * @brief ensures that an active bundle is selected
   * @return selected bundle name, can be empty if no bundle is selected (only components without bundle are shown)
  */
  const std::string& EnsureSelectedBundleName();

  /**
   * @brief get active selected bundle object
   * @return pointer to selected RteBundle or nullptr if no active bundle is selected
  */
  RteBundle* GetSelectedBundle() const;
public:

  /**
    * @brief add bundle name and its ID to the group
    * @param name bundle name
    * @param id bundle ID
   */
   void AddBundleName(const std::string& name, const std::string& id) override;

  /**
   * @brief get pointer to RteComponentClass item containing this container
   * @return return this
  */
   RteComponentClass* GetComponentClass() const override;

  /**
   * @brief return active selected bundle name
   * @return selected bundle name
  */
   const std::string& GetSelectedBundleName() const override { return m_selectedBundleName; }

  /**
   * @brief return active selected bundle name
   * @return selected bundle name
  */
   const std::string& GetSelectedBundleShortID() const override;

  /**
   * @brief get component class documentation file or URL
   * @return filename or URL bundle if available or taxonomy
  */
   std::string GetDocFile() const override;

  /**
   * @brief get taxonomy description ID for associated component class
   * @return group name corresponding "Cclass" component attribute value
  */
   std::string GetTaxonomyDescriptionID() const override;

  /**
   * @brief get component class description
   * @return description string taken from bundle if available or taxonomy
  */
   const std::string& GetDescription() const override;

  /**
    * @brief get a tooltip string to be displayed in component selection view
    * @return tooltip string containing active bundle name and  its pack ID
   */
   std::string GetToolTip() const override;

  /**
   * @brief adjust component selection in the group when active bundle changes
   * @param newBundleName name of newly selected bundle
  */
   void AdjustComponentSelection(const std::string& newBundleName) override;

private:
  std::string m_selectedBundleName; // selected bundle name
};

/**
 * @brief class representing top-level container for RteComponent class groups
*/
class RteComponentClassContainer : public RteComponentGroup
{
public:
  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteComponentClassContainer(RteItem* parent);

  /**
   * @brief create new group of RteComponentClass type
   * @param name component class name
   * @return pointer to RteComponentGroup representing RteComponentClass
  */
   RteComponentGroup* CreateGroup(const std::string& name) override;

  /**
   * @brief find recursively component aggregates matching supplied attributes
   * @param componentAttributes component attributes as a reference to XmlItem
   * @param components std::set collection of RteComponentAggregate pointer to fill
   * @return overall matching result as RteItem::ConditionResult value
  */
   ConditionResult GetComponentAggregates(const XmlItem& componentAttributes, std::set<RteComponentAggregate*>& components) const override;

  /**
   * @brief find component aggregate matching supplied instance to resolve a component
   * @param ci pointer to RteComponentInstance to match
   * @return pointer to RteComponentAggregate if found, nullptr otherwise
  */
   RteComponentAggregate* FindComponentAggregate(RteComponentInstance* ci) const override;

  /**
   * @brief find component class with given name
   * @param name component class name
   * @return pointer to RteComponentClass if found, nullptr otherwise
  */
  RteComponentClass* FindComponentClass(const std::string& name) const;

  /**
   * @brief check if at least one component passes current filter
   * @return true (top is always available)
  */
   bool IsFiltered() const override { return true; }

  /**
   * @brief check if at least one component is selected
   * @return 1 (top container is always treated as "selected")
  */
   int IsSelected() const override { return 1; }
};

#endif // RteComponent_H
