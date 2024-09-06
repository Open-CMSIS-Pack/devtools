#ifndef RteTarget_H
#define RteTarget_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteTarget.h
* @brief CMSIS RTE Data Model filtering
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteFile.h"
#include "RteComponent.h"
#include "RteCondition.h"
#include "RteDevice.h"
#include "RteBoard.h"
#include "RtePackage.h"

class RteComponentInstance;
class RteFileInstance;
class RteBoardInfo;

/**
 * @brief class representing a file specified in component
*/
struct RteFileInfo
{
  /**
   * @brief default constructor
   * @param cat enumerator of type RteFile::Category representing category of file
   * @param ci pointer to an object of type RteComponentInstance
   * @param fi pointer to an object of type RteFileInstance
  */
  RteFileInfo(RteFile::Category cat = RteFile::Category::OTHER, RteComponentInstance* ci = NULL, RteFileInstance* fi = NULL);

  /**
   * @brief check if attribute "attr" has value "config"
   * @return true if attribute "attr" has value "config", otherwise false
  */
  bool IsConfig() const;

  /**
   * @brief compare file version of the given target and the instance
   * @param targetName target name
   * @return 0 if both file versions are same, > 0 if file version of the given target is newer, otherwise < 0
  */
  int HasNewVersion(const std::string& targetName) const;

  /**
   * @brief compare file version of this instance with the ones of other targets
   * @return 0 if equal to all others target, > 0 if file version of any other target is newer, otherwise < 0
  */
  int HasNewVersion() const;

  RteFile::Category m_cat;    //file category
  RteComponentInstance* m_ci; // pointer to an object of type RteComponentInstance
  RteFileInstance* m_fi;      //pointer to an object of type RteFileInstance
};

/**
 * @brief class representing a target
*/
class RteTarget : public RteItem
{
public:
  /**
   * @brief constructor
   * @param parent pointer to an RteItem as parent of this item
   * @param filteredModel pointer to an object of type RteModel
   * @param name name of the target
   * @param attributes list of attributes
  */
  RteTarget(RteItem* parent, RteModel* filteredModel, const std::string& name, const std::map<std::string, std::string>& attributes);

  /**
   * @brief destructor
  */
   ~RteTarget() override;

public:
  /**
   * @brief return ID of this item
   * @return ID of this item as string
  */
   const std::string& GetName() const override { return m_ID; }

  /**
   * @brief setter for ID of this item
   * @param name ID of the item
  */
  void SetName(const std::string& name) { m_ID = name; }

  /**
   * @brief clear the object
  */
   void Clear() override;

  /**
   * @brief return RTE model filtered for this target
   * @return pointer to an RteModel object filtered for this target
  */
  virtual RteModel* GetFilteredModel() const { return m_filteredModel; }

  /**
   * @brief return flag indicating that target is supported
   * @return true if target is supported, otherwise false
  */
  bool IsTargetSupported() const { return m_bTargetSupported; }

  /**
   * @brief setter for flag indicating if target is supported
   * @param supported flag indicating if target is supported
  */
  void SetTargetSupported(bool supported) { m_bTargetSupported = supported; }


  /**
   * @brief expands key sequences ("@L", "%L", etc.) or access sequences in the supplied string.
   * @param str string to expand
   * @param bUseAccessSequences expand access sequences instead of key sequences, default is false
   * @param context pointer to RteItem representing expansion context (optional)
   * @return expanded string
  */
  std::string ExpandString(const std::string& str,
    bool bUseAccessSequences = false, RteItem* context = nullptr) const override;

  /**
   * @brief expand string by replacing $keyword$ with corresponding values
   * @param src source string to expand
   * @return expanded string
  */
  std::string ExpandAccessSequences(const std::string& src) const;

  /**
   * @brief return pointer to a filter object of type RteConditionContext
   * @return pointer to a filter object of type RteConditionContext
  */
  RteConditionContext* GetFilterContext() const { return m_filterContext; }

  /**
   * @brief return the dependency solver used to select a component for the target
   * @return pointer to an object of type RteDependencySolver
  */
  RteDependencySolver* GetDependencySolver() const { return m_dependencySolver; }

  /**
   * @brief return pointer to an object of type RteComponent. The default implementation returns nullptr
   * @return pointer to an object of type RteComponent
  */
   RteComponent* GetComponent() const override { return nullptr;}

  /**
   * @brief determine component identified by a given component ID
   * @param id given ID of component to be returned
   * @return pointer to an object of type RteComponent
  */
  RteComponent* GetComponent(const std::string& id) const;

