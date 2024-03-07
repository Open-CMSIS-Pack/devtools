#ifndef RteProject_H
#define RteProject_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteProject.h
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
#include "RteInstance.h"
#include "RteModel.h"

#include <optional>
#include <stdio.h>

class RteModel;
class RteComponent;
class RteComponentAggregate;
class RteComponentInstance;
class RteFileInstance;
struct RteFileInfo;
class RteGenerator;
class RteTarget;
class CprjTargetElement;

/**
 * @brief class to represent aggregated license info
*/
class RteLicenseInfo :public RteItem
{
public:
  /**
   * @brief default constructor
   * @param parent
  */
  RteLicenseInfo(RteItem* parent = nullptr);

 /**
  * @brief add component ID to the internal collection
  * @param componentID component ID string
 */
  void AddComponentId(const std::string& componentID) { m_componentIDs.insert(componentID); }

  /**
   * @brief add pack ID to the internal collection
   * @param packID pack ID string
  */
  void AddPackId(const std::string& packID) { m_packIDs.insert(packID); }


  /**
   * @brief return collection of component IDs associated with the license
   * @return set of component IDs
  */
  const std::set<std::string>& GetComponentIDs() const { return m_componentIDs; }

  /**
   * @brief return collection of pack IDs associated with the license
   * @return set of pack IDs
  */
  const std::set<std::string>& GetPackIDs() const { return m_packIDs; };

  /**
   * @brief return collection package IDs associated with the license
   * @return pack ID string
  */
  std::string GetPackageID(bool withVersion = true) const override { return GetAttribute("pack"); };

  /**
   * @brief convert info content to yml-like text
   * @return yml formatted text
  */
  std::string ToString(unsigned indent = 0);

  /**
   * @brief construct license title
   * @param license pointer to RteItem representing <license> element
   * @return license ID string : spdx or combination of title and type
  */
  static std::string ConstructLicenseTitle(const RteItem* license);

  /**
   * @brief construct license internal ID
   * @param license pointer to RteItem representing <license> element
   * @return license ID string : spdx or combination of title, type and pack ID
  */
  static std::string ConstructLicenseID(const RteItem* license);

protected:
  std::set<std::string> m_componentIDs;
  std::set<std::string> m_packIDs;
};

class RteLicenseInfoCollection
{
public:
  /**
   * @brief  default constructor
  */
  RteLicenseInfoCollection() {};

  /**
   * @brief virtual destructor
  */
  virtual ~RteLicenseInfoCollection();

  /**
   * @brief clear internal structures
  */
  void Clear();

  /**
   * @brief add license info to the collection
   * @param item pointer to RteItem (RteComponent, RteApi, RtePackage)
  */
  void AddLicenseInfo(RteItem* item);

  /**
   * @brief return collection of collected license infos
  */
  const std::map<std::string, RteLicenseInfo*>& GetLicensInfos() const { return m_LicensInfos; }

  /**
   * @brief convert collection content to yml-like text
   * @return yml formatted text
  */
  std::string ToString();

protected:
  /**
   * @brief Ensure collection contains RteLicenseInfo object for specified object
   * @param item RteItem* object to add
   * @param license RteItem* pointing to license element, may be NULL
   * @return pointer to RteLicenseInfo object
  */
  RteLicenseInfo* EnsureLicenseInfo(RteItem* item, RteItem* licenseElement);

  std::map<std::string, RteLicenseInfo*> m_LicensInfos;
};

/**
 * @brief class representing project consuming CMSIS RTE data
*/
class RteProject : public RteRootItem
{
public:
  /**
   * @brief default constructor
  */
  RteProject();

  /**
   * @brief destructor
  */
   ~RteProject() override;

public:
  /**
   * @brief clean up the project data
  */
   void Clear() override;

  /**
   * @brief initialize project
  */
  virtual void Initialize();

  /**
   * @brief check if project has been initialized
   * @return true if project has been initialized
  */
  bool IsInitialized() const { return m_bInitialized; }

  /**
   * @brief get project ID
   * @return integer as project ID
  */
  int GetProjectId() const { return m_nID; }

  /**
   * @brief set project ID
   * @param nId project ID to set
  */
  void SetProjectId(int nId) {m_nID = nId;}

