/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRPARSER_H
#define PROJMGRPARSER_H

#include <string>
#include <vector>

 /**
  * @brief compiler misc controls structure at components/groups/files level containing
  *        options for all tools,
  *        options for assembler,
  *        options for c compiler,
  *        options for c++ compiler
 */
struct MiscItem {
  std::vector<std::string> all;
  std::vector<std::string> as;
  std::vector<std::string> c;
  std::vector<std::string> cpp;
};

/**
 * @brief compiler misc controls structure at target level containing
 *        options for all tools,
 *        options for assembler,
 *        options for c compiler,
 *        options for c++ compiler,
 *        options for linker,
 *        options for archiver
*/
struct MiscTargetItem {
  std::vector<std::string> all;
  std::vector<std::string> as;
  std::vector<std::string> c;
  std::vector<std::string> cpp;
  std::vector<std::string> ld;
  std::vector<std::string> ar;
};

/**
 * @brief component item containing
 *        component identifier,
 *        component toolchain settings
*/
struct ComponentItem {
  std::string component;
  // TODO: Add toolchain settings;
};

/**
 * @brief processor item containing
 *        processor endianess,
 *        processor fpu,
 *        processor mpu,
 *        processor dsp,
 *        processor mve,
 *        processor trustzone,
 *        processor secure,
*/
struct ProcessorItem {
  std::string endian;
  std::string fpu;
  std::string mpu;
  std::string dsp;
  std::string mve;
  std::string trustzone;
  std::string secure;
};

/**
 * @brief target item containing
 *        device identifier,
 *        board identifier,
 *        processor attributes
*/
struct TargetItem {
  std::string device;
  std::string board;
  ProcessorItem processor;
};

/**
 * @brief file node containing
 *        file path,
 *        file category,
 *        file toolchain settings
*/
struct FileNode {
  std::string file;
  std::string category;
  // TODO: Add toolchain settings;
};

/**
 * @brief group node containing
 *        group name,
 *        children files,
 *        children groups
 *        group toolchain settings
*/
struct GroupNode {
  std::string group;
  std::vector<FileNode> files;
  std::vector<GroupNode> groups;
  // TODO: Add toolchain settings;
};

/**
 * @brief solution item containing
 *        TBD
*/
struct CsolutionItem {
   // TBD
};

/**
 * @brief cproject item containing
 *        project name,
 *        project description,
 *        project toolchain,
 *        project output type,
 *        target elements (board, device, processor),
 *        list of required packages,
 *        list of required components,
 *        list of user groups
*/
struct CprojectItem {
  std::string name;
  std::string description;
  std::string toolchain;
  std::string outputType;
  TargetItem target;
  std::vector<std::string> packages;
  std::vector<ComponentItem> components;
  std::vector<GroupNode> groups;
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
   * @param cproject reference to cproject struct
  */
  bool ParseCproject(const std::string& input, CprojectItem& cproject);

  /**
   * @brief parse csolution
   * @param input csolution.yml file
   * @param csolution reference to csolution struct
  */
  bool ParseCsolution(const std::string& input, CsolutionItem& csolution);
};

#endif  // PROJMGRPARSER_H