  /**
   * @brief determine a potential component.
   * Potential components have associated packs which can be selected
   * @param id given ID of component to be determined
   * @return pointer to an object of type RteComponent
  */
  RteComponent* GetPotentialComponent(const std::string& id) const;

  /**
   * @brief determine the potential component of latest version.
   * Potential components have associated packs which can be selected
   * @param id given ID of component to be determined
   * @return pointer to an object of type RteComponent
  */
  RteComponent* GetLatestPotentialComponent(const std::string& id) const;

  /**
   * @brief check if the target has any potential component.
   * Potential components have associated packs which can be selected
   * @return true if target has any potential component
  */
  bool HasPotentialComponents() const { return !m_potentialComponents.empty(); }

  /**
   * @brief check if a component is filtered for the target.
   * Filtered components have associated filtered packs which are available for the target device
   * @param c pointer to an object of type RteComponent
   * @return true if component has been successfully set for the target
  */
  bool IsComponentFiltered(RteComponent* c) const;

  /**
   * @brief clean up lists of components
  */
  void ClearFilteredComponents();

  /**
   * @brief getter for list of filtered components.
   * Filtered components have associated filtered packs which are available for the target device
   * @return list of filtered components
  */
  const RteComponentMap& GetFilteredComponents() const { return m_filteredComponents; }

  /**
   * @brief get collection of filtered bundles
   * @return map of ID to RteBundle pairs
  */
  const RteBundleMap& GetFilteredBundles() const { return m_filteredBundles; }

  /**
   * @brief add list of files associated with the given component
   * @param c component associated with the given files
   * @param files list of files of type
  */
  void AddFilteredFiles(RteComponent* c, const std::set<RteFile*>& files);

  /**
   * @brief getter for list of files associated with a given component
   * @param c given component
   * @return list of files of type RteFile
  */
  const std::set<RteFile*>& GetFilteredFiles(RteComponent* c) const;

  /**
   * @brief determine file given by a file name and an associated component
   * @param name given file name with path
   * @param c given associated component
   * @return pointer of type RteFile
  */
  RteFile* GetFile(const std::string& name, RteComponent* c) const;

  /**
   * @brief determine file given by an file name and an associated component
   * @param fileName given file name without path
   * @param c given associated component
   * @return pointer of type RteFile
  */
  RteFile* FindFile(const std::string& fileName, RteComponent* c) const;

  /**
   * @brief get parent project's RTE folder where config and generated files are stored
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
   * @brief determine file given by instances of type RteFileInstance and RteComponent
   * RTE folder is taken from parent project
   * @param fi given pointer of type RteFileInstance
   * @param c given pointer of type RteComponent
   * @return pointer of type RteFile
  */
  RteFile* GetFile(const RteFileInstance* fi, RteComponent* c) const;

  /**
   * @brief determine file given by instances of type RteFileInstance and RteComponent
   * @param fi given pointer of type RteFileInstance
   * @param c given pointer of type RteComponent
   * @param rteFolder the "RTE" folder path used for placing files
   * @return pointer of type RteFile
  */
  RteFile* GetFile(const RteFileInstance* fi, RteComponent* c, const std::string& rteFolder) const;

  /**
   * @brief evaluate dependencies of selected components of the target
  */
  void EvaluateComponentDependencies();

  /**
   * @brief getter for list of filtered APIs.
   * Filtered APIs have associated filtered packs which are available for the target device
   * @return list of APIs mapped to pointers of associated RteApi
  */
  const std::map<std::string, RteApi*>& GetFilteredApis() const { return m_filteredApis; }

  /**
   * @brief getter for RteApi instance determined by a list of attributes of a component
   * @param componentAttributes list of attributes of a component
   * @return pointer to an instance of type RteApi
  */
  RteApi* GetApi(const std::map<std::string, std::string>& componentAttributes) const;

  /**
   * @brief getter for RteApi instance determined by an api ID
   * @param id ID of api
   * @return pointer to an instance of type RteApi
  */
  RteApi* GetApi(const std::string& id) const;

  /**
   * @brief getter for a list of components which match a given RteApi instance
   * @param api given api to look for in components
   * @param components list of components to be filled
   * @param selectedOnly true to consider only selected components
   * @return status of component dependency of type ConditionResult
  */
  ConditionResult GetComponentsForApi(RteApi* api, std::set<RteComponent*>& components, bool selectedOnly) const;

  /**
   * @brief getter for a list of components which match the given list of component attributes
   * @param componentAttributes given list of component attributes
   * @param components list of components to be filled
   * @return status of component dependency of type ConditionResult
  */
  ConditionResult GetComponents(const std::map<std::string, std::string>& componentAttributes, std::set<RteComponent*>& components) const;

