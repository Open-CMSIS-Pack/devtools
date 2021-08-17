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
  virtual ~RteBoard() override;
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
  virtual std::string GetDisplayName() const override;

  /**
   * @brief collect list of all mounted and compatible devices
   * @param devices list of RteItem pointers to fill
  */
  virtual void GetDevices(std::list<RteItem*>& devices) const; //
  /**
   * @brief collect list of all mounted devices
   * @param mountedDevices list of RteItem pointers to fill
  */
  virtual void GetMountedDevices(std::list<RteItem*>& mountedDevices) const;
  /**
   * @brief collect list of all compatible devices
   * @param compatibleDevices list of RteItem pointers to fill
  */
  virtual void GetCompatibleDevices(std::list<RteItem*>& compatibleDevices) const;

  /**
   * @brief get vendor name for a mounted/compatible device, e.g. when device information is not available (a DFP pack is not installed)
   * @param devName name of a mounted or compatible device
   * @return vendor name
  */
  std::string GetDeviceVendorName(const std::string& devName) const;

  /**
   * @brief check if
   * @param deviceAttributes if attributes mounted or a compatible device match supplied device attributes
   * @return true if at least one mounted or compatible device matches supplied attributes
  */
  bool HasCompatibleDevice(const std::map<std::string, std::string>& deviceAttributes) const;

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

public:
  /**
   * @brief clear internal data and calls base class implementation
  */
  virtual void Clear() override;

protected:
  /**
   * @brief process a single XMLTreeElement during construction
   * @param xmlElement pointer to XMLTreeElement to process
   * @return true if successful
 */
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement) override;

  /**
   * @brief construct board ID
   * @return board ID string including name and revision
  */
  virtual std::string ConstructID() override;

  /**
   * @brief add memory description to internal description string
   * @param memStr reference to memory string to collect
   * @param xmlElement pointer to XMLTreeElement with ROM or RAM tags
  */
  void AddMemStr(std::string& memStr, XMLTreeElement* xmlElement);

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
   * @brief process a single XMLTreeElement during construction
   * @param xmlElement pointer to XMLTreeElement to process
   * @return true if successful
 */
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement) override;
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
