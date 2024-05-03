#ifndef RteItem_H
#define RteItem_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteItem.h
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteCallback.h"

#include "RteUtils.h"

#include "XmlTreeItem.h"

#include <set>
#include <ostream>
#include <functional>

/**
 * @brief state of a pack
*/
enum PackageState {
  PS_INSTALLED,          // pack is installed or redirected to local
  PS_AVAILABLE,          // pack is listed in .web folder (available to download)
  PS_DOWNLOADED,         // pack is downloaded to .download folder
  PS_UNKNOWN,            // state is unknown
  PS_COUNT = PS_UNKNOWN, // count of automatic states
  PS_EXPLICIT_PATH,      // packs specified with explicitly set path
  PS_GENERATED           // generated pack (*.gpdsc)
};


class RteCondition;
class RteConditionContext;
class RteComponent;
class RteApi;
class RtePackage;
class RteModel;
class RteItem;
class RteTarget;
class RteProject;
class RteDeviceItem;
class RteDeviceFamily;
class RteDependencyResult;
class RteFileInstance;
class RteFile;
class XMLTreeElement;


/**
 * @brief Abstract visitor class. Allows to perform operations over RteItem tree according to visitor design pattern
*/
class RteVisitor : public XmlItemVisitor<RteItem>
{
};


/**
 * @brief base RTE Data Model class to describe an XML element in a *.pdsc and *.cprj files.
*/
class RteItem : public XmlTreeItem<RteItem>
{

public:
  typedef std::function<bool(RteItem* c0, RteItem* c1)> CompareRteItemType;

  /**
   * @brief result of evaluating conditions and condition expressions
  */
  enum ConditionResult {
    UNDEFINED,            // not evaluated yet
    R_ERROR,              // error evaluating condition ( recursion detected, condition is missing)
    FAILED,               // HW or compiler not match
    MISSING,              // no component is installed
    MISSING_API,          // no required api is installed
    MISSING_API_VERSION,  // no api of required version is installed
    UNAVAILABLE,          // component is installed, but filtered out
    UNAVAILABLE_PACK,     // component is installed, pack is not selected
    INCOMPATIBLE,         // incompatible component is selected
    INCOMPATIBLE_VERSION, // incompatible version of component is selected
    INCOMPATIBLE_VARIANT, // incompatible variant of component is selected
    CONFLICT,             // more than one exclusive component selected
    INSTALLED,            // matching component is installed, but not selectable because not in active bundle
    SELECTABLE,           // matching component is installed, but not selected
    FULFILLED,            // required component selected or no dependency exists
    IGNORED               // condition/expression is irrelevant for the current context
  };

  static const std::string& ConditionResultToString(RteItem::ConditionResult res);
public:

  /**
   * @brief constructor
   * @param parent pointer to parent RteItem or nullptr if this item has no parent
  */
  RteItem(RteItem* parent = nullptr);

  /**
   * @brief constructor
   * @param item tag
   * @param parent pointer to parent RteItem or nullptr if this item has no parent
  */
  RteItem(const std::string& tag,  RteItem* parent = nullptr);

/**
  * @brief parametrized constructor to instantiate with given attributes
  * @param attributes collection as key to value pairs
  * @param parent pointer to parent RteItem or nullptr if this item has no parent
 */
  RteItem(const std::map<std::string, std::string>& attributes, RteItem* parent = nullptr);

  /**
   * @brief virtual destructor
  */
  ~RteItem() override;

 /**
  * @brief getter for this instance
  * @return pointer to the instance of type RteItem
 */
  RteItem* GetThis() const override { return const_cast<RteItem*>(this); }

 /**
  * @brief clear internal item structure including children. The method is called from destructor
 */
  void Clear() override;

  /**
   * @brief called to construct the item with attributes and child elements
  */
  void Construct() override;

  /**
   * @brief create a new instance of type RteItem
   * @param tag name of tag
   * @return pointer to instance of type RteItem
  */
  RteItem* CreateItem(const std::string& tag) override { return new RteItem(tag, GetThis()); }

public:

  /**
   * @brief get a child with corresponding tag and attribute
   * @param tag item's tag
   * @param attribute item's attribute key
   * @param value item's attribute value
   * @return pointer to child RteItem or nullptr
  */
  RteItem* GetChildByTagAndAttribute(const std::string& tag, const std::string& attribute, const std::string& value) const;

  /**
  * @brief get list of child items with corresponding tag
  * @param tag item's tag
  * @param items collection of RteItem* to fill
  * @reture reference to items
  */
  Collection<RteItem*>& GetChildrenByTag(const std::string& tag, Collection<RteItem*>&items) const;

  /**
   * @brief static method to query children of RteItem
   * @param item RteItem whose children to query
   * @return list of RteItem pointers
  */
  static const Collection<RteItem*>& GetItemChildren(RteItem* item);

  /**
   * @brief static method to query grandchildren of RteItem for specified child tag
   * @param item pointer to RteItem whose grandchildren to query
   * @param tag child's tag
   * @return list of RteItem pointers
  */
  static const Collection<RteItem*>& GetItemGrandChildren(RteItem* item, const std::string& tag);

  /**
   * @brief checks if this item is in valid state
   * @return true if item is valid
  */
  bool IsValid() const override { return m_bValid; }

  /**
   * @brief get child item for specified ID
   * @param id child's ID'
   * @return pointer to RteItem or nullptr
  */
  RteItem* GetItem(const std::string& id) const;

  /**
   * @brief checks if supplied item exists in the list of children
   * @param item pointer to RteItem to check
   * @return true if supplied item is found in the list of children
  */
  bool HasItem(RteItem* item) const;

  /**
   * @brief get the first child item with given tag if any
   * @param tag child's tag
   * @return pointer to RteItem or nullptr
  */
  RteItem* GetItemByTag(const std::string& tag) const;

  /**
   * @brief add item to the list of children
   * @param item pointer to RteItem to add
  */
  void AddItem(RteItem* item) { AddChild(item);}

  /**
   * @brief remove item from the list of children
   * @param item pointer to RteItem to remove
  */
  void RemoveItem(RteItem* item);

  /**
   * @brief get rte folder associated with this item
   * @return rte folder
  */
  virtual const std::string& GetRteFolder() const;

  /**
   * @brief get url or file path to documentation associated with this item
   * @return url or file path
  */
  virtual const std::string& GetDocValue() const;

  /**
 * @brief determine value of attribute "doc" or "name" in case first one is empty
 * @return value of attribute "doc"
*/
  virtual const std::string& GetDocAttribute() const;

  /**
   * @brief get vendor associated with the item
   * @return vendor string
  */
  virtual const std::string& GetVendorString() const;

  /**
 * @brief determine official vendor name of attribute related to vendor
 * @return official vendor name
*/
  virtual std::string GetVendorName() const;

  /**
 * @brief determine value of attribute related to version
 * @return value of attribute related to version
*/
  virtual const std::string& GetVersionString() const;

  /**
 * @brief determine value of attribute related to API version
 * @return value of attribute related to API version
*/
  virtual const std::string& GetApiVersionString() const { return GetAttribute("Capiversion"); }

  /**
   * @brief sort children of an RteItem instance
   * @param cmp compare function which will be called by std::list::sort
  */
  void SortChildren(CompareRteItemType cmp);

  /**
   * @brief compare two components in ascending order
   * @param c0 component to be compared
   * @param c1 component to be compared
   * @return true if c0 smaller than c1
  */
  static bool CompareComponents(RteItem* c0, RteItem* c1);

public:

  /**
   * @brief get RteCallback available for this item if any
   * @return pointer to RteCallback
  */
  virtual RteCallback* GetCallback() const;

  /**
   * @brief search for RteModel item in the parent chain
   * @return pointer to this or parent RteModel or nullptr
  */
  virtual RteModel* GetModel() const;

