#ifndef RtePackage_H
#define RtePackage_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RtePackage.h
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

#define PDSC_MIN_SUPPORTED_VERSION "1.0"
#define PDSC_MAX_SUPPORTED_VERSION "1.x" // we should only check for major version: x > any number => only major element is compared

class RteTarget;
class RtePackage;
class RteExample;
class RteGeneratorContainer;
class RteGeneratorProject;
class RteBoard;

/**
 * @brief class representing <releases> element in *.pdsc files
*/
class RteReleaseContainer : public RteItem
{
public:

  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteReleaseContainer(RteItem* parent);
  /**
   * @brief process a single XMLTreeElement during construction
   * @param xmlElement pointer to XMLTreeElement to process
   * @return true if successful
  */
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement) override;
};

/**
 * @brief class representing <taxonomy> element in  *.pdsc files
*/
class RteTaxonomyContainer : public RteItem
{
public:

  /**
   * @brief constructor
   * @param parent pointer to RteItem parent
  */
  RteTaxonomyContainer(RteItem* parent);
  /**
   * @brief process a single XMLTreeElement during construction
   * @param xmlElement pointer to XMLTreeElement to process
   * @return true if successful
  */
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement) override;
};


class RteGenerator;
class RteDeviceFamilyContainer;

/**
 * @brief class represents CMSIS-Pack and corresponds to top-level <package> element in *.pdsc file. It also serves as a base for classes supporting *.gpdsc and *.cprj files.
*/
class RtePackage : public RteItem
{
public:

  /**
   * @brief constructor
   * @param parent pointer to RteItem parent (pointer to RteModel)
  */
  RtePackage(RteItem* parent);

  /**
   * @brief constructor
   * @param model pointer to parent RteModel
   * @param attributes package attributes to assign
  */
  RtePackage(RteItem* model, const std::map<std::string, std::string>& attributes);

  /**
   * @brief virtual destructor
  */
  virtual ~RtePackage() override;

  /**
   * @brief get absolute path to the directory where pack's *.pdsc file is located
   * @return absolute path to pdsc file directory with trailing slash
  */
  std::string GetAbsolutePackagePath() const;

  /**
   * @brief get pack common ID, also known as 'pack family ID', does not contain version
   * @return ID string in the form PackVendor.PackName
  */
  const std::string& GetCommonID() const { return m_commonID; }

  /**
   * @brief helper static method  to extract common ID from full pack ID by stripping version information
   * @param id full or common ID
   * @return common pack ID
  */
  static std::string CommonIdFromId(const std::string& id);

  /**
   * @brief helper static method to construct pack display name from IDconstruct
   * @param id full or common pack ID
   * @return string in the format PackVendor::PackName
  */
  static std::string DisplayNameFromId(const std::string& id);

  /**
   * @brief helper static method to extract pack version from its ID
   * @param id pack ID
   * @return version string
  */
  static std::string VersionFromId(const std::string& id);

  /**
   * @brief helper static method to extract release version from pack ID
   * @param id full pack ID
   * @return string containing only minor.major.patch without any suffix
  */
  static std::string ReleaseVersionFromId(const std::string& id);

  /**
   * @brief helper static method to construct pack ID for release version
   * @param id full pack ID
   * @return string containing PackVendor::PackName.minor.major.patch
  */
  static std::string ReleaseIdFromId(const std::string& id);

  /**
   * @brief helper static method to extract pack vendor from ID
   * @param id full or common pack ID
   * @return pack vendor string
  */
  static std::string VendorFromId(const std::string& id);

  /**
   * @brief helper static method to extract pack name from ID
   * @param id full or common pack ID
   * @return pack name string
  */
  static std::string NameFromId(const std::string& id);

  /**
   * @brief helper static method to compare pack IDs.
   * Alpha-numeric comparison for vendor and name.
   * Semantic version comparison for pack version.
   * @param id1 first pack ID
   * @param id2 second pack ID
   * @return 0 if both IDs are equal, less than 0 if id1 < id 2, greater than 0 if id1 > id2
  */
  static int ComparePackageIDs(const std::string& id1, const std::string& id2);

  /**
   * @brief get pack display name
   * @return display string
  */
  virtual std::string GetDisplayName() const override;

  /**
   * @brief get collection of keywords described in the *.pdsc file
   * @return set of keyword string collected from <keywords> element
  */
  const std::set<std::string>& GetKeywords() const { return m_keywords; }