  /**
   * @brief get IDs of missing packs
   * @return collection of pack IDs mapped to URLs of missing packs
  */
  const std::map<std::string, std::string>& GetMissingPackIds() const { return t_missingPackIds; }

  /**
   * @brief get targets that have packs missing
   * @return string collection of target names
  */
  const std::set<std::string> GetTargetsMissingPackTargets() const { return t_missingPackTargets; }

  /**
   * @brief clean up collection of missing pack IDs and collection of targets that have missing packs
  */
  void ClearMissingPacks();

  /**
   * @brief set CMSIS RTE data model
   * @param model pointer to RteModel object
  */
  void SetModel(RteModel* model);

  /**
   * @brief get collection of component IDs mapped to pointers of associated RteComponentInstance objects
   * @return collection of component IDs mapped to pointers of associated RteComponentInstance objects
  */
  const std::map<std::string, RteComponentInstance*>& GetComponentInstances() const { return m_components; }

  /**
   * @brief get number of component instances in project
   * @return integer number of component instances
  */
  int GetComponentCount() const { return (int)m_components.size(); }

  /**
   * @brief get number of files in project
   * @return integer number of files
  */
  int GetFileCount() const { return (int)m_files.size(); }

  /**
   * @brief merge file specified by curFile into the one specified by newFile
   * @param curFile source file to merge
   * @param newFile destination file to merge
   * @param originFile a copy of the file used to instantiate the new file initially (optional)
  */
  void MergeFiles(const std::string& curFile, const std::string& newFile, const std::string& originFile = RteUtils::EMPTY_STRING);

public:
  /**
   * @brief set project path
   * @param path project path
  */
  void SetProjectPath(const std::string& path) { m_projectPath = path; }

  /**
   * @brief get project path
   * @return project path string
  */
  const std::string& GetProjectPath() const { return m_projectPath; }

  /**
   * @brief get project name
   * @return project name
  */
   const std::string& GetName() const override { return m_ID; }

  /**
   * @brief set project name
   * @param name project name to set
  */
  void SetName(const std::string& name) { m_ID = name; }

  /**
   * @brief set custom RTE folder name to store config files,
   * @param rteFolder RTE folder name, default "RTE"
  */
  void SetRteFolder(std::string rteFolder) { m_rteFolder = rteFolder; }

  /**
   * @brief get project's RTE folder where config and generated files are stored
   * @return RTE folder name, default "RTE"
  */
  const std::string& GetRteFolder() const override;

  /**
   * @brief get component instance's RTE folder where config and generated files are stored
   * @param ci component instance
   * @return RTE folder name
  */
  const std::string& GetRteFolder(const RteComponentInstance* ci) const;

  /**
   * @brief get RteCallback object
   * @return RteCallback pointer
  */
   RteCallback* GetCallback() const override;

  /**
   * @brief set RteCallback object
   * @param callback RteCallback object to set
  */
  void SetCallback(RteCallback* callback) { m_callback = callback; }

  /**
   * @brief get RteComponentInstance object determined by it's component ID
   * @param id given component ID
   * @return RteComponentInstance pointer
  */
  RteComponentInstance* GetComponentInstance(const std::string& id) const;

  /**
   * @brief get RteFileInstance determined by it's file path
   * @param id given file path
   * @return RteFileInstance pointer
  */
  RteFileInstance* GetFileInstance(const std::string& id) const;

  /**
   * @brief get collection of file paths mapped to associated RteFileInstance objects
   * @return collection of file paths mapped to associated RteFileInstance objects
  */
  const std::map<std::string, RteFileInstance*>& GetFileInstances() const { return m_files; }

  /**
   * @brief get collection of file instances for a given component instance and target
   * @param ci given RteComponentInstance object
   * @param targetName given target name
   * @param configFiles collection to fill: the original file path to RteFileInstance (one entry per multi-instance component),
  */
  void GetFileInstancesForComponent(RteComponentInstance* ci, const std::string& targetName, std::map<std::string, RteFileInstance*>& configFiles) const;

  /**
   * @brief get RteComponentInstanceGroup object
   * @return RteComponentInstanceGroup pointer
  */
    RteComponentInstanceGroup* GetClasses() const { return m_classes; }

