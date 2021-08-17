/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CBUILDMODEL_H
#define CBUILDMODEL_H

#include "Cbuild.h"
#include "CbuildProject.h"

#include "RteCprjModel.h"
#include "RteModel.h"

class CbuildModel {
public:
  CbuildModel();
  virtual ~CbuildModel();

  /**
   * @brief create RTE model
   * @param args arguments to construct RTE
   * @return true if the model created successfully, otherwise false
  */
  bool Create(const CbuildRteArgs& args);

  /**
   * @brief get path to RTE root directory
   * @return string containing path to RTE root directory
  */
  const std::string& GetRtePath() const
  {
    return m_rtePath;
  }

  /**
   * @brief get path to cprj project file
   * @return string containing fully qualified file path to cprj file
  */
  const std::string& GetProjectFile() const
  {
    return m_cprjFile;
  }

  /**
   * @brief get project path
   * @return string containing path to project directory
  */
  const std::string& GetProjectPath() const
  {
    return m_prjFolder;
  }

  /**
   * @brief get device name
   * @return string containing name of the device
  */
  const std::string& GetDeviceName() const
  {
    return m_deviceName;
  }

  /**
   * @brief get list of packs
   * @return list of packages
  */
  const std::set<std::string>& GetPacks() const
  {
    return m_packs;
  }

  /**
   * @brief get list of config files
   * @return list of config files
  */
  const std::map<std::string, std::string>& GetConfigFiles() const
  {
    return m_configFiles;
  }

  /**
   * @brief get list of C source files included in the project
   * @return list of key, value pair where
   *         key: group name,
   *         value: list of associated C files
  */
  const std::map<std::string, std::list<std::string>>& GetCSourceFiles() const
  {
    return m_cSourceFiles;
  }

  /**
   * @brief get list of C++ source files included in the project
   * @return list of key, value pair where
   *         key: group name,
   *         value: list of associated C++ files
  */
  const std::map<std::string, std::list<std::string>>& GetCxxSourceFiles() const
  {
    return m_cxxSourceFiles;
  }

  /**
   * @brief get list of ASM source files included in the project
   * @return list of key, value pair where
   *         key: group name,
   *         value: list of associated ASM files
  */
  const std::map<std::string, std::list<std::string>>& GetAsmSourceFiles() const
  {
    return m_asmSourceFiles;
  }

  /**
   * @brief get list of include paths used in the project
   * @return list of include paths
  */
  const std::set<std::string>& GetIncludePaths() const
  {
    return m_includePaths;
  }

  /**
   * @brief get list of libraries used in the project
   * @return list of libraries
  */
  const std::set<std::string>& GetLibraries() const
  {
    return m_libraries;
  }

  /**
   * @brief get list of defines used in project
   * @return list of defines
  */
  const std::set<std::string>& GetDefines() const
  {
    return m_defines;
  }

  /**
   * @brief get list of object files
   * @return list of object files used in project
  */
  const std::set<std::string>& GetObjects() const
  {
    return m_objects;
  }

  /**
   * @brief get compiler toolchain to be used
   * @return string containing name of compiler toolchain
  */
  const std::string& GetCompiler() const
  {
    return m_compiler;
  }

  /**
   * @brief get the compiler toolchain version
   * @return string containing version of compiler needed
  */
  const std::string& GetCompilerVersion() const
  {
    return m_compilerVersion;
  }

  /**
   * @brief get path to toolchain config file
   * @return string containing fully qualified path to toolchain config
  */
  const std::string& GetToolchainConfig() const
  {
    return m_toolchainConfig;
  }

  /**
   * @brief get linker script
   * @return string containing path to linker file
  */
  const std::string& GetLinkerScript() const
  {
    return m_linkerScript;
  }

  /**
   * @brief get compiler flags for C modules used for constructing the compiler command line
   * @return list of C flags
  */
  const std::set<std::string>& GetTargetCFlags() const
  {
    return m_targetCFlags;
  }

  /**
   * @brief get compiler flags for C++ modules used for constructing the compiler command line
   * @return list of C++ flags
  */
  const std::set<std::string>& GetTargetCxxFlags() const
  {
    return m_targetCxxFlags;
  }