  /**
   * @brief get parent component
   * @return nullptr since a package has no parent component
  */
  virtual RteComponent* GetComponent() const override { return nullptr;}

  /**
   * @brief get component described in this pack
   * @param uniqueID component ID to search for
   * @return pointer to RteComponent if found, nullptr otherwise
  */
  RteComponent* GetComponent(const std::string& uniqueID) const;

  /**
   * @brief get number of APIs in the pack described under <apis> element
   * @return number of <api> elements as integer
  */

  size_t GetApiCount() const { return m_apis ? m_apis->GetChildCount() : 0; }
  /**
   * @brief get number of conditions in the pack described under <conditions> element
   * @return number of <condition> elements as integer
  */

  size_t GetConditionCount() const { return m_conditions ? m_conditions->GetChildCount() : 0; }
  /**
   * @brief get number of components in the pack described under <components> element
   * @return number of <component> elements as integer
  */

  size_t GetComponentCount() const { return m_components ? m_components->GetChildCount() : 0; }
  /**
   * @brief get number of examples  in the pack described under <examples> element
   * @return number of <example> elements as integer
  */
  size_t GetExampleCount() const { return m_examples ? m_examples->GetChildCount() : 0; }

  /**
   * @brief get number of boards in the pack described under <boards> element
   * @return number of <board> elements as integer
  */
  size_t GetBoardCount() const { return m_boards ? m_boards->GetChildCount() : 0; }

  /**
   * @brief get <releases> element
   * @return pointer to RteItem representing container for releases
  */
  RteItem* GetReleases() const { return m_releases; }

  /**
   * @brief get <requirements> element
   * @return pointer to RteItem representing container for requirements
  */
  RteItem* GetRequirements() const { return m_requirements; }

  /**
   * @brief get <conditions> element
   * @return pointer to RteItem representing container for conditions
  */
  RteItem* GetConditions() const { return m_conditions; }

  /**
   * @brief get <components> element
   * @return pointer to RteItem representing container for components
  */
  RteItem* GetComponents() const { return m_components; }

  /**
   * @brief get <apis> element
   * @return pointer to RteItem representing container for APIs
  */
  RteItem* GetApis() const { return m_apis; }

  /**
   * @brief get <components> element
   * @return pointer to RteItem representing container for examples
  */
  RteItem* GetExamples() const { return m_examples; }

  /**
   * @brief get <taxonomy> element
   * @return pointer to RteItem representing taxonomy container
  */
  RteItem* GetTaxonomy() const { return m_taxonomy; }

  /**
   * @brief get <boards> element
   * @return pointer to RteItem representing container for boards
  */
  RteItem* GetBoards() const { return m_boards; }

  /**
  * @brief get collection of <cimage> elements
  * @return list of pointers RteItem representing cimage elements
  */
  const std::list<RteItem*> GetImageDescriptors() const { return GetGrandChildren("cimage"); }

  /**
  * @brief get collection of <clayer> elements
  * @return list of pointers RteItem representing clayer elements
  */
  const std::list<RteItem*> GetLayerDescriptors() const { return GetGrandChildren("clayers"); }

  /**
  * @brief get collection of <cproject> elements
  * @return list of pointers RteItem representing cproject elements
  */
  const std::list<RteItem*> GetProjectDescriptors() const { return GetGrandChildren("cprojects"); }

  /**
   * @brief get collection of <csolution> elements
   * @return list of pointers RteItem representing csolution elements
  */
  const std::list<RteItem*> GetSolutionDescriptors() const { return GetGrandChildren("csolutions"); }

  /**
  * @brief get <generators> element
  * @return pointer to RteGeneratorContainer representing container for generators
  */
  RteGeneratorContainer* GetGenerators() const { return m_generators; }

  /**
   * @brief get generator item for specified ID
   * @param id generator ID
   * @return pointer to RteGenerator if found, nullptr otherwise
  */
  RteGenerator* GetGenerator(const std::string& id) const;

  /**
   * @brief get first generator item in the generator container (usually a pack defines only one)
   * @return pointer to RteGenerator if exists, nullptr otherwise
  */
  RteGenerator* GetFirstGenerator() const;

  /**
   * @brief get <devices> element
   * @return pointer to RteDeviceFamilyContainer representing container for device families
  */
  RteDeviceFamilyContainer* GetDeviceFamiles() const { return m_deviceFamilies; }

