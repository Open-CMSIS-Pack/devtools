#ifndef RteKernel_H
#define RteKernel_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteKernel.h
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteItemBuilder.h"
#include "RteModel.h"
#include "RteProject.h"
#include "RteTarget.h"
#include "RteUtils.h"

#include "YmlTree.h"

#include <memory>

class RteCprjProject;
class CprjFile;
class IXmlItemBuilder;

/**
 * @brief this singleton class orchestrates CMSIS RTE support, provides access to underlying RTE Model and manages *.cprj projects
*/
class RteKernel
{
public:
  /**
   * @brief constructor
   * @param rteCallback pointer to RteCallback object
   * @param globalModel pointer to RteGlobalModel object
  */
  RteKernel(RteCallback* rteCallback = nullptr, RteGlobalModel* globalModel = nullptr );

  /**
   * @brief destructor
  */
  virtual ~RteKernel();

  /**
   * @brief initialize kernel
   * @return true if successful
  */
  virtual bool Init();

  /**
   * @brief getter for CMSIS pack root folder
   * @return CMSIS pack root folder
  */
  const std::string& GetCmsisPackRoot() const { return m_cmsisPackRoot; }

  /**
   * @brief setter for CMSIS pack root folder
   * @param cmsisPackRoot CMSIS pack root folder to set
   * @return true if root folder is changed, otherwise false
  */
  bool SetCmsisPackRoot(const std::string& cmsisPackRoot);

  /**
   * @brief getter for CMSIS-Toolbox installation directory
   * @return CMSIS-Toolbox installation directory if available
  */
  const std::string& GetCmsisToolboxDir() const { return m_cmsisToolboxDir; }

  /**
   * @brief setter for CMSIS-Toolbox installation directory
   * @param cmsisToolboxDir CMSIS-Toolbox installation directory to set
  */
  void SetCmsisToolboxDir(const std::string& cmsisToolboxDir) { m_cmsisToolboxDir = cmsisToolboxDir; }


  /**
   * @brief getter for RteCallback object
   * @return pointer to RteCallback object
  */
  RteCallback* GetRteCallback() const;

  /**
   * @brief setter for RteCallback object
   * @param callback pointer to RteCallback object to set
  */
  void SetRteCallback(RteCallback* callback);

  /**
   * @brief get collection of externals generators
   * @return map of id to RteGenerator pointer
  */
  const std::map<std::string, RteGenerator*>& GetExternalGenerators() const { return  m_externalGenerators; }

  /**
   * @brief get global external generator for given ID
   * @param id generator ID string
   * @return pointer to RteGenerator if found, nullptr otherwise
  */
  RteGenerator* GetExternalGenerator(const std::string& id) const;

  /**
   * @brief loads external generators
  */
  void LoadExternalGenerators();

  /**
   * @brief clear and deletes external generators
  */
  void ClearExternalGenerators();

  /**
   * @brief getter for global CMSIS RTE model
   * @return pointer to RteGlobalModel object
  */
  RteGlobalModel* GetGlobalModel() const { return m_globalModel; }

  /**
   * @brief getter for project associated with this object
   * @param nPjNum project ID
   * @return pointer to RteProject object
  */
  RteProject* GetProject(int nPjNum = 0) const;

  /**
   * @brief getter for target determined by target name and project ID
   * @param targetName target name
   * @param nPjNum project ID
   * @return pointer to RteTarget object
  */
  RteTarget* GetTarget(const std::string& targetName = RteUtils::EMPTY_STRING, int nPjNum = 0) const;

  /**
   * @brief getter for CMSIS RTE data associated with the given target in project given by its ID
   * @param targetName target name
   * @param nPjNum project ID
   * @return pointer to RteModel object
  */
  RteModel*  GetTargetModel(const std::string& targetName = RteUtils::EMPTY_STRING, int nPjNum = 0) const;

  /**
   * @brief getter for active project
   * @return pointer to Rte
  */
  RteProject* GetActiveProject() const;

  /**
   * @brief getter for active target of active project
   * @return RteTarget pointer
  */
  RteTarget* GetActiveTarget() const;

  /**
   * @brief getter for RteModel of active target
   * @return RteModel pointer
  */
  RteModel*  GetActiveTargetModel() const;

  /**
   * @brief getter for device of active target
   * @return RteDeviceItem pointer
  */
  RteDeviceItem*  GetActiveDevice() const;

