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

#include <stdio.h>

class RteModel;
class RteGeneratorModel;
class RteComponent;
class RteComponentAggregate;
class RteComponentInstance;
class RteFileInstance;
struct RteFileInfo;
class RteGenerator;
class RteTarget;
class CprjTargetElement;
class RteProject : public RteItem
{
public:
  RteProject();
  virtual ~RteProject() override;

public:
  virtual void Clear() override;
  virtual void Initialize();
  bool IsInitialized() const { return m_bInitialized; }

  int GetProjectId() const { return m_nID; }
  void SetProjectId(int nId) {m_nID = nId;}

  const std::map<std::string, std::string>& GetMissingPackIds() const { return t_missingPackIds; }
  const std::set<std::string> GetTargetsMissingPackTargets() const { return t_missingPackTargets; }
  void ClearMissingPacks();

  void SetModel(RteModel* model);

  const std::map<std::string, RteComponentInstance*>& GetComponentInstances() const { return m_components; }
  int GetComponentCount() const { return (int)m_components.size(); }
  int GetFileCount() const { return (int)m_files.size(); }

  void MergeFiles(const std::string& curFile, const std::string& newFile);

public:
  void SetProjectPath(const std::string& path) { m_projectPath = path; }
  const std::string& GetProjectPath() const { return m_projectPath; }

  virtual const std::string& GetName() const override { return m_ID; }
  void SetName(const std::string& name) { m_ID = name; }

  virtual RteCallback* GetCallback() const override;
  void SetCallback(RteCallback* callback) { m_callback = callback; }

  RteComponentInstance* GetComponentInstance(const std::string& id) const;
  RteFileInstance* GetFileInstance(const std::string& id) const;

  const std::map<std::string, RteFileInstance*>& GetFileInstances() const { return m_files; }
  void GetFileInstances(RteComponentInstance* ci, const std::string& targetName, std::map<std::string, RteFileInstance*>& configFiles) const;

  RteComponentInstanceGroup* GetClasses() const { return m_classes; }
  RteComponentInstanceGroup* GetClassGroup(const std::string& className) const;
  RteComponentInstanceAggregate* GetComponentInstanceAggregate(RteComponentInstance* ci) const;

  void GetUsedComponents(RteComponentMap& components, const std::string& targetName) const; // returns used components for specified target
  void GetUsedComponents(RteComponentMap& components) const; // returns used components for the entire project

  bool IsComponentUsed(const std::string& aggregateId, const std::string& targetName) const;
  bool IsPackageUsed(const std::string& packId, const std::string& targetName, bool bFullId = true) const;
  RtePackageInstanceInfo* GetPackageInfo(const std::string& packId) const;
  RtePackageInstanceInfo* GetLatestPackageInfo(const std::string& packId) const;
  std::string GetEffectivePackageID(const std::string& packId, const std::string& targetName) const; // returns full or common id depending on the mode

  const std::map<std::string, RtePackageInstanceInfo*>& GetFilteredPacks() const { return m_filteredPackages; }
  void GetUsedPacks(RtePackageMap& packs, const std::string& targetName) const; // returns used packs if components are resolved

  const std::map<std::string, RteGpdscInfo*>& GetGpdscInfos() const { return m_gpdscInfos; }
  RteGpdscInfo* GetGpdscInfo(const std::string& gpdscFile);
  RteGpdscInfo* AddGpdscInfo(const std::string& gpdscFile, RteGeneratorModel* model);
  RteGpdscInfo* AddGpdscInfo(RteComponent* c, RteTarget* target);


  bool HasGpdscModels() const;
  bool HasMissingGpdscModels() const;
  RteGeneratorModel* GetGpdscModelForTaxonomy(const std::string& taxonomyID);

  const std::map<std::string, RteBoardInfo*>& GetBoardInfos() const { return m_boardInfos; }
  RteBoardInfo* GetBoardInfo(const std::string& boardId) const;
  RteBoardInfo* GetTargetBoardInfo(const std::string& targetName) const;
  RteBoardInfo* SetBoardInfo(const std::string& targetName, RteBoard* board);
  RteBoardInfo* CreateBoardInfo(RteTarget* target, CprjTargetElement* board);

public:
  bool HasProjectGroup(const std::string& group) const;
  bool HasProjectGroup(const std::string& group, const std::string& target) const;
  bool IsProjectGroupEnabled(const std::string& group, const std::string& target) const;

