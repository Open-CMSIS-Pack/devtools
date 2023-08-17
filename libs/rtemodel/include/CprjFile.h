#ifndef CprjFile_H
#define CprjFile_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file CprjFile.h
* @brief CMSIS RTE Data Model : cprj file support
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include <optional>
#include "RteGenerator.h"

#include "RtePackage.h"
#include "RteDevice.h"

class RteFile;
class RteFileContainer;
class RteComponentContainer;
class RteTarget;
class RteDeviceProperty;

/**
 * @brief class representing tag 'target' in cprj file
*/
class CprjTargetElement : public RteItem
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent element of type RteItem
  */
  CprjTargetElement(RteItem* parent);

  /**
   * @brief destructor
  */
  ~CprjTargetElement() override;

  /**
   * @brief clean up the object
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
   * @brief validate device and board for the target determined by RteModel object
   * @param targetModel given CMSIS RTE data
   * @return RteDevice pointer if successful, otherwise nullptr
  */
  RteDevice* ResolveDeviceAndBoard(RteModel* targetModel);

  /**
   * @brief getter for the selected device
   * @return pointer to RteDevice object
  */
  RteDevice* GetDevice() const { return m_device; }

  /**
   * @brief getter for selected board
   * @return pointer to RteBoard object
  */
  RteBoard* GetBoard() const { return m_board; }

  /**
   * @brief getter for effective properties
   * @return collection of properties mapped to RteDeviceProperty pointers
  */
  const RteDevicePropertyMap& GetEffectiveProperties() const { return m_effectiveProperties; }

  /**
   * @brief getter for processor properties
   * @return RteDeviceProperty pointer
  */
  const RteDeviceProperty* GetProcessorProperty() const { return m_processor; }

  /**
   * @brief getter for attribute "Dcore"
   * @return value of attribute "Dcore"
  */
  const std::string & GetDcore() const;

  /**
   * @brief getter for display board name
   * @return board name string
  */
  std::string GetBoardDisplayName() const;

  /**
   * @brief getter for device name specified by attribute "Dname"
   * @return device name
  */
  std::string GetFullDeviceName() const override { return GetAttribute("Dname"); }

  /**
   * @brief getter for processor name
   * @return string containing processor name
  */
  std::string GetPname() const;

  /**
   * @brief getter for index of startup memory in list of assigned memories
   * @return integer as index  of startup memory in list of assigned memories
  */
  int GetStartupMemoryIndex() const;

  /**
   * @brief getter for device options
   * @return collection of option ID mapped to pointer of type RteDeviceProperty
  */
  const std::map<std::string, RteDeviceProperty*>& GetDeviceOptions() const { return m_deviceOptions; }

  /**
   * @brief getter for device option determined by the given option ID
   * @param id given option ID
   * @return RteDeviceProperty pointer
  */
  RteDeviceProperty* GetDeviceOption(const std::string& id) const;

  /**
   * @brief getter for collection of assigned memories
   * @return collection of index mapped to pointer of type RteItem
  */
  const std::map<int, RteItem*>& GetAssignedMemory() const { return m_assignedMemory; }

  /**
   * @brief setter for assigned memory with given index as key and given RteItem object as value
   * @param index integer as key to set assigned memory
   * @param mem RteItem pointer
  */
  void AssignMemory(int index, RteItem* mem) { if (index >= 0 && mem) m_assignedMemory[index] = mem; }

  /**
   * @brief getter for element "debugProbe" in cprj file
   * @return RteItem pointer
  */
  RteItem* GetDebugProbeOption() const { return m_debugProbeOption; }

  /**
   * @brief getter for build output option specified by "output" in cprj file
   * @return RteItem pointer
  */
  RteItem* GetBuildOption() const { return m_buildOption; }

  /**
   * @brief getter for "stack" section specified in cprj file
   * @return RteItem pointer
  */
  RteItem* GetStackOption() const { return m_stackOption; }

  /**
   * @brief getter for "heap" section specified in cprj file
   * @return RteItem pointer
  */
  RteItem* GetHeapOption() const { return m_heapOption; }

  /**
   * @brief getter for target build flags depending on language tag and toolchain
   * @param tag tag specific to language, e.g. "cflags", "cxxflags"
   * @param compiler given toolchain
   * @return build flag string specific to given language
  */
  const std::string &GetTargetBuildFlags(const std::string &tag, const std::string &compiler) const;

  /**
   * @brief determine build flags specific to C language for given toolchain
   * @param compiler given toolchain
   * @return build flags specific to C language
  */
  const std::string &GetCFlags(const std::string &compiler) const;

  /**
   * @brief determine build flags specific to C++ language for given toolchain
   * @param compiler given toolchain
   * @return build flags specific to C++ language
  */
  const std::string &GetCxxFlags(const std::string &compiler) const;

  /**
   * @brief determine build flags specific to assembly language for given toolchain
   * @param compiler given toolchain
   * @return build flags specific to assembly language
  */
  const std::string &GetAsFlags(const std::string &compiler) const;

  /**
   * @brief determine build flags specific to library for given toolchain
   * @param compiler given toolchain
   * @return build flags specific to library
  */
  const std::string &GetArFlags(const std::string &compiler) const;

  /**
   * @brief determine build flags specific to linker for the given toolchain
   * @param compiler given toolchain
   * @return build flags specific to linker
  */
  const std::string &GetLdFlags(const std::string &compiler) const;

  /**
   * @brief determine build flags specific to C object linker for the given toolchain
   * @param compiler given toolchain
   * @return build flags specific to linker
  */
  const std::string& GetLdCFlags(const std::string& compiler) const;

  /**
   * @brief determine build flags specific to Cpp object linker for the given toolchain
   * @param compiler given toolchain
   * @return build flags specific to linker
   */
  const std::string& GetLdCxxFlags(const std::string& compiler) const;

  /**
   * @brief setter for target build flags depending on language tag and compiler
   * @param tag tag specific to language, e.g. "cflags", "cxxflags"
   * @param compiler given toolchain
   * @param value value to set
  */
  void SetTargetBuildFlags(const std::string &tag, const std::string &compiler, const std::string &value);

  /**
   * @brief setter for build flags specific to C language and given toolchain
   * @param flags string containing build flags
   * @param compiler given toolchain
  */
  void SetCFlags(const std::string &flags, const std::string &compiler);

  /**
   * @brief setter for build flags specific to C++ language and given toolchain
   * @param flags string containing build flags
   * @param compiler given toolchain
  */
  void SetCxxFlags(const std::string &flags, const std::string &compiler);

  /**
   * @brief setter for build flags specific to assembly language and given toolchain
   * @param flags string containing build flags
   * @param compiler given toolchain
  */
  void SetAsFlags(const std::string &flags, const std::string &compiler);

  /**
   * @brief setter for build flags specific to library and given toolchain
   * @param flags string containing build flags
   * @param compiler given toolchain
  */
  void SetArFlags(const std::string &flags, const std::string &compiler);

  /**
   * @brief setter for build flags specific to linker and given toolchain
   * @param flags string containing build flags
   * @param compiler given toolchain
  */
  void SetLdFlags(const std::string &flags, const std::string &compiler);

  /**
   * @brief setter for build flags specific to C Object linker and given toolchain
   * @param flags string containing build flags
   * @param compiler given toolchain
  */
  void SetLdCFlags(const std::string &flags, const std::string &compiler);

  /**
   * @brief setter for build flags specific to Cpp Object linker and given toolchain
   * @param flags string containing build flags
   * @param compiler given toolchain
  */
  void SetLdCxxFlags(const std::string &flags, const std::string &compiler);