  /**
   * @brief get flat list of all devices specified in the pack
   * @param list of pointers RteDeviceItem representing RteDevice or RteDeviceVariant
  */
  void GetEffectiveDeviceItems(std::list<RteDeviceItem*>& devices) const;

  /**
   * @brief get <release> element for specified version
   * @param version release version
   * @return pointer to RteItem if found, or nullptr otherwise
  */
  RteItem* GetRelease(const std::string& version) const;

  /**
   * @brief get release note text for specified version
   * @param version release version
   * @return release text if found, empty string otherwise
  */
  const std::string& GetReleaseText(const std::string& version) const;

  /**
   * @brief get the latest release listed in the pack description
   * @return pointer to RteItem representing the latest release, may be nullptr for old packs
  */
  RteItem* GetLatestRelease() const;

  /**
   * @brief get text of the latest release
   * @return release text if found, empty string otherwise
  */
  const std::string& GetLatestReleaseText() const;

  /**
   * @brief check if specified release version is listed in the pack description
   * @param version pack release version
   * @return true if release description is found
  */
  bool ReleaseVersionExists(const std::string& version);

  /**
   * @brief get "replacement" string for the latest release if any
   * @return value of "replacement" attribute of the latest release item, empty string if not set
  */
  const std::string& GetReplacement() const;

  /**
   * @brief get date of the latest release
   * @return latest release date
  */
  const std::string& GetReleaseDate() const;

  /**
   * @brief get release date of specified version
   * @param version pack version to search for
   * @return release date if found, empty string otherwise
  */
  const std::string& GetReleaseDate(const std::string& version) const;

  /**
   * @brief construct string representing version and date of the latest release
   * @return string in the format ReleaseVersion (ReleaseDate)
  */
  std::string GetDatedVersion() const;

  /**
   * @brief construct string representing version and date of the specified release
   * @return string in the format ReleaseVersion (ReleaseDate) if found, empty string otherwise
  */
  std::string GetDatedVersion(const std::string& version) const;


  /**
   * @brief get date of pack deprecation if any
   * @return "deprecated" attribute value of the latest release item, empty string if pack is not deprecated
  */
  const std::string& GetDeprecationDate() const;

  /**
   * @brief create a pdsc-like XMLTreeElement with pack info
   * @param parent pointer to XMLTreeElement parent for created element
   * @return pointer to created XMLTreeElement
  */
  XMLTreeElement* CreatePackXmlTreeElement(XMLTreeElement* parent = nullptr) const;

public:

  /**
   * @brief get list of packs required by this one
   * @return list of pointers to RteItem objects with pack requirement information
  */
  virtual const std::list<RteItem*>& GetPackRequirements() const;

  /**
   * @brief get list of language requirements imposed by this pack
   * @return list of pointers to RteItem objects with language requirement information
  */
  virtual const std::list<RteItem*>& GetLanguageRequirements() const;

  /**
   * @brief get list of compiler requirements imposed by this pack
   * @return list of pointers to RteItem objects with compiler requirement information
  */
  virtual const std::list<RteItem*>& GetCompilerRequirements() const;

  /**
   * @brief get path to directory where this pack is or will be installed
   * @param withVersion flag if to return the path with version segment
   * @return path relative to CMSIS_PACK_ROOT directory in format: PackVendor/PackName/[PackVersion]/
  */
  virtual std::string GetPackagePath(bool withVersion = true) const override;

  /**
   * @brief get package state
   * @return PackageState value
  */
  virtual PackageState GetPackageState() const override;

  /**
   * @brief get full or common pack ID
   * @param withVersion flag to return full (true) or common (false) ID
   * @return full or common pack ID depending on argument
  */
  virtual std::string GetPackageID(bool withVersion = true) const override;

  /**
   * @brief determine package ID by given list of attributes
   * @param attr list of attributes
   * @param withVersion true if version string should included
   * @return package ID
  */
  static std::string GetPackageIDfromAttributes(const XmlItem& attr, bool withVersion = true);

  /**
   * @brief get absolute filename of pack description file (pdsc)
   * @return *.pdsc filename this <package> element is read from
  */
  virtual const std::string& GetPackageFileName() const override { return m_fileName; }

  /**
   * @brief get URL to download this pack from
   * @param withVersion flag to return url with this version, otherwise without
   * @param extension file extension (with dot), usually ".pack"
   * @return download URL if found in the description, empty string otherwise
  */
  virtual std::string GetDownloadUrl(bool withVersion, const char* extension) const override;