  /**
   * @brief get RteComponentInstanceGroup object for specified Cclass
   * @param className class name
   * @return RteComponentInstanceGroup pointer
  */
  RteComponentInstanceGroup* GetClassGroup(const std::string& className) const;

  /**
   * @brief get RteComponentInstanceAggregate object for the specified RteComponentInstance object
   * @param ci given RteComponentInstance object
   * @return RteComponentInstanceAggregate pointer
  */
  RteComponentInstanceAggregate* GetComponentInstanceAggregate(RteComponentInstance* ci) const;

  /**
   * @brief get used components of the specified target
   * @param components collection of component IDs mapped to their associated RteComponent objects
   * @param targetName target name
  */
  void GetUsedComponents(RteComponentMap& components, const std::string& targetName) const;

  /**
   * @brief get used components for the entire project
   * @param components collection of component IDs mapped to their associated RteComponent objects
  */
  void GetUsedComponents(RteComponentMap& components) const;

  /**
   * @brief check if component specified by it's aggregate ID is used in the given target
   * @param aggregateId given aggregate ID
   * @param targetName target name
   * @return true if component is in use
  */
  bool IsComponentUsed(const std::string& aggregateId, const std::string& targetName) const;

  /**
   * @brief check if package given by it's package ID is used in the given target
   * @param packId given package ID
   * @param targetName given target name
   * @param bFullId true to consider package version
   * @return true if package is in use
  */
  bool IsPackageUsed(const std::string& packId, const std::string& targetName, bool bFullId = true) const;

  /**
   * @brief get RtePackageInstanceInfo object
   * @param packId package ID to look for
   * @return RtePackageInstanceInfo pointer
  */
  RtePackageInstanceInfo* GetPackageInfo(const std::string& packId) const;

  /**
   * @brief get RtePackageInstanceInfo object of the latest package specified by it's package ID
   * @param packId given package ID
   * @return RtePackageInstanceInfo pointer
  */
  RtePackageInstanceInfo* GetLatestPackageInfo(const std::string& packId) const;

  /**
   * @brief get effective ID specified by it's package ID for the given target
   * @param packId given package ID
   * @param targetName target name
   * @return full package ID in case of match mode is VersionCmp::FIXED_VERSION, otherwise common ID
  */
  std::string GetEffectivePackageID(const std::string& packId, const std::string& targetName) const;

  /**
   * @brief get filtered packs
   * @return collection of package IDs mapped to RtePackageInstanceInfo pointers
  */
  const std::map<std::string, RtePackageInstanceInfo*>& GetFilteredPacks() const { return m_filteredPackages; }

  /**
   * @brief get packs used in the specified target
   * @param packs collection of package IDs mapped to RtePackage pointers to fill
   * @param targetName target name
  */
  void GetUsedPacks(RtePackageMap& packs, const std::string& targetName) const;

  /**
   * @brief get all packs required in the specified target
   * @param packs collection of package IDs mapped to RtePackage pointers to fill
   * @param targetName target name
  */
  virtual void GetRequiredPacks(RtePackageMap& packs, const std::string& targetName) const;

  /**
   * @brief get collection of RteGpdscInfo objects
   * @return collection of gpdsc files mapped to associated RteGpdscInfo pointers
  */
  const std::map<std::string, RteGpdscInfo*>& GetGpdscInfos() const { return m_gpdscInfos; }

  /**
   * @brief get a RteGpdscInfo object associated with the specified gpdsc file
   * @param gpdscFile given gpdsc file
   * @return RteGpdscInfo pointer
  */
  RteGpdscInfo* GetGpdscInfo(const std::string& gpdscFile);

  /**
   * @brief look for RteGpdscInfo object associated with the given gpdsc file and set the specified
   * RtePackage for it. Create a new one with the given information in case no associated RteGpdscInfo object is found
   * @param gpdscFile given gpdsc file
   * @param gpdscPack given generator RtePackage
   * @return RteGpdscInfo pointer
  */
  RteGpdscInfo* AddGpdscInfo(const std::string& gpdscFile, RtePackage* gpdscPack);

