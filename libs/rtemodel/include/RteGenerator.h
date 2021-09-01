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
class RteGenerator : public RteItem
{
public:
  RteGenerator(RteItem* parent);
  virtual ~RteGenerator() override;

  const std::string& GetCommand() const;// cmd only
  RteItem* GetArgumentsItem(const std::string& type = EMPTY_STRING) const;

  RteFileContainer* GetProjectFiles() const { return m_files; }

  virtual const std::string& GetDeviceName() const override { return m_deviceAttributes.GetDeviceName(); }
  virtual const std::string& GetDeviceVendor() const override { return m_deviceAttributes.GetDeviceVendor(); }
  virtual const std::string& GetDeviceVariantName() const override { return m_deviceAttributes.GetDeviceVariantName(); }
  virtual const std::string& GetProcessorName() const override { return m_deviceAttributes.GetProcessorName(); }

  const RteAttributes& GetDeviceAttributes() const { return m_deviceAttributes; }

  std::string GetGeneratorGroupName() const;

  const std::string& GetGpdsc() const;
  const std::string& GetWorkingDir() const { return GetItemValue("workingDir"); }

  std::string GetExpandedCommandLine(RteTarget* target) const; // cmd + arguments with expanded sequences
  std::string GetExpandedGpdsc(RteTarget* target) const;
  std::string GetExpandedWorkingDir(RteTarget* target) const;

  std::string GetExpandedWebLine(RteTarget* target) const; // url + arguments with expanded key sequences

  bool HasExe() const;
  bool HasWeb() const;

public:
  virtual const std::string& GetName() const override { return GetAttribute("id"); }
  virtual const std::string& GetGeneratorName() const override { return GetName(); }

public:
  virtual void Clear() override;

protected:
  /**
   * @brief process a single XMLTreeElement during construction
   * @param xmlElement pointer to XMLTreeElement to process
   * @return true if successful
   */
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement) override;
  virtual std::string ConstructID() override { return GetAttribute("id"); }

private:
  RteAttributes m_deviceAttributes;
  RteFileContainer* m_files;
};


// container class to keep Rte tree uniform
class RteGeneratorContainer : public RteItem
{
public:
  RteGeneratorContainer(RteItem* parent);

  RteGenerator* GetGenerator(const std::string& id) const;
  /**
   * @brief process a single XMLTreeElement during construction
   * @param xmlElement pointer to XMLTreeElement to process
   * @return true if successful
   */
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement) override;
};

#endif // RteGenerator_H