  /**
   * @brief load a cprj project for the given toolchain
   * @param cprjFile project to load
   * @param toolchain toolchain to set
   * @param initialize flag to call InitializeCprj()
   * @param updateRteFiles whether to update RTE files or not
   * @return RteCprjProject pointer
  */
  RteCprjProject* LoadCprj(const std::string& cprjFile, const std::string& toolchain = RteUtils::EMPTY_STRING, bool initialize = true, bool updateRteFiles = true);

  /**
   * @brief initializes loaded Rte
   * @param cprjProject pointer to RteCprjProject
   * @param toolchain compiler toolchain to select, if empty non-ambiguous default is used
   * @param toolChainVersion, use default if non-ambiguous
   * @return true if successful
  */
  bool InitializeCprj(RteCprjProject* cprjProject, const std::string& toolchain = RteUtils::EMPTY_STRING, const std::string& toolChainVersion = RteUtils::EMPTY_STRING);

  /**
   * @brief load required CMSIS packs
   * @param cprjFile pointer to CprjFile
   * @return true if successful
  */
  bool LoadRequiredPdscFiles(CprjFile* cprjFile);

  /**
   * @brief Parse *.cprj file
   * @param cprjFileName absolute filename
   * @return pointer to CprjFile is successful, nullptr otherwise
  */
  CprjFile* ParseCprj(const std::string& cprjFileName);

  /**
   * @brief getter for active project
   * @return RteCprjProject pointer if project is successfully loaded, otherwise nullptr
  */
  virtual RteCprjProject* GetActiveCprjProject() const;

  /**
   * @brief getter for cprj project file associated with the active project
   * @return CprjFile pointer
  */
  virtual CprjFile* GetActiveCprjFile() const;

  /**
   * @brief load and insert pack into global model
   * @param packs list of loaded packages
   * @param pdscFiles list of packs to be loaded
   * @return true if executed successfully
  */
  bool LoadAndInsertPacks(std::list<RtePackage*>& packs, std::list<std::string>& pdscFiles);

  /**
   * @brief get installed and local pdsc files as list sorted by pack ID
   * @param pdscFiles list of pdsc files to fill
   * @param bool latest get only latest versions (default true)
   * @return true if CMSIS_PACK_ROOT is set and accessible
  */
  bool GetEffectivePdscFiles(std::list<std::string>& pdscFiles, bool latest = true) const;

    /**
   * @brief get installed and local pdsc files as map sorted by ID
   * @param pdscMap map of pID to pdsc file to fill
   * @param bool latest get only latest versions (default true)
   * @return true if CMSIS_PACK_ROOT is set and accessible
  */
  bool GetEffectivePdscFilesAsMap(std::map<std::string, std::string, RtePackageComparator>& pdscMap, bool latest = true) const;

  /**
   * @brief get list of installed pdsc files
   * @param files collection to fill with absolute pdsc filenames;
   * @param rtePath pack path
  */
  void GetInstalledPdscFiles(std::map<std::string, std::string, RtePackageComparator>& pdscMap) const;

  /**
   * @brief getter for pdsc file determined by pack ID, pack path and pack attributes
   * @param attributes pack attributes
   * @param packId pack ID
   * @return pair of pack ID to pdsc file
  */
   std::pair<std::string, std::string> GetInstalledPdscFile(const XmlItem& attributes) const;

  /**
   * @brief getter for pdsc file pointed by the local repository index and determined by pack attributes, pack path and pack ID.
   * @param attributes pack attributes
   * @return pair of pack ID to pdsc file
  */
  std::pair<std::string, std::string> GetLocalPdscFile(const XmlItem& attributes) const;

  /**
   * @brief get local or installed pdsc file corresponding to supplied pack ID, pack path and pack attributes
   * @param attributes pack attributes
   * @return pair of pack ID to pdsc file
  */
  std::pair<std::string, std::string> GetEffectivePdscFile(const XmlItem& attributes) const;

  /**
   * @brief get pdsc file pointed by the pack 'path' attribute in a project
   * @param attributes pack attributes
   * @param prjPath path from project
   * @return pair of pack ID to pdsc file
  */
  std::pair<std::string, std::string> GetPdscFileFromPath(const XmlItem& attributes, const std::string& cprjPath) const;