  bool HasFileInProjectGroup(const std::string& group, const std::string& file) const;
  bool HasFileInProjectGroup(const std::string& group, const std::string& file, const std::string& target) const;
  std::string GetFileComment(const std::string& group, const std::string& file) const;
  const RteFileInfo* GetFileInfo(const std::string& groupName, const std::string& file, const std::string& target) const;

public:
  bool Apply();
  bool ApplyInstanceChanges();
  void UpdateModel();
  void CollectSettings();
  ConditionResult ResolveComponents(bool bFindReplacementForActiveTarget);

  void AddCprjComponents(const std::list<RteItem*>& selItems, RteTarget* target, std::set<RteComponentInstance*>& unresolvedComponents);
  void Update();
  void UpdateClasses();

  void GenerateRteHeaders();

  void SetGpdscListModified(bool bModified) { t_bGpdscListModified = bModified; }
  bool IsGpdscListModified() const { return t_bGpdscListModified; }

protected:
  void AddGeneratedComponents();
  void RemoveGeneratedComponents();

  void CategorizeComponentInstance(RteComponentInstance* ci);
  void CollectMissingPacks();
  void CollectMissingPacks(RteItemInstance* inst);
  RteComponentInstance* AddCprjComponent(RteItem* item, RteTarget* target);

public:
  RteComponentInstance* GetApiInstance(const std::map<std::string, std::string>& componentAttributes) const;
  virtual RteModel* GetModel() const override { return m_globalModel; }
  virtual RteProject* GetProject() const override;
  virtual bool Construct(XMLTreeElement* xmlElement) override;
  virtual bool Validate() override;

public: // targets
  const std::map<std::string, RteTarget*>& GetTargets() const { return m_targets; }
  RteTarget* GetTarget(const std::string& targetName) const;
  virtual bool AddTarget(const std::string& targetName, const std::map<std::string, std::string>& attributes,
                         bool supported = true, bool bForceFilterComponents = true);
  bool AddTarget(RteTarget *target);
  void RemoveTarget(const std::string& targetName);
  void RenameTarget(const std::string& oldName, const std::string& newName);
  const std::map<std::string, RteModel*>& GetTargetModels() const { return m_targetModels; }

  RteModel* GetTargetModel(const std::string& targetName) const;
  RteModel* EnsureTargetModel(const std::string& targetName);
  void CreateTargetModels();

  void ClearTargets();

  const std::map<int, std::string>& GetTargetIDs() const { return m_targetIDs; }
  void SetTargetIDs(const std::map<int, std::string>& targetIDs) { m_targetIDs = targetIDs; }

  const std::string& GetActiveTargetName() const { return m_sActiveTarget; }
  bool SetActiveTarget(const std::string& targetName); // returns true if target has changed
  RteTarget* GetActiveTarget() const { return GetTarget(m_sActiveTarget); }

  void EvaluateComponentDependencies(RteTarget* target); // for active components only
  bool ResolveDependencies(RteTarget* target);
  bool AreDependenciesResolved(RteTarget* target) const;

  void FilterComponents();
  void ClearUsedComponents();
  void ClearSelected();
  void PropagateActiveSelectionToAllTargets();

  bool UpdateFileToNewVersion(RteFileInstance* fi, RteFile* f, bool bMerge);

  static std::string GetRteComponentsH(const std::string& targetName, const std::string& prefix); // in target directory
  static std::string GetRteHeader(const std::string& name, const std::string & targetName, const std::string& prefix); // in target directory

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

  void AddComponentFiles(RteComponentInstance* ci, RteTarget* target, std::set<RteFile*>& forcedFiles);
  RteFileInstance* AddFileInstance(RteComponentInstance* ci, RteFile* f, int index, RteTarget* target);
  bool RemoveFileInstance(const std::string& id);
  void DeleteFileInstance(RteFileInstance* fi);
  // initializes or updates (newer version is used) existing file instance
  void InitFileInstance(RteFileInstance* fi, RteFile* f, int index, RteTarget* target, const std::string& oldVersion, bool bCopy);
  bool UpdateFileInstance(RteFileInstance* fi, RteFile* f, bool bMerge, bool bUpdateComponent);
  void CollectSettings(const std::string& targetName);

  void ClearClasses();
  void CreateTargetModels(RteItemInstance* instance);

protected:
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement) override;
  virtual void CreateXmlTreeElementContent(XMLTreeElement* parentElement) const override;

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

  // targets
protected:
  int m_nID;
  bool m_bInitialized;
  bool t_bGpdscListModified;
  std::map<std::string, RteTarget*> m_targets;
  std::map<std::string, RteModel*> m_targetModels; // filtered models for each target
  std::map<int, std::string> m_targetIDs;
  std::string m_sActiveTarget;
};

#endif // RteProject_H