  /**
   * @brief get assembler flags for ASM modules used for constructing the assembler command line
   * @return list of assembler flags
  */
  const std::set<std::string>& GetTargetAsFlags() const
  {
    return m_targetAsFlags;
  }

  /**
   * @brief get linker flags used for constructing the linker command line
   * @return list of linker flags
  */
  const std::list<std::string>& GetTargetLdFlags() const
  {
    return m_targetLdFlags;
  }

  /**
   * @brief get compiler flags for C modules contained in the component
   * @return list of key, value pair where
   *         key: component name,
   *         value: list of associated compiler flags
  */
  const std::map<std::string, std::set<std::string>>& GetCFlags() const
  {
    return m_CFlags;
  }

  /**
   * @brief get compiler flags for C++ modules contained in the component
   * @return list of key, value pair where
   *         key: component name,
   *         value: list of associated compiler flags
  */
  const std::map<std::string, std::set<std::string>>& GetCxxFlags() const
  {
    return m_CxxFlags;
  }

  /**
   * @brief get assembler flags for ASM modules contained in the component
   * @return list of key, value pair where
   *         key: component name,
   *         value: list of associated assembler flags
  */
  const std::map<std::string, std::set<std::string>>& GetAsFlags() const
  {
    return m_AsFlags;
  }

  /**
   * @brief get the list of assembler
   * @return list of key, value pair where
   *         key: component name,
   *         value: true if assembler is used, otherwise false
  */
  const std::map<std::string, bool>& GetAsm() const
  {
    return m_Asm;
  }

  /**
   * @brief get the output directory path,
            which contains binary executables, logs and map files
   * @return string containing path to output directory
  */
  const std::string& GetOutDir() const
  {
    return m_outDir;
  }

  /**
   * @brief get the intermediate files output directory path,
   *        which contains generated CMakeLists.txt, list of missing packs (cpinstall),
            command files, object files and dependency files
   * @return string containing path to intermediate directory
  */
  const std::string& GetIntDir() const
  {
    return m_intDir;
  }

  /**
   * @brief get the name of target output
   * @return target output name
  */
  const std::string& GetOutputName() const
  {
    return m_outputName;
  }

  /**
   * @brief get output type
   * @return string containing target output type
  */
  const std::string& GetOutputType() const
  {
    return m_outputType;
  }

  /**
   * @brief get list of preinclude files specific to project
   * @return list of preinclude files associated with project
  */
  const std::set<std::string>& GetPreIncludeFilesGlobal() const
  {
    return m_preIncludeFilesGlobal;
  }

  /**
   * @brief get list of preincludes files specific to component
   * @return list of key, value pair where
   *         key: component name,
   *         value: list of preinclude files associated with component
  */
  const std::map<std::string, std::set<std::string>>& GetPreIncludeFilesLocal() const
  {
    return m_preIncludeFilesLocal;
  }

  /**
   * @brief get audit data
   * @return string containing audit data (packages, components and config files information)
  */
  const std::string& GetAuditData() const
  {
    return m_auditData;
  }

  /**
   * @brief get a list of layer files
   * @return list of key, value pair where
   *         key: layer name,
   *         value: layer files associated with layer
  */
  const std::map<std::string, std::set<std::string>>& GetLayerFiles() const
  {
    return m_layerFiles;
  }

  /**
   * @brief get list of packages needed for layers
   * @return list of key, value pair where
   *         key: layer name,
   *         value: list of packages needed by the layer
  */
  const std::map<std::string, std::set<std::string>>& GetLayerPackages() const
  {
    return m_layerPackages;
  }

  /**
   * @brief get target component
   * @return pointer to cprj target
  */
  const RteTarget* GetTarget() const
  {
    return m_cprjTarget;
  }

protected:
  RtePackage        *m_cprjPack = 0;
  RteCprjProject    *m_cprjProject = 0;
  RteTarget         *m_cprjTarget = 0;