  /**
   * @brief add a new RteGpdscInfo object specific to the given RteComponent and RteTarget object to the project
   * @param c given RteComponent object
   * @param target given RteTarget object
   * @return RteGpdscInfo pointer
  */
  RteGpdscInfo* AddGpdscInfo(RteComponent* c, RteTarget* target);

  /**
   * @brief check if any RteGpdscInfo object exists in the project
   * @return true if any RteGpdscInfo object exists
  */
  bool HasGpdscPacks() const;

  /**
   * @brief check if any RteGpdscInfo is missing its loaded generator pack
   * @return true if any RteGpdscInfo object does not have its RtePackage
  */
  bool HasMissingGpdscPacks() const;

  /**
   * @brief get collection of board display names mapped to RteBoardInfo objects
   * @return collection of board display names mapped to RteBoardInfo objects
  */
  const std::map<std::string, RteBoardInfo*>& GetBoardInfos() const { return m_boardInfos; }

  /**
   * @brief get RteBoardInfo object for the specified board display name
   * @param boardId given board display name
   * @return RteBoardInfo pointer
  */
  RteBoardInfo* GetBoardInfo(const std::string& boardId) const;

  /**
   * @brief get RteBoardInfo for the specified target
   * @param targetName target name
   * @return RteBoardInfo pointer
  */
  RteBoardInfo* GetTargetBoardInfo(const std::string& targetName) const;

  /**
   * @brief set RteBoard object for the specified target
   * @param targetName target name
   * @param board RteBoard object to set
   * @return RteBoardInfo pointer
  */
  RteBoardInfo* SetBoardInfo(const std::string& targetName, RteBoard* board);

  /**
   * @brief create and add a new RteBoardInfo object for the specified target of a cprj project
   * @param target given RteTarget object
   * @param board CprjTargetElement object to set, it represents tag <target> in the associated cprj file
   * @return RteBoardInfo pointer
  */
  RteBoardInfo* CreateBoardInfo(RteTarget* target, CprjTargetElement* board);

public:
  /**
   * @brief check if project has any group matching the specified group name
   * @param group given group name
   * @return true if project has any group matching the specified group name
  */
  bool HasProjectGroup(const std::string& group) const;

  /**
   * @brief check if the specified target has the given group
   * @param group given group name
   * @param target given target name
   * @return true if the specified target has the given group
  */
  bool HasProjectGroup(const std::string& group, const std::string& target) const;

  /**
   * @brief check if the specified group in the specified target is enabled
   * @param group given group
   * @param target given target name
   * @return true if the specified group in the specified target is enabled
  */
  bool IsProjectGroupEnabled(const std::string& group, const std::string& target) const;

  /**
   * @brief check if the specified file is present in the given project group
   * @param group given project group
   * @param file given file
   * @return true if the specified file is contained in the given project group
  */
  bool HasFileInProjectGroup(const std::string& group, const std::string& file) const;

  /**
   * @brief check if the specified file is present in the given group of the given target
   * @param group given group name
   * @param file given file name
   * @param target given target name
   * @return true if the specified file is present in the given group of the given target
  */
  bool HasFileInProjectGroup(const std::string& group, const std::string& file, const std::string& target) const;

  /**
   * @brief get short display name of the specified file in the given group
   * @param group given group
   * @param file given file
   * @return short display name of the specified file
  */
  std::string GetFileComment(const std::string& group, const std::string& file) const;

  /**
   * @brief get associated RteFileInfo of the specified file determined by the given group and target
   * @param groupName given group name
   * @param file given file name
   * @param target given target name
   * @return RteFileInfo pointer
  */
  const RteFileInfo* GetFileInfo(const std::string& groupName, const std::string& file, const std::string& target) const;

  /**
   * @brief collect license info used in project
   * @param collection of license infos
  */
  void CollectLicenseInfos(RteLicenseInfoCollection& licenseInfos) const;

  /**
   * @brief collect license info used in project target
   * @param collection of license infos
   * @param targetName target name to collect info, empty string for active one
  */
  void CollectLicenseInfosForTarget(RteLicenseInfoCollection& licenseInfos, const std::string& targetName) const;


  /**
   * @brief update CMSIS RTE data such as components, boards, gpdsc information, project files in project.
   * @return true if CMSIS RTE data is updated otherwise false
  */
  bool Apply();