  /**
   * @brief search for RtePackage item in the parent chain
   * @return pointer to this or parent RtePackage or nullptr
  */
  virtual RtePackage* GetPackage() const;

  /**
   * @brief search for RteComponent item in the parent chain
   * @return pointer to this or parent RteComponent or nullptr
  */
  virtual RteComponent* GetComponent() const;

  /**
   * @brief search for RteProject item in the parent chain
   * @return pointer to this or parent RteProject or nullptr
  */
  virtual RteProject* GetProject() const;

  /**
   * @brief check if this item is deprecated
   * @return true if this item or its package is deprecated
  */
  virtual bool IsDeprecated() const;

  /**
   * @brief collect cached results of evaluating component dependencies for this item
   * @param results map to collect results
   * @param target RteTarget associated with condition context
   * @return overall RteItem::ConditionResult for this item
  */
  virtual ConditionResult GetDepsResult(std::map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const;

  /**
   * @brief evaluate condition attached to this item
   * @param context RteConditionContext to use for evaluation
   * @return evaluated ConditionResult, IGNORED if this item does not have associated condition
  */
  virtual ConditionResult Evaluate(RteConditionContext* context);

  /**
   * @brief get cached condition result for this item
   * @param context condition evaluation context
   * @return cached ConditionResult, previously obtained by Evaluate()
  */
  virtual ConditionResult GetConditionResult(RteConditionContext* context) const;

 public:
  /**
   * @brief get item's ID created by ConstructID() called from Construct()
   * @return item ID string
  */
   virtual const std::string& GetID() const;

  /**
   * @brief get unique component ID: "Vendor::Class&Bundle:Group:Sub&Variant\@1.2.3(condition)[pack]"
   * @return component unique ID
  */
  virtual std::string GetComponentUniqueID() const;

 /**
  * @brief get full component ID: "Vendor::Class&Bundle:Group:Sub&Variant\@1.2.3"
  * @param withVersion true to append version as "\@1.2.3"
  * @return full component ID
 */
  virtual std::string GetComponentID(bool withVersion) const;

  /**
   * @brief construct partial component ID: "Class&Bundle:Group:Sub&Variant"
   * @param bWithBundle flag to include bundle in the ID
   * @return partial component ID
  */
  virtual std::string GetPartialComponentID(bool bWithBundle) const;

  /**
   * @brief get component aggregate ID: "Vendor::Class&Bundle:Group:Sub"
   * @return component aggregate ID
  */
  virtual std::string GetComponentAggregateID() const;

  /**
   * @brief determine API ID
   * @param withVersion true if version is considered as part of API ID
   * @return API ID
  */
  virtual std::string GetApiID(bool withVersion) const;

  /**
   * @brief get ID of a condition expression for dependency search: tag + component ID
   * @return dependency expression ID
  */
  virtual std::string GetDependencyExpressionID() const;

  /**
   * @brief get item's name to display to user
   * @return name to display
  */
  virtual std::string GetDisplayName() const;

  /**
 * @brief check if tag is API
 * @return true if tag is API
*/
  virtual bool IsApi() const { return m_tag == "api"; }

  /**
   * @brief return value of component attribute "Cclass"
   * @return value of component attribute "Cclass"
  */
  virtual const std::string& GetCclassName() const { return GetAttribute("Cclass"); }
  /**
   * @brief return value of component attribute "Cgroup"
   * @return value of component attribute "Cgroup"
  */
  virtual const std::string& GetCgroupName() const { return GetAttribute("Cgroup"); }
  /**
   * @brief return value of component attribute "Csub"
   * @return value of component attribute "Csub"
  */
  virtual const std::string& GetCsubName() const { return GetAttribute("Csub"); }
  /**
   * @brief return value of component attribute "Cvariant"
   * @return value of component attribute "Cvariant"
  */
  virtual const std::string& GetCvariantName() const { return GetAttribute("Cvariant"); }
  /**
   * @brief return value of component attribute "Cbundle"
   * @return value of component attribute "Cbundle"
  */
  virtual const std::string& GetCbundleName() const { return GetAttribute("Cbundle"); }

  /**
 * @brief construct string containing component's attributes "Cclass" "Cgroup" and "Csub"
 * @param delimiter character between attributes "Cclass" "Cgroup" and "Csub"
 * @return component ID
*/
  std::string ConcatenateCclassCgroupCsub(char delimiter = ':') const;

  /**
   * @brief sets attributes to this by parsing supplied component ID
   * @param componentId component ID string
  */
  void SetAttributesFomComponentId(const std::string& componentId);

  /**
   * @brief construct component ID
   * @param bVersion true if value of attribute related to version should be included
   * @return component ID
  */
  std::string ConstructComponentID( bool bVersion) const;

  /**
   * @brief construct component display name
   * @param bClass true if value of attribute "Cclass" should be included
   * @param bVariant true if value of attribute "Cvariant" should be included
   * @param bVersion true if value of attribute related to version should be included
   * @param delimiter character between attributes "Cclass" "Cgroup" and "Csub"
   * @return
  */
  virtual std::string ConstructComponentDisplayName(bool bClass = false, bool bVariant = false, bool bVersion = false, char delimiter = ':') const;

  /**
   * @brief determine value of component attribute "Cclass" with "::" as prefix included
   * @return project group name
  */
  virtual std::string GetProjectGroupName() const;

  /**
   * @brief only if bundle attribute not empty, determine value of vendor and bundle attribute
   * @return value of vendor and bundle attribute
  */
  virtual std::string GetBundleShortID() const;

  /**
   * @brief determine bundle ID
   * @param bWithVersion true if version should be included
   * @return bundle ID
  */
  virtual std::string GetBundleID(bool bWithVersion) const;

  /**
 * @brief determine taxonomy ID of an item
 * @return taxonomy ID of an item
*/
  virtual std::string GetTaxonomyDescriptionID() const;
  /**
   * @brief determine taxonomy ID of the given attributes
   * @param attributes list of attributes as XmlItem reference
   * @return taxonomy ID
  */
  static std::string GetTaxonomyDescriptionID(const XmlItem& attributes);
  /**
   * @brief get ID of RtePackage containing this item
   * @param withVersion flag to append version to pack ID
   * @return pack ID if any or empty string
  */
  virtual std::string GetPackageID(bool withVersion = true) const;

  /**
   * @brief get path to installed pack containing this item
   * @param withVersion flag to include packs's version
   * @return path relative to pack installation directory, path does not need to exist
  */
  virtual std::string GetPackagePath(bool withVersion = true) const;

  /**
  * @brief get absolute path to the directory where pack's *.pdsc file is located
   * @return absolute path to pdsc file directory with trailing slash
 */
  virtual std::string GetAbsolutePackagePath() const { return GetRootFilePath(true); }

  /**
   * @brief get PackageState of RtePackage containing this item
   * @return PackageState of parent RtePackage or PS_UNKNOWN if item does not belong to a package
  */
  virtual PackageState GetPackageState() const;

  /**
   * @brief get file name of RtePackage's containing this item
   * @return RtePackage's file name or empty string if item does not belong to a package
  */
  virtual const std::string& GetPackageFileName() const;

  /**
   * @brief get absolute filename associated with this item (only relevant for files, docs and books)
   * @return absolute filename or url
  */
  virtual std::string GetOriginalAbsolutePath() const;

  /**
   * @brief get absolute filename associated with this item
   * @param name filename to append to the absolute path
   * @return constructed absolute path or name if name is a url or already absolute
  */
  virtual std::string GetOriginalAbsolutePath(const std::string& name) const;

  /**
   * @brief check if item provides data matching OS this code is built for: Linux, Windows or Mac OS
   * @return true if item data matches host platform
  */
  virtual bool MatchesHost() const;

  /**
   * @brief check if item provides data matching supplied host type
   * @param hostType host type to match, empty to match current host
   * @return true if item data matches host platform
  */
  virtual bool MatchesHost(const std::string& hostType) const;

  /**
   * @brief collect components matching supplied attributes
   * @param item reference to RteItem containing component attributes to match
   * @param components container for components
   * @return pointer to first component found if any, null otherwise
  */
  virtual RteComponent* FindComponents(const RteItem& item, std::list<RteComponent*>& components) const;

  /**
   * @brief checks if the item contains all attributes matching those in the supplied map
   * @param item reference to an RteItem containing component attributes to match
   * @return true if the item has all attributes found in the supplied map
  */
  virtual bool MatchComponent(const RteItem& item) const;

  /**
  * @brief checks if the item contains all attributes matching those in the supplied map
  * @param attributes map of key-value pairs to match against component attributes
  * @param bRespectVersion flag to consider Cversion and Capiversion attributes, default is true
  * @return true if the item has all attributes found in the supplied map
  */
  virtual bool MatchComponentAttributes(const std::map<std::string, std::string>& attributes, bool bRespectVersion = true) const;

  /**
   * @brief check if the item matches supplied API attributes
   * @param attributes collection of 'C' (API) attributes
   * @param bRespectVersion flag to consider Capiversion attribute, default is true
   * @return true if the item matches supplied API attributes
  */
  virtual bool MatchApiAttributes(const std::map<std::string, std::string>& attributes, bool bRespectVersion = true) const;

  /**
   * @brief check if given collection of attributes contains the same values for "Dname", "Pname" and "Dvendor"
   * @param attributes collection of attributes
   * @return true if collection of attributes contains the same values for "Dname", "Pname" and "Dvendor"
  */
  virtual bool MatchDevice(const std::map<std::string, std::string>& attributes) const;

  /**
   * @brief check if the item matches all supplied 'D' attributes stored in the instance
   * @param attributes collection of 'D' device attributes
   * @return true if given list contains all device attributes stored in the instance
  */
  virtual bool MatchDeviceAttributes(const std::map<std::string, std::string>& attributes) const;

  /**
   * @brief check if attribute "maxInstances" is not empty
   * @return true if attribute "maxInstances" is not empty
  */
  virtual bool HasMaxInstances() const;
  /**
   * @brief determine value of attribute "maxInstances" as integer, default value is 1
   * @return value of attribute "maxInstances" as integer, default value is 1
  */
  virtual int GetMaxInstances() const;

  /**
   * @brief expands key sequences ("@L", "%L", etc.) or access sequences in the supplied string.
   * @param str string to expand
   * @param bUseAccessSequences expand access sequences instead of key sequences, default is false
   * @param context pointer to RteItem representing expansion context (optional)
   * @return expanded string
  */
  virtual std::string ExpandString(const std::string& str,
    bool bUseAccessSequences = false, RteItem* context = nullptr) const;

  /**
   * @brief get item's description
   * @return description of the item if exist,or empty string
  */
  virtual const std::string& GetDescription() const;

  /**
   * @brief get absolute path to a document fie associated with the item
   * @return absolute path or url
  */
  virtual std::string GetDocFile() const;

  /**
   * @brief composes url to download pack containing the item
   * @param withVersion flag if url should contain version
   * @param extension download file extension
   * @return composed download url
  */
  virtual std::string GetDownloadUrl(bool withVersion, const char* extension) const;

  /**
  * @brief determine value of attribute "condition"
  * @return condition ID
  */
  virtual const std::string& GetConditionID() const { return GetAttribute("condition"); }

  /**
   * @brief return value of attribute "variant"
   * @return value of attribute "variant"
   */
  virtual const std::string& GetVariantString() const { return GetAttribute("variant"); }

  /**
   * @brief return value of attribute "type"
   * @return value of attribute "type"
  */
  virtual const std::string& GetTypeString() const { return GetAttribute("type"); }
  /**
  * @brief return value of attribute "file"
  * @return value of attribute "file"
  */
  virtual const std::string& GetFileString() const { return GetAttribute("file"); }
  /**
  * @brief return value of attribute "path"
  * @return value of attribute "path"
  */
  virtual const std::string& GetPathString() const { return GetAttribute("path"); }
  /**
  * @brief return value of attribute "copy-to"
  * @return value of attribute "copy-to"
  */
  virtual const std::string& GetCopyToString() const { return GetAttribute("copy-to"); }
  /**
   * @brief return value of attribute "folder"
   * @return value of attribute "folder"
   */
  virtual const std::string& GetFolderString() const { return GetAttribute("folder"); }

  /**
 * @brief determine full device name
 * @return full device name
*/
  virtual std::string GetFullDeviceName() const;

  /**
  * @brief return value of device attribute "Dfamily"
  * @return value of device attribute "Dfamily"
 */
  virtual const std::string& GetDeviceFamilyName() const { return GetAttribute("Dfamily"); }
  /**
   * @brief return value of device attribute "DsubFamily"
   * @return value of device attribute "DsubFamily"
  */
  virtual const std::string& GetDeviceSubFamilyName()const { return GetAttribute("DsubFamily"); }
  /**
   * @brief return value of device attribute "Dname"
   * @return value of device attribute "Dname"
  */
  virtual const std::string& GetDeviceName() const { return GetAttribute("Dname"); }
  /**
   * @brief return value of device attribute "Dvariant"
   * @return value of device attribute "Dvariant"
  */
  virtual const std::string& GetDeviceVariantName() const { return GetAttribute("Dvariant"); }
  /**
   * @brief return value of device attribute "Dvendor"
   * @return value of device attribute "Dvendor"
  */
  virtual const std::string& GetDeviceVendor() const { return GetAttribute("Dvendor"); }
  /**
   * @brief return value of attribute "Pname"
   * @return value of attribute "Pname"
  */
  virtual const std::string& GetProcessorName() const { return GetAttribute("Pname"); }

  /**
   * @brief return device attribute value in YAML format used in csolution
   * @param rteName attribute name in pdsc format
   * @param defaultValue YAML value to be returned if attribute is not found, default is empty string
   * @return YAML value if attribute exists or default
  */
  const std::string& GetYamlDeviceAttribute(const std::string& rteName,
    const std::string& defaultValue=RteUtils::EMPTY_STRING);

  /**
 * @brief return value of memory attribute "alias"
 * @return value of memory attribute "alias"
*/
  virtual const std::string& GetAlias() const { return GetAttribute("alias"); }
  /**
   * @brief determine value of memory attribute "uninit" as boolean. Evaluates "init" if "uninit" does not exist
   * @return value of memory attribute "uninit"
  */
  virtual bool IsNoInit() const
  {
    if (HasAttribute("uninit")) {
      return GetAttributeAsBool("uninit");
    }
    return GetAttributeAsBool("init"); // backward compatibility
  }
  /**
   * @brief check value of memory attribute "startup"
   * @return true if value is "1" or "true"
  */
  virtual bool IsStartup() const { return GetAttributeAsBool("startup"); }
  /**
   * @brief return value of memory attribute "access"
   * @return value of memory attribute "access"
  */
  virtual const std::string& GetAccess() const { return GetAttribute("access"); }
  /**
   * @brief check for memory read access
   * @return true if read access specified
  */
  virtual bool IsReadAccess();
  /**
   * @brief check for memory write access
   * @return true if write access specified
  */
  virtual bool IsWriteAccess();
  /**
   * @brief check for memory executable access
   * @return true if executable access specified
  */
  virtual bool IsExecuteAccess();
  /**
   * @brief check for memory secure access
   * @return true if secure access specified
  */
  virtual bool IsSecureAccess();
  /**
   * @brief check for memory non-secure access
   * @return true if non-secure access specified
  */
  virtual bool IsNonSecureAccess();
  /**
   * @brief check for memory callable access
   * @return true if callable access specified
  */
  virtual bool IsCallableAccess();
  /**
   * @brief check for memory peripheral area
   * @return true if memory peripheral area specified
  */
  virtual bool IsPeripheralAccess();

  /**
   * @brief get RteCondition associated with the item
   * @return pointer RteCondition or nullptr if item has no condition
  */
  virtual RteCondition* GetCondition() const;

  /**
   * @brief find RteCondition with given ID in RtePackage containing this item
   * @param id condition ID to search for
   * @return pointer RteCondition or nullptr if no such condition found
  */
  virtual RteCondition* GetCondition(const std::string& id) const;

  /**
   * @brief get license set associated with the item
   * @return pointer to RteItem if found, nullptr otherwise
  */
  virtual RteItem* GetLicenseSet() const;

  /**
   * @brief check if item is associated with a condition that depends on selected device
   * @return true if item's condition depends on selected device
  */
  virtual bool IsDeviceDependent() const;

  /**
  * @brief check if item is associated with a condition that depends on selected board
  * @return true if item's condition depends on selected board
  */
  virtual bool IsBoardDependent() const;

  /**
  * @brief return value of the attribute "generator"
  * @return value of attribute "generator"
  */
  virtual const std::string& GetGeneratorName() const { return GetAttribute("generator"); }

  /**
 * @brief return attribute value of the attribute "generated" as boolean
 * @return attribute value as boolean
*/
  virtual bool IsGenerated() const { return GetAttributeAsBool("generated"); }
  /**
   * @brief check if a component may be selected
   * @return true if a component may be selected
  */
  virtual bool IsSelectable() const { return !IsGenerated() || GetAttributeAsBool("selectable") || HasAttribute("generator"); }
  /**
   * @brief check value of attribute "custom"
   * @return true if value is "1" or "true"
  */
  virtual bool IsCustom() const { return GetAttributeAsBool("custom"); }

  /**
 * @brief determine value of device attribute "remove" as boolean
 * @return value of device attribute "remove" as boolean
*/
  virtual bool IsRemove() const { return GetAttributeAsBool("remove"); }

  /**
   * @brief determine value of attribute "default" as boolean
   * @return value of attribute "default" as boolean
  */
  virtual bool IsDefault() const { return GetAttributeAsBool("default"); }

  /**
   * @brief find child with attribute "default" set to true
   * @return pointer to RteItem if found, nullptr otherwise
  */
  virtual RteItem* GetDefaultChild() const;

  /**
 * @brief evaluate attribute "isDefaultVariant"
 * @return true if value of attribute is "1" or "true"
*/
  virtual bool IsDefaultVariant() const { return GetAttributeAsBool("isDefaultVariant"); }

  /**
 * @brief return value of attribute "url"
 * @return value of attribute "url"
*/
  virtual const std::string& GetURL() const { return GetAttribute("url"); }

  /**
   * @brief get errors found by Construct() or Validate()
   * @return list of error strings
  */
  const std::list<std::string>& GetErrors() const { return m_errors; }

  /**
   * @brief creates an error string for this item
   * @param severity error severity : "Error", "Warning", "Info", etc.
   * @param errNum error number as a string, e.g. "E042"
   * @param message error message
   * @return created error string
  */
  std::string CreateErrorString(const char* severity, const char* errNum, const char* message) const;

  /**
   * @brief clear internal list of errors
  */
  virtual void ClearErrors() { m_errors.clear(); }

public:

  /**
   * @brief validate internal item structure and children recursively and set internal validity flag
   * @return validation result as boolean value
  */
  virtual bool Validate();

  /**
   * @brief reset internal validity flag
  */
  virtual void Invalidate() { m_bValid = false; }

  /**
   * @brief insert this item or its data to supplied RteModel
   * @param model pointer to RteModel, cannot be nullptr
  */
  virtual void InsertInModel(RteModel* model);

  /**
   * @brief create XMLTreeElement object to export this item to XML
   * @param parentElement parent for created XMLTreeElement
   * @param bCreateContent create XML content out of children
   * @return created XMLTreeElement
  */
  virtual XMLTreeElement* CreateXmlTreeElement(XMLTreeElement* parentElement, bool bCreateContent = true) const;

  /**
   * @brief create child item with given tag and name, add it to children collection
   * @param tag tag to set to created item
   * @param name "name" attribute to set to created item
   * @return pointer to created RteItem
  */
  virtual RteItem* CreateChild(const std::string& tag, const std::string& name = RteUtils::EMPTY_STRING);

protected:

  /**
   * @brief construct item's ID
   * @return constructed string ID
  */
  virtual std::string ConstructID();

  /**
   * @brief check if item provides XML content to be generated by CreateXmlTreeElementContent()
   * @return true if element has children or other data to represent as XML text
  */
  virtual bool HasXmlContent() const;

  /**
   * @brief creates child element for supplied XMLTreeElement
   * @param parentElement XMLTreeElement to generate content for
  */
  virtual void CreateXmlTreeElementContent(XMLTreeElement* parentElement) const;
public:
  /**
   * @brief Empty RteItem to be used as a null object
  */
  static RteItem EMPTY_RTE_ITEM;

protected:
  bool m_bValid; // validity flag
  std::string m_ID; // an item ID, constructed in ConstructID() called by Construct()

  std::list<std::string> m_errors; // errors or warnings found by Construct() or Validate()

};


/**
 * @brief container class that represent root element at file level (pdsc, cprj, etc.)
*/
class RteRootItem : public RteItem
{
public:

