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
  RteGenerator(RteItem* parent);

  /**
   * @brief virtual destructor
  */
  virtual ~RteGenerator() override;

  /**
   * @brief get generator command without expansion
   * @param hostType host type to match, empty to match current host
   * @return generator command for specified host type
  */
  const std::string GetCommand(const std::string& hostType = EMPTY_STRING ) const;

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
  virtual const std::string& GetDeviceName() const override { return m_deviceAttributes.GetDeviceName(); }

  /**
   * @brief get generator device vendor
   * @return device vendor
  */
  virtual const std::string& GetDeviceVendor() const override { return m_deviceAttributes.GetDeviceVendor(); }

  /**
   * @brief get device variant
   * @return device variant name
  */
  virtual const std::string& GetDeviceVariantName() const override { return m_deviceAttributes.GetDeviceVariantName(); }

  /**
   * @brief get processor name
   * @return processor name
  */
  virtual const std::string& GetProcessorName() const override { return m_deviceAttributes.GetProcessorName(); }

  /**
   * @brief get all device attributes
   * @return device attributes as a reference to RteItem
  */
  const RteItem& GetDeviceAttributes() const { return m_deviceAttributes; }

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
  const std::string& GetWorkingDir() const { return GetItemValue("workingDir"); }

 /**
 * @brief get all arguments as vector similar to argv for the given host type
 * @param target pointer to RteTarget
 * @param hostType host type, empty to match current host
 * @return vector of arguments including command line at 0 vector element
*/
  std::vector<std::string> GetExpandedArgv(RteTarget* target, const std::string& hostType = EMPTY_STRING) const;

  /**
   * @brief get full command line with arguments and expanded key sequences for specified target
   * @param target pointer to RteTarget
   * @param hostType host type, empty to match current host
   * @return expanded command line with arguments, properly quoted
  */
  std::string GetExpandedCommandLine(RteTarget* target, const std::string& hostType = EMPTY_STRING) const;

  /**
   * @brief get absolute path to gpdsc file for specified target
   * @param target pointer to RteTarget
   * @return absolute gpdsc filename
  */
  std::string GetExpandedGpdsc(RteTarget* target) const;

  /**
   * @brief get absolute path to working directory for specified target
   * @param target pointer to RteTarget
   * @return absolute path to working directory
  */
  std::string GetExpandedWorkingDir(RteTarget* target) const;

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
  virtual const std::string& GetName() const override { return GetAttribute("id"); }

  /**
   * @brief get generator name
   * @return generator ID
  */
  virtual const std::string& GetGeneratorName() const override { return GetName(); }

public:
  /**
   * @brief clear internal data structures
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
   * @brief construct generator ID
   * @return value of "id" attribute
  */
  virtual std::string ConstructID() override { return GetAttribute("id"); }

private:
  RteItem m_deviceAttributes;
  RteFileContainer* m_files;
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
   * @brief process a single XMLTreeElement during construction
   * @param xmlElement pointer to XMLTreeElement to process
   * @return true if successful
   */
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement) override;
};

#endif // RteGenerator_H
