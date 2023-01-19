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

#include <XmlItem.h>
#include <map>

class RteAttributes : public XmlItem
{
public:
  // default constructor
  RteAttributes() noexcept : XmlItem() {};

  // parametrized constructor to instantiate with given attributes
  RteAttributes(const std::map<std::string, std::string>& attributes);

   // attribute operations
public:

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

public:
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
  * @brief return value of attribute "file"
  * @return value of attribute "file"
  */
  virtual const std::string& GetFolderString() const { return GetAttribute("folder"); }
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
   * @brief construct string containing component's attributes "Cclass" "Cgroup" and "Csub"
   * @param delimiter character between attributes "Cclass" "Cgroup" and "Csub"
   * @return component ID
  */
  virtual std::string ConcatenateCclassCgroupCsub(char delimiter = '.') const;

  /**
   * @brief construct component ID
   * @param prefix component ID prefix (empty for API, Cvendor[.Cbundle] for components)
   * @param bVariant true if value of attribute "Cvariant" should be included
   * @param bVersion true if value of attribute related to version should be included
   * @param bCondition true if condition ID should be included
   * @param delimiter character between attributes "Cclass" "Cgroup" and "Csub"
   * @return component ID
  */
  virtual std::string ConstructComponentID(const std::string& prefix, bool bVariant, bool bVersion, bool bCondition, char delimiter = '.') const;

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
   * @brief construct name for component pre-include header file (prefix "Pre_Include")
   * @return header file name for component pre-include
  */
  virtual std::string ConstructComponentPreIncludeFileName() const;
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
  static const RteAttributes EMPTY_ATTRIBUTES;
};

typedef std::map<std::string, RteAttributes> RteAttributesMap;

#endif // RteAttributes_H