  /**
   * @brief get condition with specified ID
   * @param id condition ID to search for
   * @return pointer to RteCondition if found, nullptr otherwise
  */
  virtual RteCondition* GetCondition(const std::string& id) const override;

  /**
   * @brief get condition for this pack
   * @return nullptr since pack has no condition
  */
  virtual RteCondition* GetCondition() const override { return nullptr;}

public:
  /**
   * @brief get top-level item corresponding to the pack
   * @return this
  */
  virtual RtePackage* GetPackage() const override { return const_cast<RtePackage*>(this); }

  /**
   * @brief check if pack is deprecated
   * @return true if pack is deprecated and should no longer be used
  */
  virtual bool IsDeprecated() const override;

  /**
   * @brief check if pack is dominating, i.e. has precedence over other pack when selecting components
   * @return true if pack is dominating
  */
  virtual bool IsDominating() const;

  /**
   * @brief check if the pack is generated by a program associated with an RteGenerator object
   * @return true if package state is PackageState::PS_GENERATED
  */
  virtual bool IsGenerated() const override { return GetPackageState() == ::PackageState::PS_GENERATED; }

  /**
   * @brief clear all pack structure
  */
  virtual void Clear() override;

  /**
   * @brief construct this pack item and all child elements
   * @param xmlElement pointer to XMLTreeElement to construct from
   * @return true if successful
  */
  virtual bool Construct(XMLTreeElement* xmlElement) override;

  /**
   * @brief validate this pack item and children. Checks for condition duplicates
   * @return true if validation is successful
  */
  virtual bool Validate() override;

  /**
   * @brief insert components and API's described in this pack into supplied RTE model
   * @param model pointer to RteModl to insert to
  */
  virtual void InsertInModel(RteModel* model) override;

protected:
  /**
   * @brief process a single XMLTreeElement during construction
   * @param xmlElement pointer to XMLTreeElement to process
   * @return true if successful
  */
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement) override;

  /**
   * @brief construct and cache pack full and custom ID
   * @return full pack ID
  */
  virtual std::string ConstructID() override;

private:
  int m_nDominating;      // pack dominating flag (-1 if not set)
  int m_nDeprecated;      // pack deprecation flag (-1 if not set)
  RteItem* m_releases;    // <releases> element
  RteItem* m_conditions;  // <conditions> element
  RteItem* m_components;  // <components> element
  RteItem* m_apis;        // <apis> element
  RteItem* m_examples;    // <examples> element
  RteItem* m_taxonomy;    // <taxonomy> element
  RteItem* m_boards;      // <boards> element
  RteItem* m_requirements;// <requirements> element

  RteGeneratorContainer* m_generators;        // <generators> element
  RteDeviceFamilyContainer* m_deviceFamilies; // <devices> element

  std::set<std::string> m_keywords; // collected keyword
  std::string m_commonID; // common or 'family' pack ID
  std::string m_fileName; // absolute filename of this file
};

/**
 * @brief helper compare class for sorted pack maps.
 * compares common ID alpha-numerically in ascending order and version semantically in descending order
*/
class RtePackageComparator
{
public:
  /**
   * @brief compare two pack IDs
   * @param a first pack ID
   * @param b second pack ID
   * @return true if first pack is 'less' than the second one
  */
  bool operator()(const std::string& a, const std::string& b) const { return RtePackage::ComparePackageIDs(a, b) < 0; }
};


typedef std::map<std::string, RtePackage*, RtePackageComparator > RtePackageMap;
typedef std::map<std::string, RteItem*, RtePackageComparator > RteItemPackageMap;

/**
 * @brief class that replicates frequently used information of a pack object or a pack release.
 * contains a reference to the actual pack object that this info represents
*/
class RtePackageInfo : public RteItem
{
public:

  /**
   * @brief constructor
   * @param pack pointer to RtePackage to represent, owns this info
  */
  RtePackageInfo(RtePackage* pack);

  /**
   * @brief constructor
   * @param pack pointer to RtePackage to represent, owns this info
   * @param version release version this pack info represents
  */
  RtePackageInfo(RtePackage* pack, const std::string& version);

  /**
   * @brief get common or 'family' pack ID
   * @return pack ID without version
  */
  const std::string& GetCommonID() const;

