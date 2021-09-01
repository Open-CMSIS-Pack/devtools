#ifndef RteAttributes_H
#define RteAttributes_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteAttributes.h
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include <string>
#include <map>

class RteAttributes
{
public:
  // default constructor
  RteAttributes() {};

  // parametrized constructor to instantiate with given attributes
  RteAttributes(const std::map<std::string, std::string>& attributes);

  // destructor
  virtual ~RteAttributes();

  // copy constructor, uses default implementation
  RteAttributes(const RteAttributes&) = default;

  // assignment operator, uses default implementation
  RteAttributes& operator=(const RteAttributes&) = default;

  // move assignment operator, uses default implementation
  RteAttributes& operator=(RteAttributes&&) noexcept = default;

  /**
   * @brief return XML tag name
   * @return XML tag name
  */
  const std::string& GetTag() const { return m_tag; }
  /**
   * @brief set XML tag name
   * @param tag XML tag name
  */
  void SetTag(const std::string& tag) { m_tag = tag; }
  /**
   * @brief determine XML attribute name if not empty, otherwise XML tag name
   * @return attribute or tag name
  */
  virtual const std::string& GetName() const;

  // attribute operations
public:
  /**
   * @brief add new XML attribute to XML tag
   * @param name attribute name
   * @param value attribute value
   * @param insertEmpty true to insert, false to remove if attribute exists but is empty
   * @return true if attribute is inserted/removed
  */
  bool AddAttribute(const std::string& name, const std::string& value, bool insertEmpty = true);
  /**
   * @brief add new attribute
   * @param name attribute name
   * @param value attribute value
   * @return true if attribute is added
  */
  bool SetAttribute(const char* name, const char* value);
  /**
   * @brief add new attribute
   * @param name attribute name
   * @param value attribute value
   * @param radix transformation radix
   * @return true if attribute is added
  */
  bool SetAttribute(const char* name, long value, int radix = 10);
  /**
   * @brief remove attribute
   * @param name attribute name
   * @return true if attribute is removed
  */
  bool RemoveAttribute(const char* name);
  /**
   * @brief replace instance attributes with the given ones
   * @param attributes given attributes
   * @return true if attributes are set
  */
  bool SetAttributes(const std::map<std::string, std::string>& attributes);
  /**
   * @brief replace instance attributes with the given ones
   * @param attributes given instance of RteAttributes
   * @return true if attributes are set
  */
  bool SetAttributes(const RteAttributes& attributes);
  /**
   * @brief add missing attributes, optionally replace existing
   * @param attributes map of name to value pairs to add
   * @param replaceExisting true to replace existing attributes
   * @return true if any attribute is set
  */
  bool AddAttributes(const std::map<std::string, std::string>& attributes, bool replaceExisting);
  /**
   * @brief clear all attributes of the instance
   * @return true if any attribute is removed
  */
  bool ClearAttributes();
  /**
   * @brief determine number of attributes
   * @return number of attributes
  */
  int GetCount() const { return (int)m_attributes.size(); }
  /**
   * @brief check if attribute list is empty
   * @return true if attribute list is empty
  */
  bool IsEmpty() const { return m_attributes.empty(); }
  /**
   * @brief return list of attributes
   * @return list of attributes
  */
  const std::map<std::string, std::string>& GetAttributes() const { return m_attributes; }
  /**
   * @brief concatenate instance attributes
   * @return string containing all attributes
  */
  std::string GetAttributesString() const;
  /**
   * @brief concatenate attributes in a string conformed to XML syntax
   * @return XML string containing attributes
  */
  std::string GetAttributesAsXmlString() const;
  /**
   * @brief determine prefix of attribute value
   * @param name attribute name
   * @param delimiter character value as delimiter
   * @return prefix of attribute value
  */
  std::string GetAttributePrefix(const char* name, char delimiter = ':') const;
  /**
   * @brief determine suffix of attribute value
   * @param name attribute name
   * @param delimiter character value as delimiter
   * @return suffix of attribute value
  */
  std::string GetAttributeSuffix(const char* name, char delimiter = ':') const;
  /**
   * @brief determine suffix as integer of attribute value
   * @param name attribute name
   * @param delimiter character value as delimiter
   * @return suffix as integer of attribute value
  */
  int GetAttributeSuffixAsInt(const char* name, char delimiter = ':') const;
  /**
   * @brief convert attribute value to boolean value
   * @param name attribute name
   * @param defaultValue default value to be returned if attribute value is empty
   * @return true if attribute value equal "1" or "true"
  */
  bool GetAttributeAsBool(const char* name, bool defaultValue = false) const;
  /**
   * @brief convert attribute value to integer value
   * @param name attribute name
   * @param defaultValue default value to be returned if attribute value is empty
   * @return attribute value as integer
  */
  int GetAttributeAsInt(const char* name, int defaultValue = -1) const;
  /**
   * @brief convert attribute value to unsigned integer
   * @param name attribute name
   * @param defaultValue default value to be returned if attribute value is empty
   * @return attribute value as unsigned integer
  */
  unsigned GetAttributeAsUnsigned(const char* name, unsigned defaultValue = 0) const;
  /**
   * @brief convert attribute value to unsigned long long
   * @param name attribute name
   * @param defaultValue default value to be returned if attribute value is empty
   * @return attribute value as unsigned long long
  */
  unsigned long long GetAttributeAsULL(const char* name, unsigned long long defaultValue = 0L) const;
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

public:
  /**
   * @brief determine attribute value
   * @param name attribute name
   * @return attribute value
  */
  virtual const std::string& GetAttribute(const std::string& name) const;
  /**
   * @brief check if attribute exists
   * @param name attribute name
   * @return true if attribute exists in the instance
  */
  virtual bool HasAttribute(const std::string& name) const;
  /**
   * @brief check if attribute has a certain value
   * @param pattern value to be checked, can contain wild cards
   * @return true if value is found
  */
  virtual bool HasValue(const std::string& pattern) const;
  /**
   * @brief check if given attributes exist in the instance
   * @param attributes given list of attributes
   * @return true if given attributes exist in the instance
  */
  virtual bool CompareAttributes(const std::map<std::string, std::string>& attributes) const;
  /**
   * @brief check if attributes of the given instance exist in this instance
   * @param other given instance of RteAttributes
   * @return true if attributes of the given instance exist in this instance
  */
  virtual bool Compare(const RteAttributes& other) const;
  /**
   * @brief check if attributes of the given instance exist in this instance
   * @param other pointer to given instance of RteAttributes
   * @return true if attributes of the given instance exist in this instance
  */
  virtual bool Compare(const RteAttributes* other) const;
  /**
   * @brief check if all given attributes exist in the instance
   * @param attributes given list of attributes
   * @return true if all given attributes exist in the instance
  */
  virtual bool EqualAttributes(const std::map<std::string, std::string>& attributes) const;
  /**
   * @brief check if all attributes of the given instance exist in this instance
   * @param other given instance of RteAttributes
   * @return true if all attributes of the given instance exist in this instance
  */
  virtual bool EqualAttributes(const RteAttributes& other) const;
  /**
   * @brief check if all attributes of the given instance exist in this instance
   * @param other pointer to given instance of RteAttributes
   * @return true if all attributes of the given instance exist in this instance
  */
  virtual bool EqualAttributes(const RteAttributes* other) const;
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
   * @brief determine value of attribute related to vendor
   * @return value of attribute related to vendor
  */
  virtual const std::string& GetVendorString() const;
  /**
   * @brief determine value of version and variant attribute (if any, separated by a blank)
   * @return value of version and variant attribute
  */
  virtual std::string GetVersionVariantString() const;
  /**
   * @brief determine official vendor name of attribute related to vendor
   * @return official vendor name
  */
  virtual std::string GetVendorName() const;
  /**
   * @brief only if not an API, determine value of vendor and bundle attribute (if any, separated by a blank)
   * @return value of vendor and bundle attribute
  */
  virtual std::string GetVendorAndBundle() const;
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
   * @brief determine taxonomy ID of the given list of attributes
   * @param attributes list of attributes
   * @return taxonomy ID
  */
  static std::string GetTaxonomyDescriptionID(const std::map<std::string, std::string>& attributes);
  /**
   * @brief determine value of attribute "doc" or "name" in case first one is empty
   * @return value of attribute "doc"
  */
  virtual const std::string& GetDocAttribute() const;
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
   * @brief determine package ID
   * @param withVersion true if version string should be included
   * @return package ID
  */
  virtual std::string GetPackageID(bool withVersion = true) const;
  /**
   * @brief determine package ID by given list of attributes
   * @param attr list of attributes
   * @param withVersion true if version string should included
   * @return package ID
  */
  static std::string GetPackageIDfromAttributes(const RteAttributes& attr, bool withVersion = true);
  /**
   * @brief return value of attribute "url"
   * @return value of attribute "url"
  */
  virtual const std::string& GetURL() const { return GetAttribute("url"); }
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
   * @brief check value of attribute "custom"
   * @return true if value is "1" or "true"
  */
  virtual bool IsCustom() const { return GetAttributeAsBool("custom"); }
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
   * @brief determine full device name
   * @return full device name
  */
  virtual std::string GetFullDeviceName() const;
  /**
   * @brief determine value of device attribute "remove" as boolean
   * @return value of device attribute "remove" as boolean
  */
  virtual bool IsRemove() const { return GetAttributeAsBool("remove"); }
  /**
   * @brief determine value of device attribute "default" as boolean
   * @return value of device attribute "default" as boolean
  */
  virtual bool IsDefault() const { return GetAttributeAsBool("default"); }
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
   * @brief determine value of attribute "condition"
   * @return condition ID
  */
  virtual const std::string& GetConditionID() const { return GetAttribute("condition"); }
  /**
   * @brief determine value of component attribute "Cclass" with "::" as prefix included
   * @return project group name
  */
  virtual std::string GetProjectGroupName() const;
  /**
   * @brief check if tag is API
   * @return true if tag is API
  */
  virtual bool IsApi() const { return m_tag == "api"; }
  /**
   * @brief evaluate attribute "isDefaultVariant"
   * @return true if value of attribute is "1" or "true"
  */
  virtual bool IsDefaultVariant() const { return GetAttributeAsBool("isDefaultVariant"); }
  /**
   * @brief determine full unique component ID
   * @param withVersion true if version is considered as part of component ID
   * @return full unique component ID
  */
  virtual std::string GetComponentUniqueID(bool withVersion) const;
  /**
   * @brief determine component ID
   * @param withVersion true if version is considered as part of component ID
   * @return full component ID
  */
  virtual std::string GetComponentID(bool withVersion) const;
  /**
   * @brief determine API ID
   * @param withVersion true if version is considered as part of API ID
   * @return API ID
  */
  virtual std::string GetApiID(bool withVersion) const;
  /**
   * @brief determine component ID with vendor and bundle name as prefix
   * @return component ID
  */
  virtual std::string GetComponentAggregateID() const;
  /**
   * @brief construct component ID
   * @param prefix component ID prefix (empty for API, Cvendor[.Cbundle] for components)
   * @param bVariant true if value of attribute "Cvariant" should be included
   * @param bVersion true if value of attribute related to version should be included
   * @param bCondition true if condition ID should be included
   * @param delimiter character behind attribute "Cclass" and "Csub"
   * @return component ID
  */
  virtual std::string ConstructComponentID(const std::string& prefix, bool bVariant, bool bVersion, bool bCondition, char delimiter = '.') const;
  /**
   * @brief construct component display name
   * @param bClass true if value of attribute "Cclass" should be included
   * @param bVariant true if value of attribute "Cvariant" should be included
   * @param bVersion true if value of attribute related to version should be included
   * @param delimiter character behind attribute "Cclass" and "Csub"
   * @return
  */
  virtual std::string ConstructComponentDisplayName(bool bClass = false, bool bVariant = false, bool bVersion = false, char delimiter = ':') const;
  /**
   * @brief construct name for component pre-include header file (prefix "Pre_Include")
   * @return header file name for component pre-include
  */
  virtual std::string ConstructComponentPreIncludeFileName() const;
  /**
   * @brief clears list of attributes
  */
  virtual void Clear() { ClearAttributes(); }
  /**
   * @brief default does nothing
  */
  virtual void ProcessAttributes() {}; // called from SetAttributes(), AddAttributes() and ClearAttributes(), default does nothing
  /**
   * @brief check if all given 'C' attributes exist in the instance
   * @param attributes given list of 'C' attributes
   * @return true if all given 'C' attributes exist in the instance
  */
  virtual bool HasComponentAttributes(const std::map<std::string, std::string>& attributes) const;
  /**
   * @brief check if given list contains all 'C' attributes stored in the instance
   * @param attributes given list of 'C' attributes
   * @return true if given list contains all 'C' attributes stored in the instance
  */
  virtual bool MatchApiAttributes(const std::map<std::string, std::string>& attributes) const;
  /**
   * @brief check if given list contains all 'D' attributes stored in the instance
   * @param attributes given list of device attributes
   * @return true if given list contains all device attributes stored in the instance
  */
  virtual bool MatchDeviceAttributes(const std::map<std::string, std::string>& attributes) const;
  /**
   * @brief check if given list of attributes contains the same values for "Dname", "Pname" and "Dvendor"
   * @param attributes given list of attributes
   * @return true if given list of attributes contains the same values for "Dname", "Pname" and "Dvendor"
  */
  virtual bool MatchDevice(const std::map<std::string, std::string>& attributes) const;

public:
  static const std::string EMPTY_STRING;

protected:
  std::string m_tag; // an item can have a corresponding XML element with tag
  std::map<std::string, std::string> m_attributes;
};

typedef std::map<std::string, RteAttributes> RteAttributesMap;

#endif // RteAttributes_H