  std::string            m_rtePath;
  std::string            m_prjFolder;
  std::string            m_cprjFile;
  std::string            m_prjName;
  std::string            m_targetName;
  std::string            m_deviceName;
  std::string            m_toolchainConfigVersion;

  std::map<std::string, std::string>                m_configFiles;
  std::map<std::string, std::list<std::string>>     m_cSourceFiles;
  std::map<std::string, std::list<std::string>>     m_cxxSourceFiles;
  std::map<std::string, std::list<std::string>>     m_asmSourceFiles;
  std::set<std::string>                             m_includePaths;
  std::set<std::string>                             m_packs;
  std::string                                       m_linkerScript;
  std::set<std::string>                             m_libraries;
  std::set<std::string>                             m_defines;
  std::set<std::string>                             m_objects;
  std::set<std::string>                             m_language;
  std::string                                       m_compiler;
  std::string                                       m_compilerVersion;
  std::string                                       m_toolchainConfig;
  std::set<std::string>                             m_targetCFlags;
  std::set<std::string>                             m_targetCxxFlags;
  std::set<std::string>                             m_targetAsFlags;
  std::list<std::string>                            m_targetLdFlags;
  std::map<std::string, std::set<std::string>>      m_CFlags;
  std::map<std::string, std::set<std::string>>      m_CxxFlags;
  std::map<std::string, std::set<std::string>>      m_AsFlags;
  std::map<std::string, bool>                       m_Asm;
  std::string                                       m_outDir;
  std::string                                       m_intDir;
  std::string                                       m_outputType;
  std::string                                       m_outputName;
  std::set<std::string>                             m_preIncludeFilesGlobal;
  std::map<std::string, std::set<std::string>>      m_preIncludeFilesLocal;
  std::string                                       m_auditData;

  // layers
  std::map<std::string, std::set<std::string>>      m_layerFiles;
  std::map<std::string, std::set<std::string>>      m_layerPackages;

  void Init(const std::string &file, const std::string &rtePath);
  const bool EvaluateResult();
  const bool EvalConfigFiles();
  const bool EvalPreIncludeFiles();
  const bool EvalSourceFiles();
  const bool EvalNonRteSourceFiles();
  const bool EvalGeneratedSourceFiles();
  const bool EvalRteSourceFiles(std::map<std::string, std::list<std::string>> &cSourceFiles, std::map<std::string, std::list<std::string>> &cxxSourceFiles, std::map<std::string, std::list<std::string>> &asmSourceFiles, std::string &linkerScript);
  const bool EvalFile(RteItem* file, const std::string& group, const std::string& base, std::string& filepath);
  const bool EvalItem(RteItem* item, const std::string& groupName = std::string(), const std::string& groupLayer = std::string());
  const bool EvalItemFlags(const RteItem* item, const std::string& groupName = std::string());
  const bool GenerateRteHeaders();
  const bool EvalDeviceName();
  const bool EvalFlags();
  const bool EvalIncludesDefines();
  const bool EvalTargetOutput();
  void SetItemFlags(const RteItem* item, const std::string& name);
  const std::string GetParentName(const RteItem* item);
  const std::set<std::string>& GetParentFlags(const RteItem* item, std::map<std::string, std::set<std::string>>& flagsMap, const std::set<std::string>& targetFlags);
  const bool GenerateAuditData();
  const bool GenerateFixedCprj(const std::string& update);
  const bool EvaluateToolchainConfig(const std::string& name, const std::string& versionRange, const std::string& localPath, const std::string& compilerRoot, const std::string& ext);
  const bool GetCompatibleToolchain(const std::string& name, const std::string& versionRange, const std::string& dir, const std::string& ext);
  std::set<std::string> SplitArgs(const std::string& args, const std::string& delim=std::string(" -"), bool relativePath=true);
  std::list<std::string> SplitArgsList(const std::string& args);
  static std::set<std::string> MergeArgs(const std::set<std::string>& add, const std::set<std::string>& remove, const std::set<std::string>& reference);
  static std::string GetExtendedRteGroupName(RteItem* ci);
};

#endif /* CBUILDMODEL_H */