  /**
   * @brief get pack display name
   * @return display string
  */
  virtual std::string GetDisplayName() const override;

  /**
   * @brief get full or common pack ID
   * @param withVersion flag to return full (true) or common (false) ID
   * @return full or common pack ID depending on argument
  */
  virtual std::string GetPackageID(bool withVersion = true) const override;

  /**
   * @brief get path to directory where this pack is or will be installed
   * @param withVersion flag if return path segment corresponding to pack version (last segment)
   * @return path relative to CMSIS_PACK_ROOT directory in format: PackVendor/PackName/[PackVersion]/
  */
  virtual std::string GetPackagePath(bool withVersion = true) const override;

  /**
   * @brief check if this info represents the latest pack release, i.e. this RtePackage object itself
   * @return true if info represents the latest release
  */
  virtual bool IsLatestRelease() const;

  /**
   * @brief get URL to download this pack or this release from
   * @param withVersion flag to return url with this version, otherwise without
   * @param extension file extension (with dot), usually ".pack"
   * @return download url if found in the description, empty string otherwise
  */
  virtual std::string GetDownloadUrl(bool withVersion, const char* extension) const override;

 /**
  * @brief get <devices> element of the referenced pack
  * @return pointer to RteDeviceFamilyContainer representing container for device families
 */
  RteDeviceFamilyContainer* GetDeviceFamiles() const;

 /**
  * @brief get <devices> element of the referenced pack
  * @return pointer to RteDeviceFamilyContainer representing container for device families
 */
  RteItem* GetExamples() const;

  /**
   * @brief get <boards> element of the referenced pack
   * @return pointer to RteItem representing container for boards
  */
  RteItem* GetBoards() const;

  /**
   * @brief get absolute path to the directory where pack's *.pdsc file is located
   * @return absolute path to pdsc file directory with trailing slash
  */
  std::string GetAbsolutePackagePath() const;

  /**
   * @brief get release text this info represents
   * @return release text
  */
  const std::string& GetReleaseText() const; // return release note text

  /**
 * @brief get release note text for specified version
 * @param version release version
 * @return release text if found, empty string otherwise
*/
  const std::string& GetReleaseText(const std::string& version) const;

  /**
   * @brief get text of the latest release
   * @return release text if found, empty string otherwise
  */
  const std::string& GetLatestReleaseText() const;

  /**
   * @brief check if specified release version is listed in the pack description
   * @param version pack release version
   * @return true if release description is found
  */
  bool ReleaseVersionExists(const std::string& version);

  /**
   * @brief get "replacement" string for the latest release if any
   * @return value of "release" attribute of the latest release item, empty string if not set
  */
  const std::string& GetReplacement() const;

  /**
   * @brief get date of the latest release
   * @return latest release date
  */
  const std::string& GetReleaseDate() const;

  /**
   * @brief get release date of specified version
   * @param version pack version to search for
   * @return release date if found, empty string otherwise
  */
  const std::string& GetReleaseDate(const std::string& version) const;

 /**
   * @brief construct string representing version and date of the latest release
   * @return string in the format ReleaseVersion (ReleaseDate)
  */
  std::string GetDatedVersion() const;

  /**
   * @brief construct string representing version and date of the specified release
   * @return string in the format ReleaseVersion (ReleaseDate) if found, empty string otherwise
  */
  std::string GetDatedVersion(const std::string& version) const;

 /**
   * @brief get date of pack deprecation if any
   * @return "deprecated" attribute value of the latest release item, empty string if pack is not deprecated
  */
  const std::string& GetDeprecationDate() const;

  /**
   * @brief get pack repository URL
   * @return pack repository URL if specified, empty string otherwise
  */
  const std::string& GetRepository() const; // return repository url of the package or EMPTY_STRING if not specified

  /**
   * @brief get attribute value of a specified pack release
   * @param attribute name of attribute
   * @param version release version
   * @param latest flag to ignore supplied version and get value from the latest release
   * @return attribute value if found, empty string otherwise
  */
  const std::string& GetReleaseAttributeValue(  const std::string& attribute, const std::string &version, bool latest = true) const;