  /**
   * @brief create a smart pointer holding an XMLTree pointer to parse XML or YAML files
   * @param itemBuilder pointer to IXmlItemBuilder item factory
   * @param ext file extension to define which parser is required: XML (default) or YAML (".yml" or ".yaml")
   * @return a std::unique_ptr object holding an XMLTree-derived pointer which is nullptr in the default implementation
  */
  virtual std::unique_ptr<XMLTree> CreateUniqueXmlTree(IXmlItemBuilder* itemBuilder = nullptr, const std::string& ext= RteUtils::EMPTY_STRING) const;

  /**
   * @brief create a smart pointer holding an IXmlItemBuilder pointer to be used by XMLTree or YmlTree
   * @param rootParent pointer to RteItem to be parent for root items (optional)
   * @param packState PackageState value
   * @param option pointer to RteItem with options to pass
   * @return a std::unique_ptr object holding an RteItemBuilder-derived pointer
  */
  virtual std::unique_ptr<RteItemBuilder> CreateUniqueRteItemBuilder(RteItem* rootParent = nullptr, PackageState packState = PackageState::PS_UNKNOWN,
                                                                     const RteItem& options = RteItem::EMPTY_RTE_ITEM) const;


  /**
   * @brief save active project into cprj file
   * @param file cprj file name
   * @return true if successful
  */
  bool SaveActiveCprjFile(const std::string& file = RteUtils::EMPTY_STRING) const;

  /**
   * @brief get global pack registry object
   * @return  pointer to RtePackRegistry
  */
  RtePackRegistry* GetPackRegistry() const { return GetGlobalModel()->GetPackRegistry(); }

  /**
   * @brief load pdsc or gpdsc file and construct it
   * @param pdscFile pathname to load
   * @param packState PackageState to assign to a loaded pack
   * @return pointer to loaded RtePackage
  */
  RtePackage* LoadPack(const std::string& pdscFile, PackageState packState = PackageState::PS_UNKNOWN) const ;

  /**
   * @brief load specified pdsc files, but does not insert them in the model
   * @param pdscFiles list of pathnames to load
   * @param packs list to receive loaded packs
   * @param model RteModel to get state and serve as parent
   * @param bReplace boolean flag to replace existing entries in the registry
   * @return true if successful
  */
  bool LoadPacks(const std::list<std::string>& pdscFiles, std::list<RtePackage*>& packs,
                 RteModel* model = nullptr, bool bReplace = false) const;

  /**
   * @brief getter for caller information (name & version)
   * @return XmlItem reference
  */
  const XmlItem& GetToolInfo() const { return m_toolInfo; }

  /**
   * @brief set caller information
   * @param attr attributes to set as XmlItem reference
   * @return true if changed
  */
  void SetToolInfo(const XmlItem& attr) { m_toolInfo = attr; }

protected:
  /**
   * @brief get local pdsc files, optionally filtered
   * @param attr pack attributes to filter
   * @param pdscMap map packId to pdsc path to fill
   * @return true if an entry is found
  */
  bool GetLocalPdscFiles(const XmlItem& attr, std::map<std::string, std::string, RtePackageComparator>& pdscMap) const;
  /**
   * @brief parses $CMSIS_PACK_ROOT/.Local/loacl_repository.pidx file
   * @return pointer to "index" element if successful, nullptr otherwise
  */
  XMLTreeElement* ParseLocalRepositoryIdx() const;

  /**
   * @brief create an XMLTree object to parse XML files
   * @param itemBuilder pointer to IXmlItemBuilder item factory
   * @return XMLTree-derived pointer which is nullptr in the default implementation
  */
  virtual XMLTree* CreateXmlTree(IXmlItemBuilder* itemBuilder) const { return nullptr; }

  /**
   * @brief create a YmlTree object to parse YAML files
   * @param itemBuilder pointer to IXmlItemBuilder item factory
   * @return YmlTree-derived pointer
  */
  virtual YmlTree* CreateYmlTree(IXmlItemBuilder* itemBuilder) const;

  /**
   * @brief get this pointer to use in const methods
   * @return this
  */
  RteKernel* GetThisKernel() const { return const_cast<RteKernel*>(this); }


// data
protected:

  RteGlobalModel* m_globalModel;
  bool m_bOwnModel;
  RteCallback* m_rteCallback;
  XmlItem m_toolInfo;
  std::string m_cmsisPackRoot;
  std::string m_cmsisToolboxDir;
  std::map<std::string, RteItem*> m_externalGeneratorFiles;
  std::map<std::string, RteGenerator*> m_externalGenerators;

};
#endif // RteKernel_H