  /**
   * @brief update CMSIS RTE data when already used components are changed (e.g. component version or variant)
   * @return true if CMSIS RTE data is updated otherwise false
  */
  bool ApplyInstanceChanges();

  /**
   * @brief update dependencies of components on target and vice versa
  */
  void UpdateModel();

  /**
   * @brief collect settings concerning include files and paths, libraries, pre-include header files and device properties for targets
  */
  void CollectSettings();

  /**
   * @brief try to resolve components (find components for their instances) for the active target
   * @param bFindReplacementForActiveTarget true to look for component with best match
   * @return ConditionResult enumerator
  */
  ConditionResult ResolveComponents(bool bFindReplacementForActiveTarget);

  /**
   * @brief add list of components to the specified target and resolve them
   * @param selItems list of component to add
   * @param target given RteTarget object
   * @param unresolvedComponents (out) collection of component instance that cannot be resolved
  */
  void AddCprjComponents(const Collection<RteItem*>& selItems, RteTarget* target, std::set<RteComponentInstance*>& unresolvedComponents);

  /**
   * @brief update CMSIS RTE data such as components, boards, gpdsc information, project files in project
  */
  void Update();

  /**
   * @brief update component classes
  */
  void UpdateClasses();

  /**
   * @brief generate header files specific to components and build environment
  */
  void GenerateRteHeaders();

  /**
   * @brief set flag indicating modification of list of RteGpdscInfo objects
   * @param bModified true to show that list of RteGpdscInfo objects is modified
  */
  void SetGpdscListModified(bool bModified) { t_bGpdscListModified = bModified; }

  /**
   * @brief check if list of RteGpdscInfo objects is modified
   * @return true if list of RteGpdscInfo objects is modified
  */
  bool IsGpdscListModified() const { return t_bGpdscListModified; }

protected:
  void AddGeneratedComponents();
  void RemoveGeneratedComponents();

  void CategorizeComponentInstance(RteComponentInstance* ci);
  void CollectMissingPacks();
  void CollectMissingPacks(RteItemInstance* inst);
  virtual RteComponentInstance* AddCprjComponent(RteItem* item, RteTarget* target);

public:
  /**
   * @brief get a RteComponentInstance which matches the specified list of component attributes
   * @param componentAttributes list of component attributes to match
   * @return RteComponentInstance pointer
  */
  RteComponentInstance* GetApiInstance(const std::map<std::string, std::string>& componentAttributes) const;

  /**
   * @brief get CMSIS RTE data model specific to this project
   * @return RteModel pointer
  */
   RteModel* GetModel() const override { return m_globalModel; }

  /**
   * @brief get 'this' pointer of the instance
   * @return RteProject pointer
  */
   RteProject* GetProject() const override;

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
     * @brief called to construct the item with attributes and child elements
   */
   void Construct() override;

  /**
   * @brief validates if all required packs, components and APIs are resolved
   * @return true if successful
  */
   bool Validate() override;

public:
  /**
   * @brief get collection of target names mapped to associated RteTarget objects
   * @return collection of target names mapped to associated RteTarget objects
  */
  const std::map<std::string, RteTarget*>& GetTargets() const { return m_targets; }

  /**
   * @brief get target object specified by target name
   * @param targetName given target name
   * @return RteTarget pointer
  */
  RteTarget* GetTarget(const std::string& targetName) const;

  /**
   * @brief add new target with the given name and attributes to the project
   * @param targetName given target name
   * @param attributes given collection of target attributes
   * @param supported flag indicating that target will be considered for further initialization
   * @param bForceFilterComponents true if validation of components should be initiated in case target is supported
   * @return true if target is successfully added
  */
  virtual bool AddTarget(const std::string& targetName, const std::map<std::string, std::string>& attributes,
                         bool supported = true, bool bForceFilterComponents = true);

  /**
   * @brief add the specified target object to the project and additionally update it's data model
   * @param target given target object to add
   * @return true if target is successfully added
  */
  bool AddTarget(RteTarget *target);

  /**
   * @brief remove the specified target from the project
   * @param targetName target to remove
  */
  void RemoveTarget(const std::string& targetName);