  /**
    * @brief get URL to download this release from
    * @param withVersion flag to return url with this version, otherwise without
    * @param extension file extension (with dot), usually ".pack"
    * @param useReleaseUrl  use data specified in release, ignore other arguments
    * @return download url if found in the description, empty string otherwise
   */
  std::string GetDownloadReleaseUrl(bool withVersion, const char* extension, bool useReleaseUrl) const;

protected:
  /**
   * @brief construct and cache common and full pack ID
   * @return full pack ID constructed from this item attributes
  */
  virtual std::string ConstructID() override;

private:
  /**
   * @brief initialize the info
   * @param pack pointer to RtePackage to represent
   * @param version release version to represent
  */
  void Init(RtePackage* pack, const std::string& version);

private:
  std::string m_commonID; // common pack ID
};

typedef std::map<std::string, RtePackageInfo*, VersionCmp::Greater > RtePackageInfoMap;
typedef std::map<std::string, RtePackageInfo* > RtePackageInfoMapStdComp;



/**
 * @brief class to aggregate package versions to manage pack selection in projects and to support pack filtering.
*/
class RtePackageAggregate : public RteItem
{
public:
  /**
   * @brief constructor
   * @param parent pointer parent RteItem
  */
  RtePackageAggregate(RteItem* parent);

  /**
   * @brief virtual destructor
  */
  virtual  ~RtePackageAggregate() override;

public:
  /**
   * @brief clear internal data
  */
  virtual void Clear() override;

  /**
   * @brief get display name for this aggregate
   * @return display name constructed from common pack ID
  */
  virtual std::string GetDisplayName() const override { return m_displayName; }

  /**
   * @brief get sorted package collection
   * @return reference to RteItemPackageMap map
  */
  const RteItemPackageMap& GetPackages() const { return m_packages; }

  /**
   * @brief check if a pack from aggregate is used in the project
   * @return true if at least one version of the pack is used in the project
  */
  bool IsUsed() const { return !m_usedPackages.empty(); }

  /**
   * @brief check if aggregate uses selection of pack version
   * @return
  */
  bool HasFixedSelection() const { return !m_selectedPackages.empty(); }

  /**
   * @brief get  version mode this pack aggregate uses
   * @return VersionCmp::MatchMode value
  */
  VersionCmp::MatchMode GetVersionMatchMode() const { return m_mode; }

  /**
   * @brief set version mode to use
   * @param mode VersionCmp::MatchMode to use
  */
  void SetVersionMatchMode(VersionCmp::MatchMode mode);

  /**
   * @brief adjust version mode according to pack usage
  */
  void AdjustVersionMatchMode();

  /**
   * @brief get pointer to parent RtePackage item
   * @return nullptr since this item does not have pack parent
  */
  virtual RtePackage* GetPackage() const override { return nullptr;}

  /**
   * @brief get pack for specified ID
   * @param id full pack ID
   * @return pointer to RteItem representing RtePackage or RtePackageInstanceInfo
  */
  RteItem* GetPackage(const std::string& id) const;

  /**
   * @brief get the latest pack or its instance
   * @return pointer to RteItem representing RtePackage or RtePackageInstanceInfo
  */
  RteItem* GetLatestEntry() const;

  /**
   * @brief get the latest installed pack
   * @return pointer to RteItem representing RtePackage or nullptr if no pack is installed
  */
  RtePackage* GetLatestPackage() const;

  /**
   * @brief get latest pack ID
   * @return full Id of the latest pack
  */
  const std::string& GetLatestPackageID() const;

  /**
   * @brief check if pack specified pack release is used in the project target
   * @param id full pack ID
   * @return true if pack is used
  */
  bool IsPackageUsed(const std::string& id) const;

  /**
   * @brief check id specified pack release is selected for the project target
   * @param id full pack ID
   * @return true if pack is selected
  */
  bool IsPackageSelected(const std::string& id) const;

  /**
   * @brief set is a pack is used in the project
   * @param id full pack ID
   * @param bUsed usage flag to set/remove
  */
  void SetPackageUsed(const std::string& id, bool bUsed);

  /**
   * @brief set is a pack is selected in the project
   * @param id full pack ID
   * @param bSelected selection flag to set/remove
  */
  void SetPackageSelected(const std::string& id, bool bSelected);

  /**
   * @brief add pack to this aggregate
   * @param pack pointer to RtePackage to add
  */
  void AddPackage(RtePackage* pack);

  /**
   * @brief add package instance to the aggregate
   * @param packID full pack ID
   * @param pi pointer to RteItem representing RtePackInstance
  */
  void AddPackage(const std::string& packID, RteItem* pi);