protected:
  RteDevice* ResolveDevice(RteModel* targetModel);
  void CollectEffectiveProperties(const std::string& procName);
  void CollectEffectiveProperties(RteItem* item, const std::string& procName);


private:
  std::map<std::string, RteDeviceProperty*> m_deviceOptions;
  RteItem* m_debugProbeOption;
  RteItem* m_buildOption;
  RteItem* m_stackOption;
  RteItem* m_heapOption;

  RteDevice* m_device; // resolved device
  RteBoard* m_board; // resolved board if any
  RteDeviceProperty* m_processor; // processor property
  RteDevicePropertyMap m_effectiveProperties; // merged properties
  std::map<int, RteItem*> m_assignedMemory;
  RteItem* m_startupMemory;

};

/**
 * @brief class representing cprj file
*/
class CprjFile : public RteRootItem
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent element of type RteItem
  */
  CprjFile(RteItem* parent);

  /**
   * @brief destructor
  */
  ~CprjFile() override;

  /**
   * @brief clean up the instance
  */
  void Clear() override;

  /**
   * @brief create a new instance of type RteItem
   * @param tag name of tag
   * @return pointer to instance of type RteItem
  */
  RteItem* CreateItem(const std::string& tag) override;

  /**
   * @brief getter for node "target" specified in cprj file
   * @return pointer to CprjTargetElement object
  */
  CprjTargetElement* GetTargetElement() const { return m_cprjTargetElement; }

  /**
   * @brief getter for project files
   * @return pointer to RteFileContainer object
  */
  RteFileContainer* GetProjectFiles() const { return m_files; }

  /**
   * @brief getter for node "info" specified in cprj file
   * @return pointer to RteItem object
  */
  RteItem* GetProjectInfo() const;

  const std::string& GetRteFolder() const override;
  /**
   * @brief getter for list of components specified for the project
   * @return list of RteItem pointers
  */
  const Collection<RteItem*>& GetProjectComponents() const;

  /**
   * @brief getter for node "layers" specified in cprj file
   * @return list of RteItem pointers
  */
  const Collection<RteItem*>& GetProjectLayers() const;

  /**
   * @brief getter for node "packages" specified in cprj file
   * @return list of RteItem pointers
  */
  const Collection<RteItem*>& GetPackRequirements() const;

  /**
   * @brief getter for node "compilers" specified in cprj file
   * @return list of RteItem pointers
  */
  const Collection<RteItem*>& GetCompilerRequirements() const;

  /**
   * @brief check if the file is generated by a program associated with an RteGenerator object
   * @return true if instance successfully generated. Default returns false
  */
  bool IsGenerated() const override { return false; }

protected:
   std::string ConstructID() override;

  CprjTargetElement* m_cprjTargetElement;
  RteFileContainer* m_files;
};

#endif // CprjFile_H
