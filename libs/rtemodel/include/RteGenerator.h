#ifndef RteGenerator_H
#define RteGenerator_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteGenerator.h
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RtePackage.h"
#include "RteDevice.h"

class RteFile;
class RteFileContainer;
class RteTarget;

/** RTE Generator
*
* Contains generator properties
*/

/**
 * @brief class to support handling of  <generator> elements in pdsc file
*/
class RteGenerator : public RteItem
{
public:

  /**
  * @brief constructor
  * @param parent pointer to RteItem parent
  */
  RteGenerator(RteItem* parent, bool bExternal = false);

  /**
   * @brief virtual destructor
  */
   ~RteGenerator() override;

  /**
   * @brief get generator "run" attribute
   * @return generator run command
  */
   const std::string& GetRunAttribute() const { return GetAttribute("run"); }

  /**
   * @brief get generator "path" attribute
   * @return generator path
  */
   const std::string& GetPathAttribute() const { return GetAttribute("path"); }

  /**
   * @brief get generator command without expansion
   * @param hostType host type to match, empty to match current host
   * @return generator command for specified host type
  */
  virtual const std::string GetCommand(const std::string& hostType = EMPTY_STRING ) const;

  /**
  * @brief get expanded generator executable command
  * @param target pointer to RteTarget
  * @param hostType host type to match, empty to match current host
  * @return generator command for specified host type
 */
   std::string GetExecutable(RteTarget* target, const std::string& hostType = EMPTY_STRING) const;

  /**
   * @brief get item containing command line arguments
   * @param type generator type ("exe", "web", etc. ).
   * Default is empty for backward compatibility, equals to "exe"
   * @return command line arguments (default for "exe")
  */
  RteItem* GetArgumentsItem(const std::string& type = EMPTY_STRING) const;

  /**
   * @brief get files to add to project when using the generator
   * @return pointer to RteFileContainer containing file items, nullptr if no files need to be added
  */
  RteFileContainer* GetProjectFiles() const { return m_files; }

  /**
   * @brief get generator device name
   * @return device name
  */
   const std::string& GetDeviceName() const override { return m_pDeviceAttributes ? m_pDeviceAttributes->GetDeviceName() : EMPTY_STRING; }

  /**
   * @brief get generator device vendor
   * @return device vendor
  */
   const std::string& GetDeviceVendor() const override { return m_pDeviceAttributes ? m_pDeviceAttributes->GetDeviceVendor() : EMPTY_STRING; }

  /**
   * @brief get device variant
   * @return device variant name
  */
   const std::string& GetDeviceVariantName() const override { return m_pDeviceAttributes ? m_pDeviceAttributes->GetDeviceVariantName() : EMPTY_STRING; }

  /**
   * @brief get processor name
   * @return processor name
  */
   const std::string& GetProcessorName() const override { return m_pDeviceAttributes ? m_pDeviceAttributes->GetProcessorName() : EMPTY_STRING; }

  /**
   * @brief get all device attributes
   * @return device attributes as a reference to RteItem
  */
  const RteItem& GetDeviceAttributes() const { return m_pDeviceAttributes ? *m_pDeviceAttributes : RteItem::EMPTY_RTE_ITEM; }

  /**
   * @brief get generator group name to use in project
   * @return string in the format ":GetName()::Common Sources"
  */
  std::string GetGeneratorGroupName() const;

  /**
   * @brief get gpdsc file name
   * @return  gpdsc file name
  */
  const std::string& GetGpdsc() const;

 /**
  * @brief get generator working directory
  * @return working directory value
 */
  const std::string& GetWorkingDir() const;

 /**
  * @brief return value of attribute "download-url"
  * @return value of attribute "download-url"
  */
  const std::string& GetURL() const override { return GetAttribute("download-url"); }

 /**
 * @brief get all arguments as vector for the given host type
 * @param target pointer to RteTarget
 * @param hostType host type, empty to match current host
 * @param dryRun include dry-run arguments
 * @return vector of arguments consisting of switch and value in pairs
*/
  std::vector<std::pair<std::string, std::string> > GetExpandedArguments(RteTarget* target,const std::string& hostType = EMPTY_STRING, bool dryRun = false) const;

 /**
   * @brief get full command line with arguments and expanded key sequences for specified target
   * @param target pointer to RteTarget
   * @param hostType host type, empty to match current host
   * @param dryRun include dry-run arguments
   * @return expanded command line with arguments, properly quoted
  */
  std::string GetExpandedCommandLine(RteTarget* target, const std::string& hostType = EMPTY_STRING, bool dryRun = false) const;

  /**
   * @brief get absolute path to gpdsc file for specified target
   * @param target pointer to RteTarget
   * @param optional generator destination directory
   * @return absolute gpdsc filename
  */
  std::string GetExpandedGpdsc(RteTarget* target, const std::string& genDir = EMPTY_STRING) const;

  /**
   * @brief get absolute path to working directory for specified target
   * @param target pointer to RteTarget
   * @param optional generator destination directory
   * @return absolute path to working directory
  */
  std::string GetExpandedWorkingDir(RteTarget* target, const std::string& genDir = EMPTY_STRING) const;

  /**
   * @brief get command line for a web application with expanded key sequences
   * @param target pointer to RteTarget
   * @return expanded command line for web application
  */
  std::string GetExpandedWebLine(RteTarget* target) const;

  /**
   * @brief check if this generator uses as executable
   * @return true if uses executable
  */
  bool HasExe() const;

  /**
   * @brief check if this generator uses web application
   * @return true if uses web application
  */
  bool HasWeb() const;

public:

  /**
   * @brief get generator name
   * @return generator ID
  */
   const std::string& GetName() const override { return GetAttribute("id"); }

  /**
   * @brief get generator name
   * @return generator ID
  */
   const std::string& GetGeneratorName() const override { return GetName(); }

  /**
   * @brief clear internal data structures
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
  RteItem* CreateItem(const std::string& tag) override;

 /**
  * @brief check whether the generator can be run in dry-run mode
  * @param hostType host type, empty to match current host
  * @return true if the generator is dry-run capable, false otherwise
  */
  bool IsDryRunCapable(const std::string& hostType = EMPTY_STRING) const;

 /**
  * @brief check if the generator is external
  * @return true if the generator is external, default returns false
  */
  bool IsExternal() const { return m_bExternal; }


protected:
  /**
   * @brief construct generator ID
   * @return value of "id" attribute
  */
   std::string ConstructID() override { return GetAttribute("id"); }

private:
  RteItem* m_pDeviceAttributes;
  RteFileContainer* m_files;
  bool m_bExternal;
};

/**
 * @brief class to support <generator> element in *.pdsc files
*/
class RteGeneratorContainer : public RteItem
{
public:

  /**
  * @brief constructor
  * @param parent pointer to RteItem parent
  */
  RteGeneratorContainer(RteItem* parent);

  /**
   * @brief get generator with specified ID
   * @param id generator ID
   * @return pointer to RteGenerator if found, nullptr otherwise
  */
  RteGenerator* GetGenerator(const std::string& id) const;

  /**
   * @brief create a new instance of type RteItem
   * @param tag name of tag
   * @return pointer to instance of type RteItem
  */
  RteItem* CreateItem(const std::string& tag) override;
};

#endif // RteGenerator_H