  /**
   * @brief getter for a collection of RteComponentAggregates which match the given component attributes
   * @param componentAttributes given component attributes
   * @param aggregates collection of RteComponentAggregates to be filled
   * @return status of component dependency of type ConditionResult
  */
  ConditionResult GetComponentAggregates(const XmlItem& componentAttributes, std::set<RteComponentAggregate*>& aggregates) const;

  /**
   * @brief select a component for the target
   * @param a pointer to RteComponentAggregate to be selected
   * @param count number of instance to select: can be more than 1 for a multi-instance component
   * @param bUpdateDependencies true to trigger evaluation of dependencies for the target
   * @param bUpdateBundle true to update bundle name
   * @return true if selection is changed
  */
  bool SelectComponent(RteComponentAggregate* a, int count, bool bUpdateDependencies, bool bUpdateBundle = false);

  /**
   * @brief select a component for the target
   * @param c component to select
   * @param count number of instances to select: can be more than 1 for a multi-instance component
   * @param evaluateDependencies true to trigger evaluation of dependencies for the target
   * @param bUpdateBundle true to update bundle name
   * @return true if selection is changed
  */
  bool SelectComponent(RteComponent* c, int count, bool evaluateDependencies, bool bUpdateBundle = false);

  /**
   * @brief clear all components selected for the target
  */
  void ClearSelectedComponents();

  /**
   * @brief check number of selected instances for a given component
   * @param c given component
   * @return number of selected instances
  */
  int IsSelected(RteComponent* c) const;

  /**
   * @brief check number of selected instances for a given component
   * @param c given component
   * @return number of selected instances
  */
  int IsComponentSelected(RteComponent* c) const;

  /**
   * @brief check if api is selected for a given component
   * @param a given api
   * @return 1 if api is selected otherwise 0
  */
  int IsApiSelected(RteApi* a) const;

  /**
   * @brief check if target components selected
   * @return always 1
  */
  virtual int IsSelected() const;

  /**
   * @brief return an instance of type RtePackageFilter
   * @return instance of type RtePackageFilter
  */
  const RtePackageFilter& GetPackageFilter() const;

  /**
   * @brief return an instance of type RtePackageFilter
   * @return instance of type RtePackageFilter
  */
  RtePackageFilter& GetPackageFilter();

  /**
   * @brief setter for package filter of type RtePackageFilter
   * @param filter package filter of type RtePackageFilter to set
  */
  void SetPackageFilter(const RtePackageFilter& filter);

  /**
   * @brief update filtered RTE data model and components
  */
  void UpdateFilterModel();

  /**
   * @brief setter for component instance of type RteComponentInstance
   * @param c pointer to given component instance of type RteComponentInstance to set
   * @param count number of instance to set
  */
  void SetComponentUsed(RteComponentInstance* c, int count);

  /**
   * @brief clear unresolved used components
  */
  void ClearUsedComponents();

  /**
   * @brief getter of number of used instances for a given component
   * @param c pointer to given component
   * @return number of used instances
  */
  int IsComponentUsed(RteComponent* c) const;

  /**
   * @brief getter for instance of type RteComponentInstance determined by a given component
   * @param c pointer to given component
   * @return pointer to instance of type RteComponentInstance
  */
  RteComponentInstance* GetUsedComponentInstance(RteComponent* c) const;

  /**
   * @brief getter for instance of type RteComponentInstance determined by a component file
   * @param filePath file to be matched
   * @return pointer to instance of type RteComponentInstance
  */
  RteComponentInstance* GetComponentInstanceForFile(const std::string& filePath) const;

  /**
   * @brief add given instance of RteComponentInstance mapped to given file
   * @param filePath given file
   * @param ci given instance of RteComponentInstance to add
  */
  void AddComponentInstanceForFile(const std::string& filePath, RteComponentInstance* ci);

  /**
   * @brief set component selection given by another RteTarget
   * @param otherTarget given target whose component selection is to be retrieved
  */
  void SetSelectionFromTarget(RteTarget* otherTarget);

  // classes and aggregates

  /**
   * @brief getter for component class container of type RteComponentClassContainer
   * @return pointer to RteComponentClassContainer
  */
  RteComponentClassContainer* GetClasses() const { return m_classes; }

  /**
   * @brief getter for an instance of type RteComponentClass given by its class name
   * @param name class name
   * @return pointer to instance of type RteComponentClass
  */
  RteComponentClass* GetComponentClass(const std::string& name) const;