  /**
   * @brief rename a target
   * @param oldName name of the target to be renamed
   * @param newName new name of the target
  */
  void RenameTarget(const std::string& oldName, const std::string& newName);

  /**
   * @brief get collection of target names mapped to their associated RteModel objects
   * @return collection of target names mapped to their associated RteModel objects
  */
  const std::map<std::string, RteModel*>& GetTargetModels() const { return m_targetModels; }

  /**
   * @brief get RteModel object for the specified target
   * @param targetName given target name
   * @return RteModel pointer
  */
  RteModel* GetTargetModel(const std::string& targetName) const;

  /**
   * @brief ensure that the specified target owns a RteModel object
   * @param targetName
   * @return
  */
  RteModel* EnsureTargetModel(const std::string& targetName);

  /**
   * @brief ensure that all targets specified in tag <targetInfo> own associated RteModel object
  */
  void CreateTargetModels();

  /**
   * @brief clear all RteTarget objects as well as their associated RteModel objects,
   * update all RteComponentInstance and RtePackageInstanceInfo objects
  */
  void ClearTargets();

  /**
   * @brief get collection of target IDs as integer mapped to associated target names
   * @return collection of target IDs as integer mapped to associated target names
  */
  const std::map<int, std::string>& GetTargetIDs() const { return m_targetIDs; }

  /**
   * @brief set collection of target IDs as integer mapped to associated target names
   * @param targetIDs collection of target IDs as integer mapped to associated target names to set
  */
  void SetTargetIDs(const std::map<int, std::string>& targetIDs) { m_targetIDs = targetIDs; }

  /**
   * @brief get active target name
   * @return target name
  */
  const std::string& GetActiveTargetName() const { return m_sActiveTarget; }

  /**
   * @brief activate the specified target
   * @param targetName target name
   * @return true if active target is changed
  */
  bool SetActiveTarget(const std::string& targetName);

  /**
   * @brief get RteTarget object of the active target
   * @return RteTarget pointer
  */
  RteTarget* GetActiveTarget() const { return GetTarget(m_sActiveTarget); }

  /**
   * @brief validate component dependencies for the specified target or for the currently active one
   * @param target RteTarget object to consider or nullptr to consider currently active target
  */
  void EvaluateComponentDependencies(RteTarget* target);

  /**
   * @brief validate component dependencies of the specified target
   * @param target RteTarget pointer or nullptr for currently active one whose components are to be validated
   * @return true if validation is successful
  */
  bool ResolveDependencies(RteTarget* target);

  /**
   * @brief check if component dependencies of the specified target are fulfilled or ignored
   * @param target RteTarget pointer or nullptr for currently active one whose component dependencies are to be checked
   * @return true if component dependencies are fulfilled or ignored
  */
  bool AreDependenciesResolved(RteTarget* target) const;

  /**
   * @brief filter and validate components for all targets
  */
  void FilterComponents();

  /**
   * @brief clear used components for all targets
  */
  void ClearUsedComponents();

  /**
   * @brief clear all selected components for all targets
  */
  void ClearSelected();

  /**
   * @brief propagate component selection of active target to others
  */
  void PropagateActiveSelectionToAllTargets();

  /**
   * @brief copy file specified by RteFile pointer into the given RteFileInstance object
   * @param fi RteFileInstance object as destination
   * @param f RteFile object as source
   * @param bMerge true to merge source with destination
   * @return true if source can be copied to destination
  */
  bool UpdateFileToNewVersion(RteFileInstance* fi, RteFile* f, bool bMerge);

  /**
   * @brief get file name and path of "RTE_Components.h" determined by the specified target and prefix
   * @param targetName given target name
   * @param prefix given prefix to prep-end to the file path
   * @return string containing file name and path
  */
  std::string GetRteComponentsH(const std::string& targetName, const std::string& prefix) const;

  /**
   * @brief get file name and project relative path of regions*.h determined by the specified target and prefix
   * @param targetName given target name
   * @param prefix given prefix to prep-end to the file path
   * @return string containing file name and path
  */
  std::string GetRegionsHeader(const std::string& targetName, const std::string& prefix) const;