  /**
   * @brief get latest pack description
   * @return pack description
  */
  virtual const std::string& GetDescription() const override;

  /**
   * @brief get latest pack URL
   * @return  URL string
  */
  virtual const std::string& GetURL() const override;

protected:
  /**
   * @brief map collection of packs sorted by full ID (newest first)
   * an entry can be either an RtePackage or RtePackageInstanceInfo (package is required by a target, but not installed)
  */
  RteItemPackageMap m_packages; // packages sorted by full ID (newest first)

  VersionCmp::MatchMode m_mode; // version match mode to use
  std::set<std::string> m_selectedPackages; // packs selected to be used in the project target
  std::set<std::string> m_usedPackages;     // packs used in the project target
  std::string m_displayName;                // cached display name of this aggregate
};

/**
 * @brief class to perform pack filtering in the project
*/
class RtePackageFilter
{
public:
  /**
   * @brief default constructor
  */
  RtePackageFilter() :m_bUseAllPacks(true) {};

  /**
   * @brief virtual destructor
  */
  virtual ~RtePackageFilter();

  /**
   * @brief clear filter information
  */
  void Clear();

public:
  /**
   * @brief check if this filter is equal to supplied one
   * @param other reference to another RtePackageFilter to check
   * @return true if pack filter are equal
  */
  bool IsEqual(const RtePackageFilter& other) const;

  /**
   * @brief check if all packs are excluded by this filter
   * @return
  */
  bool AreAllExcluded() const;

  /**
   * @brief check if to use latest releases of all installed packs in project
   * @return true if all packs (latest installed releases) to use in project
  */
  bool IsUseAllPacks() const;

  /**
   * @brief check if specified pack is selected for project
   * @param packId full pack ID
   * @return true if selected
  */
  bool IsPackageSelected(const std::string& packId) const;

  /**
   * @brief check if specified pack is selected for project
   * @param pack pointer to RtePackage
   * @return true if selected
  */
  bool IsPackageSelected(RtePackage* pack) const;


  /**
   * @brief check if specified pack is excluded from project
   * @param packId full pack ID
   * @return true if excluded
  */
  bool IsPackageExcluded(const std::string& packId) const;

  /**
   * @brief check if specified pack is excluded from project
   * @param pack pointer to RtePackage
   * @return true if excluded
  */
  bool IsPackageExcluded(RtePackage* pack) const;

  /**
   * @brief check if specified pack passes filter
   * @param pack pointer to RtePackage
   * @return true if pack passes this filter
  */
  bool IsPackageFiltered(RtePackage* pack) const;
  /**
   * @brief check if specified pack passes filter
   * @param packId full pack ID
   * @return true if pack passes this filter
  */
  bool IsPackageFiltered(const std::string& packId) const;

  /**
   * @brief get attributes of selected packs
   * @return collection of selected pack IDs
  */
  const std::set<std::string>& GetSelectedPackages() const { return m_selectedPacks; }

  /**
   * @brief set selected packs as collection of their IDs
   * @param packs collection of pack IDs to select
   * @return true if selection changed
  */
  bool SetSelectedPackages(const std::set<std::string>& packs);

  /**
   * @brief get collection of latest packs to use
   * @return collection common pack IDs
  */
  const std::set<std::string>& GetLatestPacks() const { return m_latestPacks; }

  /**
   * @brief set attributes of the latest available pack releases
   * @param latestPacks collection of common IDs
   * @return true if internal collection has changed
  */
  bool SetLatestPacks(const std::set<std::string>& latestPacks);

  /**
   * @brief set IDs of the latest installed pack releases
   * @param latestInstalledPacks collection of pack IDs
  */
  void SetLatestInstalledPacks(const std::set<std::string>& latestInstalledPacks) { m_latestInstalledPacks = latestInstalledPacks; }

  /**
   * @brief set to use all packs (their latest releases)
   * @param bUseAllPacks flag
  */
  void SetUseAllPacks(bool bUseAllPacks) { m_bUseAllPacks = bUseAllPacks; }

protected:
  bool m_bUseAllPacks; // flag to use latest releases of all installed packs
  std::set<std::string> m_selectedPacks; // pack IDs
  std::set<std::string> m_latestPacks; // common IDs to pack version

  std::set<std::string> m_latestInstalledPacks; // IDs of global latest packs, used when m_selectedPacks and m_latestPacks are empty
};

#endif // RtePackage_H