  /**
   * @brief getter for an instance of type RteComponentGroup given by a component
   * @param c pointer to given component
   * @return pointer to instance of type RteComponentGroup
  */
  RteComponentGroup* GetComponentGroup(RteComponent* c) const;

  /**
   * @brief  getter for an instance of type RteComponentAggregate given by a component
   * @param c pointer to given component
   * @return pointer to instance of type RteComponentAggregate
  */
  RteComponentAggregate* GetComponentAggregate(RteComponent* c) const;

  /**
   * @brief getter for an instance of type RteComponentAggregate given by an ID
   * @param id given ID
   * @return pointer to instance of type RteComponentAggregate
  */
  RteComponentAggregate* GetComponentAggregate(const std::string& id) const;

  /**
   * @brief find instance of type RteComponentAggregate given by an instance of type RteComponentInstance
   * @param ci given pointer to RteComponentInstance object
   * @return pointer to an instance of type RteComponentAggregate
  */
  RteComponentAggregate* FindComponentAggregate(RteComponentInstance* ci) const;

  /**
   * @brief getter for latest component given by an instance of type RteComponentInstance
   * @param ci pointer to given RteComponentInstance object
   * @return pointer to instance of type RteComponent
  */
  RteComponent* GetLatestComponent(RteComponentInstance* ci) const;

  /**
   * @brief determine a component given by an instance of type RteComponentInstance
   * @param ci pointer to instance of type RteComponentInstance
   * @return pointer to RteComponent object
  */
  RteComponent* ResolveComponent(RteComponentInstance* ci) const;

  /**
   * @brief getter for a potential component given by an instance of type RteComponentInstance
   * @param ci pointer to a RteComponentInstance object
   * @return pointer to RteComponent object
  */
  RteComponent* GetPotentialComponent(RteComponentInstance* ci) const;

  /**
   * @brief getter for device startup component
   * @return pointer to RteComponent object
  */
  RteComponent* GetDeviceStartupComponent() const { return m_deviceStartupComponent; }

  /**
   * @brief getter for latest component which has the component ID "ARM::CMSIS.CORE"
   * @return pointer to RteComponent object
  */
  RteComponent* GetCMSISCoreComponent() const;

  /**
   * @brief getter for include path given by the component ID "ARM::CMSIS.CORE"
   * @return include path string
  */
  std::string GetCMSISCoreIncludePath() const;