  /**
   * @brief constructor
   * @param parent pointer to parent RteItem or nullptr if this item has no parent
  */
  RteRootItem(RteItem* parent = nullptr) : RteItem(parent) {}

  /**
   * @brief constructor
   * @param item tag
   * @param parent pointer to parent RteItem or nullptr if this item has no parent
  */
  RteRootItem(const std::string& tag, RteItem* parent = nullptr) : RteItem(tag, parent) {}

  /**
   * @brief getter for root item
   * @return this
  */
  RteItem* GetRoot() const override {
    return GetThis();
  }

  /**
   * @brief get absolute filename of the file for this root item
   * @return filename this root element is read from
  */
  const std::string& GetRootFileName() const override { return m_rootFileName; }

  /**
   * @brief set filename associated with the root item this instance belongs to
   * @param rootFileName absolute file name string
  */
   void SetRootFileName(const std::string& rootFileName) override {
    m_rootFileName = rootFileName;
  }

  /**
   * @brief create a new instance of type RteItem
   * @param tag name of tag
   * @return pointer to instance of type RteItem
  */
   RteItem* CreateItem(const std::string& tag) override;

protected:
  std::string m_rootFileName; // absolute filename of this item's file
};


/**
 * @brief Visitor to print error messages
*/
class RtePrintErrorVistior: public RteVisitor
{
public:
  /**
   * @brief constructor
   * @param callback RteCallback that provides methods to output messages
  */
  RtePrintErrorVistior(RteCallback* callback): m_callback(callback) {}

public:

	VISIT_RESULT Visit(RteItem* rteItem) override
	{
    if(rteItem->IsValid())
      return VISIT_RESULT::SKIP_CHILDREN;
    const std::list<std::string>& errors = rteItem->GetErrors();
    if(errors.empty())
      return VISIT_RESULT::CONTINUE_VISIT;
    std::list<std::string>::const_iterator it;
    for( it = errors.begin(); it != errors.end(); it++){
      std::string sErr= (*it);
      if(m_callback) {
        m_callback->OutputMessage(sErr);
      }
    }
    return VISIT_RESULT::CONTINUE_VISIT;
  }

private:
  RteCallback* m_callback; // callback to output  messages
};


#endif // RteItem_H
