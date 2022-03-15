/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRPARSER_H
#define PROJMGRPARSER_H

#include <map>
#include <string>
#include <vector>

/**
* @brief type pair containing
*        build-type,
*        target-type
*/
struct TypePair {
  std::string build;
  std::string target;
};

/**
 * @brief include/exclude types
 *        for-type (include)
 *        not-for-type (exclude)
*/
struct TypeFilter {
  std::vector<TypePair> include;
  std::vector<TypePair> exclude;
};

/**
 * @brief compiler misc controls
 *        compiler name,
 *        options for assembler,
 *        options for c compiler,
 *        options for c++ compiler,
 *        options for c and c++ compiler,
 *        options for linker,
 *        options for archiver
*/
struct MiscItem {
  std::string compiler;
  std::vector<std::string> as;
  std::vector<std::string> c;
  std::vector<std::string> cpp;
  std::vector<std::string> c_cpp;
  std::vector<std::string> link;
  std::vector<std::string> lib;
};

/**
 * @brief pack item containing
 *        pack name
 *        pack path
 *        type filter
*/
struct PackItem {
  std::string pack;
  std::string path;
  TypeFilter type;
};

/**
 * @brief processor item containing
 *        processor fpu,
 *        processor trustzone,
 *        processor endianess
*/
struct ProcessorItem {
  std::string fpu;
  std::string trustzone;
  std::string endian;
};

/**
 * @brief build types containing
 *        toolchain,
 *        optimization level,
 *        debug information,
 *        preprocessor defines,
 *        preprocessor undefines,
 *        include paths,
 *        exclude paths,
 *        misc compiler controls,
 *        platform processor
*/
struct BuildType {
  std::string compiler;
  std::string optimize;
  std::string debug;
  std::string warnings;
  std::vector<std::string> defines;
  std::vector<std::string> undefines;
  std::vector<std::string> addpaths;
  std::vector<std::string> delpaths;
  std::vector<MiscItem> misc;
  ProcessorItem processor;
};

/**
 * @brief target types containing
 *        platform board,
 *        platform device,
 *        target build settings
*/
struct TargetType {
  std::string board;
  std::string device;
  BuildType build;
};

/**
 * @brief rte directory item containing
 *        rte directory path,
 *        type inclusion
*/
struct RteDirItem {
  std::string dir;
  TypeFilter type;
};

/**
 * @brief component item containing
 *        component identifier,
 *        component build settings,
 *        type inclusion
*/
struct ComponentItem {
  std::string component;
  BuildType build;
  TypeFilter type;
};

/**
 * @brief layer item containing
 *        layer name,
 *        layer build settings,
 *        type inclusion
*/
struct LayerItem {
  std::string layer;
  BuildType build;
  TypeFilter type;
};

/**
 * @brief file node containing
 *        file path,
 *        file category,
 *        file build settings,
 *        type filter
*/
struct FileNode {
  std::string file;
  std::string category;
  BuildType build;
  TypeFilter type;
};

/**
 * @brief group node containing
 *        group name,
 *        children files,
 *        children groups,
 *        group build settings,
 *        type filter
*/
struct GroupNode {
  std::string group;
  std::vector<FileNode> files;
  std::vector<GroupNode> groups;
  BuildType build;
  TypeFilter type;
};

/**
 * @brief context descriptor containing
 *        cproject filename,
 *        target properties,
 *        type filter
*/
struct ContextDesc {
  std::string cproject;
  TargetType target;
  TypeFilter type;
};

/**
 * @brief solution item containing
 *        csolution path
 *        build types,
 *        target types,
 *        list of cprojects,
 *        list of contexts descriptors,
 *        list of packs
*/
struct CsolutionItem {
  std::string path;
  std::map<std::string, BuildType> buildTypes;
  std::map<std::string, TargetType> targetTypes;
  std::vector<std::string> cprojects;
  std::vector<ContextDesc> contexts;
  std::vector<PackItem> packs;
};

/**
 * @brief cproject item containing
 *        project name,
 *        project path,
 *        project output type,
 *        project target properties,
 *        list of required components,
 *        list of user groups,
 *        list of layers
*/
struct CprojectItem {
  std::string name;
  std::string path;
  std::string outputType;
  TargetType target;
  std::vector<RteDirItem> rteDirs;
  std::vector<ComponentItem> components;
  std::vector<GroupNode> groups;
  std::vector<LayerItem> clayers;
};

/**
 * @brief interface item containing
 *        map of provided interfaces,
 *        map of consumed interfaces
*/
struct InterfaceItem {
  std::map<std::string, std::string> provides;
  std::map<std::string, std::string> consumes;
};

/**
 * @brief clayer item containing
 *        layer name,
 *        layer path,
 *        layer output type,
 *        layer target properties,
 *        list of required components,
 *        list of user groups,
 *        list of interfaces
*/
struct ClayerItem {
  std::string name;
  std::string path;
  std::string outputType;
  TargetType target;
  std::vector<RteDirItem> rteDirs;
  std::vector<ComponentItem> components;
  std::vector<GroupNode> groups;
  std::vector<InterfaceItem> interfaces;
};

/**
 * @brief projmgr parser class for public interfacing
*/
class ProjMgrParser {
public:
  /**
   * @brief class constructor
  */
  ProjMgrParser(void);

  /**
   * @brief class destructor
  */
  ~ProjMgrParser(void);

  /**
   * @brief parse cproject
   * @param input cproject.yml file
   * @param boolean parse single project, default false
  */
  bool ParseCproject(const std::string& input, bool checkSchema, bool single = false);

  /**
   * @brief parse csolution
   * @param input csolution.yml file
  */
  bool ParseCsolution(const std::string& input, bool checkSchema);

  /**
   * @brief parse clayer
   * @param input clayer.yml file
  */
  bool ParseClayer(const std::string& input, bool checkSchema);

  /**
   * @brief get csolution
   * @return csolution item
  */
  CsolutionItem& GetCsolution(void);

  /**
   * @brief get cprojects
   * @return cprojects vector
  */
  std::map<std::string, CprojectItem>& GetCprojects(void);

  /**
   * @brief get clayers
   * @return clayers vector
  */
  std::map<std::string, ClayerItem>& GetClayers(void);

protected:
  CsolutionItem m_csolution;
  std::map<std::string, CprojectItem> m_cprojects;
  std::map<std::string, ClayerItem> m_clayers;
};

#endif  // PROJMGRPARSER_H