  /**
   * @brief collect dependency results of selected components for the given target in consideration of api conflicts
   * @param results collection of dependency results to be filled with
   * @param target pointer to given target
   * @return enumerator of type ConditionResult
  */
   ConditionResult GetDepsResult(std::map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const override;

  /**
   * @brief collect dependency results of selected components for the given target
   * @param results collection of dependency results to be filled with
   * @param target pointer to given target
   * @return enumerator of type ConditionResult
  */
  ConditionResult GetSelectedDepsResult(std::map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const;

  /**
   * @brief getter for selected component aggregates of type RteComponentAggregate
   * @return collection of RteComponentAggregate object pointer mapped to number of instances
  */
  const std::map<RteComponentAggregate*, int>& GetSelectedComponentAggregates() const { return m_selectedAggregates; }

  /**
   * @brief collect selected component aggregates of type RteComponentAggregate
   * @return collection of RteComponentAggregate object pointer mapped to number of instances
  */
  const std::map<RteComponentAggregate*, int>& CollectSelectedComponentAggregates();

  /**
   * @brief collect deselected component aggregates
   * @param unSelectedGpdscAggregates collection of type RteComponentAggregate to be filled
  */
  void GetUnSelectedGpdscAggregates(std::set<RteComponentAggregate*>& unSelectedGpdscAggregates) const;

  /**
   * @brief collect files referenced in selected components
  */
  void CollectFilteredFiles();

  /**
   * @brief get device folder relative to RTE folder
   * @return string containing folder
  */
  std::string GetDeviceFolder() const;

  /**
   * @brief get file name regions*.h relative to RTE folder
   * @return string containing file name
  */
  std::string GetRegionsHeader() const;

  /**
   * @brief generate region*.h header file according to selected device and board
   * @param directory destination directory to write the header to
   * @return true if generation of the header file is successful
  */
  bool GenerateRegionsHeader(const std::string& directory = EMPTY_STRING);

  /**
   * @brief generate header files specific to selected components, e.g. Pre_Include_Global.h, RTE_Components.h
   * @return true if generation of header files is successful
  */
  bool GenerateRteHeaders();

protected:
  void CollectSelectedComponentAggregates(std::map<RteComponentAggregate*, int>& selectedAggregates) const;
  ConditionResult GetComponentsForApi(RteApi* api, const std::map<std::string, std::string>& componentAttributes, std::set<RteComponent*>& components, bool selectedOnly) const;
  static void GetSpecificBundledClasses(const std::map<RteComponentAggregate*, int>& aggregates, std::map<std::string, std::string>& specificClasses);

  void FilterComponents();
  std::string GenerateRegionsHeaderContent() const;
  std::string GenerateMemoryRegionContent(const std::vector<RteItem*> memVec, const std::string& id, const std::string& dfp) const;
  std::pair<std::string, std::string> GetAccessAttributes(RteItem* mem) const;
  bool GenerateRTEComponentsH();
  bool GenerateRteHeaderFile(const std::string& headerName, const std::string& content,
                              bool bRegionsHeader = false, const std::string& directory = EMPTY_STRING);

  // instance operations
public:
  /**
   * @brief clear different collections containing e.g. include paths, project groups, defines, etc.
  */
  void ClearCollections();

  /**
   * @brief collect settings of given component instance
   * @param ci pointer to RteComponentInstance object
  */
  void CollectComponentSettings(RteComponentInstance* ci);

  /**
   * @brief collect documentation files from component groups
  */
  void CollectClassDocs();

  /**
   * @brief add file instance given by RteFileInstance object to project group
   * @param fi pointer to RteFileInstance object
  */
  void AddFileInstance(RteFileInstance* fi);


  /**
   * @brief add file to project group based on instances of type RteFile and RteComponentInstance
   * @param f pointer to RteFile object
   * @param ci pointer to RteComponentInstance object
  */
  void AddFile(RteFile* f, RteComponentInstance* ci);

  /**
   * @brief add file to a destination determined by RteFile::Category
   * @param pathName file name with path
   * @param cat file category
   * @param comment file-specific comment to add
   * @param c pointer to RteComponent object for files specific to component
   * @param f pointer to RteFile object for its additional properties
  */
  void AddFile(const std::string& pathName, RteFile::Category cat, const std::string& comment, RteComponent* c = 0, RteFile* f = 0);

  /**
   * @brief add an include path to the target
   * @param path include path to add
   * @param language supported language, supply LANGUAGE_NONE to add the include path to any language
  */
  void AddIncludePath(const std::string& path, RteFile::Language language);

  /**
   * @brief add an include path to the target for given component
   * @param path include path to add
   * @param c given component
   * @param language supported language, supply LANGUAGE_NONE if no language specified
  */
  void AddPrivateIncludePath(const std::string& path, RteComponent* c, RteFile::Language language);

  /**
   * @brief add a pre-include file specific to the given component to the target
   * @param pathName pre include file
   * @param c given component
  */
  void AddPreIncludeFile(const std::string& pathName, RteComponent* c);

  /**
   * @brief add a project group to the project
   * @param groupName name of the project group
  */
  void AddProjectGroup(const std::string& groupName);

  /**
   * @brief check if the given group exists in the project
   * @param group name of the group
   * @return true if the given group exists in the project
  */
  bool HasProjectGroup(const std::string& group) const;

  /**
   * @brief check if the given file exists in the project group
   * @param group name of the given project group
   * @param file name of the given file
   * @return true if the given file exists in the project group
  */
  bool HasFileInProjectGroup(const std::string& group, const std::string& file) const;

  /**
   * @brief determine display name of the given file in the given group
   * @param groupName name of the given project group
   * @param file name of the given file
   * @return display name of the file
  */
  std::string GetFileComment(const std::string& groupName, const std::string& file) const;

  /**
   * @brief getter for RteFileInfo object associated with the given file in the given group
   * @param groupName name of the given group
   * @param file name of the given file
   * @return pointer to RteFileInfo object
  */
  const RteFileInfo* GetFileInfo(const std::string& groupName, const std::string& file) const;

  /**
   * @brief getter for a collection of pre-include files associated with the given file in the given group
   * @param groupName name of the given group
   * @param file name of the given file
   * @return collection of pre-include files
  */
  const std::set<std::string>& GetLocalPreIncludes(const std::string& groupName, const std::string& file) const;

  /**
   * @brief getter for project groups
   * @return collection of group names mapped to RteFileInfo objects
  */
  const std::map<std::string, std::map<std::string, RteFileInfo> >& GetProjectGroups() const { return m_projectGroups; }

  /**
   * @brief getter for files of a given project group
   * @param groupName name of the given group
   * @return collection of file names mapped to RteFileInfo objects
  */
  const std::map<std::string, RteFileInfo>& GetFilesInProjectGroup(const std::string& groupName) const;

  /**
   * @brief get global include paths for specified language
   * @param language specific language, default is RteFile::Language::LANGUAGE_NONE to return all non-language specific include paths
   * @return collection of include path strings
  */
  const std::set<std::string>& GetIncludePaths(RteFile::Language language = RteFile::Language::LANGUAGE_NONE) const;

  /**
   * @brief get global effective include paths for specified language, for example combined "c"', "c-cpp" and non-language specific
   * @param language specific language, specifying Language::LANGUAGE_NONE will return the same paths as GetIncludePaths()
   * @param includePaths reference to a collection of strings to obtain a result
   * @param c an optional component to obtain collection of include paths seen by a component (private + global)
   * @return reference to the collection of strings supplied by includePaths argument
  */
  std::set<std::string>& GetEffectiveIncludePaths(std::set<std::string>& includePaths, RteFile::Language language, RteComponent* c = nullptr) const;

  /**
   * @brief getter for a collection of includes
   * @return collection of includes
  */
  const std::map<std::string, std::string>& GetHeaders() const { return m_headers; }

  /**
   * @brief getter for private include paths specific to the given component and language
   * @param c given component, if nullptr, global include paths are returned, equivalent to GetIncludePaths()
   * @param language specific language, default is RteFile::Language::LANGUAGE_NONE to return all non-language specific include paths
   * @return string collection of private include paths
  */
  const std::set<std::string>& GetPrivateIncludePaths(RteComponent* c, RteFile::Language language = RteFile::Language::LANGUAGE_NONE) const;

  /**
   * @brief getter for effective include paths for given component for specified language, not combined with global paths
   * @param c given component, if nullptr, global include paths are returned, equivalent to GetEffectiveIncludePaths()
   * @param language specific language
   * @param includePaths reference to a collection of strings to obtain a result
   * @return reference to the collection of strings supplied by includePaths argument
  */
  std::set<std::string>& GetEffectivePrivateIncludePaths(std::set<std::string>& includePaths, RteComponent* c, RteFile::Language language) const;

  /**
   * @brief getter for collection of pre-include files
   * @return string collection of pre-include files
  */
  const std::map<RteComponent*, std::set<std::string> >& GetPreIncludeFiles() const { return m_PreIncludeFiles; }

  /**
   * @brief getter for pre-include files specific to the given component
   * @param c given component
   * @return string collection of pre-include files
  */
  const std::set<std::string>& GetPreIncludeFiles(RteComponent* c) const;

  /**
   * @brief getter for device header file without path
   * @return device header file
  */
  const std::string& GetDeviceHeader() const { return m_deviceHeader; }

  /**
   * @brief getter for collection of libraries
   * @return string collection of libraries
  */
  const std::set<std::string>& GetLibraries() const { return m_libraries; }

  /**
   * @brief getter for collection of library source paths
   * @return string collection of library source paths
  */
  const std::set<std::string>& GetLibrarySourcePaths() const { return m_librarySourcePaths; }

  /**
   * @brief getter for collection of object files
   * @return string collection of object files
  */
  const std::set<std::string>& GetObjects() const { return m_objects; }

  /**
   * @brief getter for part of pre-include header file specific to component
   * @return collection of part of local pre-include header file
  */
  const std::set<std::string>& GetRteComponentHstrings() const { return m_RTE_Component_h; }

  /**
   * @brief getter for part of global pre-include header file
   * @return string collection of part of global pre-include header file
  */
  const std::set<std::string>& GetGlobalPreIncludeStrings() const { return m_PreIncludeGlobal; }

  /**
   * @brief getter for components mapped to pre-include strings
   * @return string collection of components mapped to pre-include strings
  */
  const std::map<RteComponent*, std::string>& GetLocalPreIncludeStrings() const { return m_PreIncludeLocal; }

  /**
   * @brief getter for component document files
   * @return string collection of component document files
  */
  const std::set<std::string>& GetDocs() const { return m_docs; }

  /**
   * @brief getter for component viewer description files mapped to associated components
   * @return string collection of component viewer description files mapped to associated components
  */
  const std::map<std::string, RteComponent*>& GetScvdFiles() const { return m_scvdFiles; }

  /**
   * @brief getter for attribute value of "Dvendor"
   * @return attribute value of "Dvendor"
  */
   const std::string& GetVendorString() const override;

  /**
   * @brief set device and attribute "Dcore"
  */
   void ProcessAttributes() override;

  /**
   * @brief add properties specific to the given device
   * @param device pointer to given RteDeviceItem object
   * @param processorName name of processor
  */
  void AddDeviceProperties(RteDeviceItem* device, const std::string& processorName);

  /**
   * @brief getter for files that have attribute "template"
   * @return collection of RteComponent pointers mapped to RteFileTemplateCollection pointers
  */
  const std::map<RteComponent*, RteFileTemplateCollection*>& GetAvailableTemplates() const { return m_availableTemplates; }

  /**
   * @brief getter for template files specific to the given component
   * @param c pointer to given component
   * @return RteFileTemplateCollection pointer
  */
  RteFileTemplateCollection* GetTemplateCollection(RteComponent* c) const;

  /**
   * @brief getter for defines
   * @return string collection of defines
  */
  const std::set<std::string>& GetDefines() const { return m_defines; }

  /**
   * @brief insert a new define into the list of defines
   * @param define #define string to add
  */
  void InsertDefine(const std::string& define) { m_defines.insert(define); }

  /**
   * @brief getter for flash algorithm files
   * @return string collection of flash algorithm files
  */
  const std::set<std::string>& GetFlashAlgos() const { return m_algos; }

  /**
   * @brief getter for system view description file
   * @return system view description file
  */
  const std::string& GetSvdFile() const { return m_svd; }

  /**
   * @brief getter for RteDeviceItem object
   * @return pointer to RteDeviceItem object
  */
  RteDeviceItem* GetDevice() const { return m_device; }

  /**
   * @brief getter for CMSIS pack containing selected device
   * @return pointer to RtePackage object
  */
  RtePackage* GetDevicePackage() const { return m_device ? m_device->GetPackage() : nullptr; }

  /**
   * @brief getter for list of boards compatible with target's device
   * @param boards collection of RteBoad to fill
  */
  void GetBoards(std::vector<RteBoard*>& boards) const;

  /**
   * @brief find board given by board name
   * @param displayName name of board
   * @return pointer to RteBoard object
  */
  RteBoard* FindBoard(const std::string& displayName) const;

  /**
   * @brief getter for board information
   * @return RteBoardInfo pointer
  */
  RteBoardInfo* GetBoardInfo() const;

  /**
   * @brief getter for selected board
   * @return RteBoard pointer
  */
  RteBoard* GetBoard() const;

  /**
   * @brief getter for CMSIS pack associated with selected board
   * @return RtePackage pointer
  */
  RtePackage* GetBoardPackage() const;

  /**
   * @brief set a board given by RteBoard object
   * @param board given RteBoard pointer
  */
  void SetBoard(RteBoard* board);

  /**
   * @brief getter for CMSIS pack containing selected device
   * @return RtePackage pointer
  */
  RtePackage* GetEffectiveDevicePackage() const { return GetDevicePackage(); }

  /**
   * @brief get <environment> property of device with name equals to "uv"
   * @return RteDeviceProperty pointer
  */
  RteDeviceProperty* GetDeviceEnvironment() const { return m_deviceEnvironment; }

  /**
   * @brief get the absolute path to the generator input file
   * @return absolute path to the generator input file
  */
  const std::string& GetGeneratorInputFile() const { return m_generatorInputFile; }

  /**
   * @brief set the generator input file path
   * @param newGeneratorInputFile the new absolute path to the generator input file
  */
  void SetGeneratorInputFile(const std::string& newGeneratorInputFile) { m_generatorInputFile = newGeneratorInputFile; }

  /**
   * @brief get <environment> property of device with given name
   * @param tag given name
   * @return device environment string
  */
  const std::string& GetDeviceEnvironmentString(const std::string& tag) const;

  /**
   * @brief getter for all files of the target
   * @return collection of RteComponent pointers mapped to RteFile pointers
  */
  const std::map<RteComponent*, std::set<RteFile*> >& GetAllFilteredFiles() const { return m_filteredFiles; }

  /**
   * @brief getter for collection of missing packs for the target
   * @return collection of pack IDs mapped to URLs of missing packs
  */
  const std::map<std::string, std::string>& GetMissingPackIds() const { return t_missingPackIds; }

  /**
   * @brief clear collection of missing packs of the target
  */
  void ClearMissingPacks();

  /**
   * @brief add missing pack to collection of missing packs for the target
   * @param pack full pack ID
   * @param url URL of the missing pack
  */
  void AddMissingPackId(const std::string& pack, const std::string& url);

  /**
   * @brief check if pack is missing for the target
   * @param pack pack to check
   * @return true if pack is missing
  */
  bool IsPackMissing(const std::string& pack);

  /**
   * @brief getter for the gpdsc file names
   * @return string collection of gpdsc file names
  */
  const std::set<std::string>& GetGpdscFileNames() const { return m_gpdscFileNames; }

  /**
   * @brief check if gpdsc is used
   * @param gpdsc name if gpdsc file
   * @return true if gpdsc file is used
  */
  bool IsGpdscUsed(const std::string& gpdsc) const { return m_gpdscFileNames.find(gpdsc) != m_gpdscFileNames.end(); }

protected:
  /**
   * @brief add supplied component to the list of filtered components
   * @param c pointer to RteComponent
  */
  virtual void AddFilteredComponent(RteComponent* c);
  /**
   * @brief adds components to the list of available components from suppled container
   * @param parentContainer pointer to RteItem representing <components> or <bundle> elements
   * @return pointer to device startup component if found in the container, nullptr otherwise
  */
  virtual RteComponent* AddFilteredComponents(RteItem* parentContainer);

  virtual void AddPotentialComponent(RteComponent* c);
  virtual void CategorizeComponent(RteComponent* c);
  virtual void CategorizeComponentInstance(RteComponentInstance* ci, int count);

 /**
  * @brief add an include path to the target for given component
  * @param path include path to add
  * @param c given component or nullptr to add to the global collection
  * @param language supported language, supply RteFile::Language::LANGUAGE_NONE if no language specified
 */
  void InternalAddIncludePath(const std::string& path, RteComponent* c, RteFile::Language language);

  void UpdateSelectedAggregates(RteComponentAggregate* a, int count);

  void CollectPreIncludeStrings(RteComponent* c, int count);

  void AddBoadProperties(RteDeviceItem* device, const std::string& processorName);
  void AddAlgorithm(RteItem* algo, RteItem* holder);

  std::string NormalizeIncPath(const std::string& path) const;
  std::string ReplaceProjectPathWithDotSlash(const std::string& path) const;

  // model data
protected:
  RteModel* m_filteredModel;

  bool m_bTargetSupported; // target is supported by RTE, can only be defined from outside
  RteComponentMap m_filteredComponents; // components filtered for this target
  RteComponentMap m_potentialComponents; // components filtered for this target regardless pack filter
  RteBundleMap m_filteredBundles; // collection of bundles with at least one filtered component

  std::map<std::string, RteApi* > m_filteredApis;
  RteComponentClassContainer* m_classes; // contains only filtered components

  std::map<RteComponent*, std::set<RteFile*> > m_filteredFiles;

  RteConditionContext* m_filterContext;
  RteDependencySolver* m_dependencySolver;

  RtePackage* m_effectiveDevicePackage;

  // instance data
  std::map<RteComponentAggregate*, int> m_selectedAggregates;
  std::map<std::string, std::string> t_missingPackIds; // list of missing packs for this target

  std::map<std::string, std::map<std::string, RteFileInfo> > m_projectGroups; // <group name ,<filepath, comment> >
  std::map<std::string, RteComponentInstance*> m_fileToComponentInstanceMap; // <filepath, component instance>
  // all include paths, component specific if RteComponet != 0, global otherwise
  // global collection contains device includes
  std::map<RteComponent*, std::map < RteFile::Language, std::set<std::string> > > m_includePaths;

  std::map<std::string, std::string> m_headers; // also contains device includes, the second is a comment (originating component or text), without path
  std::map<RteComponent*, std::set<std::string> > m_PreIncludeFiles; // global (key == NULL) and local (key = component ) pre-includes
  std::string m_deviceHeader; // device header filename without path (also put into m_headers)
  std::set<std::string> m_librarySourcePaths;
  std::set<std::string> m_libraries; // N.B: libs are added to project directly, here they are only for quick access
  std::set<std::string> m_objects;
  std::set<std::string> m_docs;
  std::map<std::string, RteComponent*> m_scvdFiles; // component viewer description files
  std::string m_generatorInputFile; // Absolute path to the generator input file

  // header file content
  std::set<std::string> m_RTE_Component_h; // defines put into the file
  std::set<std::string> m_PreIncludeGlobal; // defines put into the global pre-include file
  std::map<RteComponent*, std::string> m_PreIncludeLocal; // defines put into the local pre-include file component->pre-include content

  std::set<std::string> m_gpdscFileNames;

  RteComponent* m_deviceStartupComponent; // device startup component being used
  RteDeviceItem* m_device; // device used by target
  // environment
  RteDeviceProperty* m_deviceEnvironment; // device environment property for "uv"

  // template support
  std::map<RteComponent*, RteFileTemplateCollection*> m_availableTemplates;
  // device values:
  std::set<std::string> m_defines; // device defines
  std::set<std::string> m_algos; // flash algorithms
  std::string m_svd; // svd file

  bool m_bDestroy; // destroy flag to prevent updates
};

#endif // RteTarget_H
