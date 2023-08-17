#ifndef RteBoard_H
#define RteBoard_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteBoard.h
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


/**
 * @brief class to provide board description, corresponds to <board> pdsc file element
*/
class RteBoard : public RteItem
{
public:

  /**
  * @brief constructor
  * @param parent pointer to RteItem parent
  */
  RteBoard(RteItem* parent);

  /**
   * @brief virtual destructor
  */
   ~RteBoard() override;
  /**
   * @brief get mounted device name in the format Dname[,Dvendor]
   * @param withVendor flag to indicate to append vendor string
   * @return device name, optionally with canonical vendor string
  */
  virtual const std::string& GetMountedDevsString(bool withVendor) { return (withVendor ? m_mountedDevsVendor : m_mountedDevs); }
  /**
   * @brief get brief information about board ROM
   * @return comma separated list of available ROM features
  */
  virtual const std::string& GetROMString() { return m_rom; }
  /**
   * @brief get brief information about board RAM
   * @return comma separated list of available RAM features
  */
  virtual const std::string& GetRAMString() { return m_ram; }

  /**
   * @brief get board name to present to user
   * @return string with board name and revision
  */
   std::string GetDisplayName() const override;

  /**
   * @brief get board name
   * @return board name string
  */
   const std::string& GetName() const override;

  /**
   * @brief get board vendor
   * @return board vendor as string
  */
   const std::string& GetVendorString() const override;

  /**
   * @brief get board version string from its revision
   * @return version string
  */
   const std::string& GetVersionString() const override { return GetRevision(); };

  /**
   * @brief get board revision
   * @return revision string
  */
  const std::string& GetRevision() const;

  /**
   * @brief collect list of all mounted and compatible devices
   * @param bCompatible boolean flag to collect compatible devices
   * @param bCMounted boolean flag to collect mounted devices
   * @param devices list of RteItem pointers to fill
  */
  virtual void GetDevices(Collection<RteItem*>& devices, bool bCompatible = true, bool bMounted = true ) const;
  /**
   * @brief collect list of all mounted devices
   * @param mountedDevices list of RteItem pointers to fill
  */
  virtual void GetMountedDevices(Collection<RteItem*>& mountedDevices) const;
  /**
   * @brief collect list of all compatible devices
   * @param compatibleDevices list of RteItem pointers to fill
  */
  virtual void GetCompatibleDevices(Collection<RteItem*>& compatibleDevices) const;

  /**
   * @brief get vendor name for a mounted/compatible device, e.g. when device information is not available (a DFP pack is not installed)
   * @param devName name of a mounted or compatible device
   * @return vendor name
  */
  std::string GetDeviceVendorName(const std::string& devName) const;

  /**
   * @brief check if board has mounted device for given attributes
   * @param deviceAttributes device attributes to match
   * @return true if at least one mounted device matches supplied attributes
  */
  bool HasMountedDevice(const XmlItem& deviceAttributes) const;

  /**
   * @brief check if board has an MCU device
   * @return true if at least one mounted device contains device with Dname != "NO_MCU"
  */
  bool HasMCU() const;

  /**
   * @brief check if board has mounted or compatible device for given attributes
   * @param deviceAttributes device attributes to match
   * @param bOnlyMounted flag to check only mounted devices
   * @return true if at least one mounted or compatible device matches supplied attributes
  */
  bool HasCompatibleDevice(const XmlItem& deviceAttributes, bool bOnlyMounted = false) const;

  /**
   * @brief check if supplied device attributes match supplied attributes describing mounted or compatible device
   * @param deviceAttributes device attributes to match
   * @param boardDeviceAttributes mounted or compatible device attributes
   * @return true if attributes match
  */
  static bool IsDeviceCompatible(const XmlItem& deviceAttributes, const RteItem& boardDeviceAttributes);

  /**
   * @brief collect board books
   * @param books map of book name to book title pairs
  */
  void GetBooks(std::map<std::string, std::string>& books) const; // collects board books as name-title collection

  /**
   * @brief get child RteItem describing debug port for given processor name and device index
   * @param pname processor name. Can be empty if board has only one single-core device
   * @param deviceIndex 0-based device index if board has multiple devices
   * @return pointer to RteItem corresponding <debugProbe> child element
  */
  RteItem* GetDebugProbe(const std::string& pname, int deviceIndex = -1);

  /**
   * @brief collect list of algorithms provided by the board
   * @param algos list of RteItem pointers to fill
   * @return reference to algos parameter
  */
  Collection<RteItem*>& GetAlgorithms(Collection<RteItem*>& algos) const;

  /**
 * @brief collect list of memory provided by the board
 * @param mems list of RteItem pointers to fill
 * @return reference to mems parameter
 */
  Collection<RteItem*>& GetMemories(Collection<RteItem*>& mems) const;

public:
  /**
   * @brief clear internal data and calls base class implementation
  */
   void Clear() override;

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


protected:

  /**
   * @brief construct board ID
   * @return board ID string including name and revision
  */
   std::string ConstructID() override;

  /**
   * @brief add memory description to internal description string
   * @param memStr reference to memory string to collect
   * @param xmlItem pointer to XmlItem with ROM or RAM tags
  */
  void AddMemStr(std::string& memStr, const XmlItem* xmlItem);

protected:
  std::string m_mountedDevs; // Display std::string for mounted devices
  std::string m_mountedDevsVendor; // Display std::string for mounted devices with vendors
  std::string m_rom; // brief ROM info
  std::string m_ram; // brief RAM info
};


/**
 * @brief class to process <boards> section of pdsc file
*/
class RteBoardContainer : public RteItem
{
public:
  /**
  * @brief constructor
  * @param parent pointer to RteItem parent
 */
  RteBoardContainer(RteItem* parent);

  /**
   * @brief find board with given ID
   * @param id board ID to search for
   * @return pointer to RteBoard item if found, nullptr otherwise
  */
  RteBoard* GetBoard(const std::string& id) const;

  /**
   * @brief create a new instance of type RteItem
   * @param tag name of tag
   * @return pointer to instance of type RteItem
  */
  RteItem* CreateItem(const std::string& tag) override;
};

// helper compare class for std::map container, returns true if a > b

/**
 * @brief comparator class to sort RteBoardMap collection
*/
class BoardLessNoCase
{
public:
  bool operator()(const std::string& a, const std::string& b) const {
    return AlnumCmp::CompareLen(RteUtils::RemovePrefixByString(a, "::"), RteUtils::RemovePrefixByString(b, "::"), false) < 0;
  }
};
/**
 * @brief std::map to store RteBoard pointers with alpha-numeric sorting over board name
*/
typedef std::map<std::string, RteBoard*, BoardLessNoCase > RteBoardMap;

#endif // RteBoard_H
