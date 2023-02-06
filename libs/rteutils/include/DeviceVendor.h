#ifndef DeviceVendor_H
#define DeviceVendor_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file DeviceVendor.h
* @brief Device vendor matching
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

class DeviceVendor
{
private:
  DeviceVendor() {}; // private constructor for utility class

public:
  /**
   * @brief compare two vendor names
   * @param vendor1 vendor name to be compared
   * @param vendor2 vendor name to be compared
   * @return true if vendor names matched
  */
  static bool Match(const std::string& vendor1, const std::string& vendor2);
  /**
   * @brief check if vendor name is official one, e.g. NXP is official for Freescale
   * @param vendorName vendor name to be checked
   * @return true if vendor name is official
  */
  static bool IsCanonicalVendorName(const std::string& vendorName); // input : vendor name without suffix!
  /**
   * @brief determine full official vendor name
   * @param vendorPrefix prefix specific to vendor
   * @return full official vendor name
  */
  static std::string GetFullVendorString(const std::string& vendorPrefix);
  /**
   * @brief determine official vendor name
   * @param vendor device vendor name
   * @return official vendor name
  */
  static std::string GetCanonicalVendorName(const std::string& vendor);
public:
  static std::string NO_VENDOR;
  static std::string NO_MCU;

private:
  static const std::map<std::string, std::string>& GetVendorIdToIdMap();
  static const std::map<std::string, std::string>& GetVendorNameToIdMap();
  static const std::map<std::string, std::string>& GetVendorIdToNameMap();

  static const std::string& VendorIDToOfficialID(const std::string& vendorSuffix);
  static const std::string& VendorNameToID(const std::string& vendorPrefix);
  static const std::string& VendorIDToName(const std::string& vendorSuffix);

private:
  static std::map<std::string, std::string> m_vendorNameToId;
  static std::map<std::string, std::string> m_vendorIdToName;
  static std::map<std::string, std::string> m_vendorIdToId; // maps old ID to new ID

};

#endif // DeviceVendor_H