  /**
   * @brief get file name and path locating in folder "RTE" determined by the specified name, target and prefix
   * @param name given file name
   * @param targetName given target name
   * @param prefix given prefix to be added at beginning of the file path
   * @return string containing file name and path
  */
  std::string GetRteHeader(const std::string& name, const std::string & targetName, const std::string& prefix) const;

  /**
   * @brief copy config files to RTE directory if not exist
   * @param targetName
  */
  void WriteInstanceFiles(const std::string& targetName);

  /**
   * @brief check if RTE folder content should be updated with config files
   * @return true if update is enabled
  */
  bool ShouldUpdateRte() const;

 /**
   * @brief update RTE folder content should be updated with config files
  */
  void UpdateRte();


protected:
  virtual RteTarget* CreateTarget(RteModel* filteredModel, const std::string& name, const std::map<std::string, std::string>& attributes);
  void AddTargetInfo(const std::string& targetName);
  bool RemoveTargetInfo(const std::string& targetName);
  bool RenameTargetInfo(const std::string& oldName, const std::string& newName);

  void FilterComponents(RteTarget* target);

  void ClearFilteredPackages();
  void PropagateFilteredPackagesToTargetModels();
  virtual void PropagateFilteredPackagesToTargetModel(const std::string& targetName);
  bool CollectFilteredPackagesFromTargets();

  void ResolvePacks();

protected:
  RteComponentInstance* AddComponent(RteComponent* c, int count, RteTarget* target, RteComponentInstance* oldInstance);
  RteComponentInstance* AddComponent(const std::string& id);
  bool RemoveComponent(const std::string& id);

  void AddComponentFiles(RteComponentInstance* ci, RteTarget* target);
  RteFileInstance* AddFileInstance(RteComponentInstance* ci, RteFile* f, int index, RteTarget* target);
  bool RemoveFileInstance(const std::string& id);
  void DeleteFileInstance(RteFileInstance* fi);
  // initializes or updates (newer version is used) existing file instance
  void InitFileInstance(RteFileInstance* fi, RteFile* f, int index, RteTarget* target, const std::string& savedVersion, const std::string& rteFolder);
  bool UpdateFileInstance(RteFileInstance* fi, RteFile* f, bool bMerge, bool bUpdateComponent);
  void UpdateFileInstanceVersion(RteFileInstance* fi, const std::string& savedVersion);
  void UpdateConfigFileBackups(RteFileInstance* fi, RteFile* f);

  void CollectSettings(const std::string& targetName);

  void ClearClasses();
  void CreateTargetModels(RteItemInstance* instance);

protected:
   void CreateXmlTreeElementContent(XMLTreeElement* parentElement) const override;

protected:
  RteModel* m_globalModel; // global Model
  std::string m_projectPath; // project directory for RTE components
  RteCallback* m_callback;

  std::map<std::string, RteComponentInstance*> m_components; // project components: we can only have unique ones
  std::map<std::string, RteFileInstance*> m_files; // flat std::list of copied and referenced (e.g. DOC) files. Key: instance pathname (to project path for copied ones)

  RteItemInstance* m_packFilterInfos;
  std::map<std::string, RtePackageInstanceInfo*> m_filteredPackages; // packs filters saved in project
  std::map<std::string, RteGpdscInfo*> m_gpdscInfos; // gpdsc packs used in project
  std::map<std::string, RteBoardInfo*> m_boardInfos; // board(s) packs used in project

  RteComponentInstanceGroup* m_classes;

  std::map<std::string, std::string> t_missingPackIds; // std::list of missing packs for all targets
  std::set<std::string> t_missingPackTargets; // names of targets that have missing packs

  int m_nID; // project ID
  bool m_bInitialized;
  bool t_bGpdscListModified;
  std::map<std::string, RteTarget*> m_targets;
  std::map<std::string, RteModel*> m_targetModels; // filtered models for each target
  std::map<int, std::string> m_targetIDs;
  std::string m_sActiveTarget;
  std::string m_rteFolder;

  std::set<RteFile*> m_forcedFiles; // files with a deprecated attr="copy", need to be copied to RTE folder

public:
  static const std::string DEFAULT_RTE_FOLDER;

};

#endif // RteProject_H
