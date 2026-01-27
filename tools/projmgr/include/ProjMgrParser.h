/*
 * Copyright (c) 2020-2026 Arm Limited. All rights reserved.
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
*        target-type,
*        regex pattern
*/
struct TypePair {
  std::string build;
  std::string target;
  std::string pattern;
};

/**
* @brief context name containing
*        project name,
*        build-type,
*        target-type
*/
struct ContextName {
  std::string project;
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
 *        for compiler control,
 *        options for assembler,
 *        options for c compiler,
 *        options for c++ compiler,
 *        options for c and c++ compiler,
 *        options for linker,
 *        options for linker c flags,
 *        options for linker c++ flags,
 *        options for archiver
*/
struct MiscItem {
  std::string forCompiler;
  std::vector<std::string> as;
  std::vector<std::string> c;
  std::vector<std::string> cpp;
  std::vector<std::string> c_cpp;
  std::vector<std::string> link;
  std::vector<std::string> link_c;
  std::vector<std::string> link_cpp;
  std::vector<std::string> lib;
  std::vector<std::string> library;
};

/**
 * @brief pack item containing
 *        pack name
 *        pack path
 *        type filter
 *        origin file
*/
struct PackItem {
  std::string pack;
  std::string path;
  TypeFilter type;
  std::string origin;
};

/**
 * @brief resolved pack item containing,
 *        pack ID,
 *        list of selected-by-pack expressions (original expressions causing this pack to be added)
*/
struct ResolvedPackItem {
  std::string pack;
  std::vector<std::string> selectedByPack;
};

/**
 * @brief processor item containing
 *        processor fpu,
 *        processor dsp,
 *        processor mve,
 *        processor trustzone,
 *        processor endianess,
 *        branch protection
*/
struct ProcessorItem {
  std::string fpu;
  std::string dsp;
  std::string mve;
  std::string trustzone;
  std::string endian;
  std::string branchProtection;
};

/**
 * @brief memory item containing
 *        name
 *        access
 *        start
 *        size
 *        algorithm
*/
struct MemoryItem {
  std::string name;
  std::string access;
  std::string start;
  std::string size;
  std::string algorithm;
};

/**
 * @brief telnet item containing
 *        mode
 *        port
 *        file
 *        pname
*/
struct TelnetItem {
  std::string mode;
  std::string port;
  std::string file;
  std::string pname;
};

/**
 * @brief custom item containing
 *        scalar
 *        array
 *        map
*/
struct CustomItem {
  std::string scalar;
  std::vector<CustomItem> vec;
  std::vector<std::pair<std::string, CustomItem>> map;
};

/**
 * @brief debugger item containing
 *        name of debug configuration
 *        debug protocol (jtag or swd)
 *        debug clock speed
 *        debug configuration file
 *        start pname
 *        telnet options
 *        custom properties
*/
struct DebuggerItem {
  std::string name;
  std::string protocol;
  std::string clock;
  std::string dbgconf;
  std::string startPname;
  std::vector<TelnetItem> telnet;
  CustomItem custom;
};

/**
 * @brief target set image containing
 *        project context
 *        image file
 *        info brief description
 *        type specifies an explicit file type
 *        load mode
 *        load offset
 *        processor name associated with specified image
*/
struct ImageItem {
  std::string context;
  std::string image;
  std::string info;
  std::string type;
  std::string load;
  std::string offset;
  std::string pname;
};

/**
 * @brief target set containing
 *        set name (default unnamed),
 *        info string,
 *        target set images,
 *        debugger configuration
*/
struct TargetSetItem {
  std::string set;
  std::string info;
  std::vector<ImageItem> images;
  DebuggerItem debugger;
};

/**
 * @brief build types containing
 *        toolchain,
 *        optimization level,
 *        debug information,
 *        warnings,
 *        language standard C,
 *        language standard C++,
 *        preprocessor defines,
 *        preprocessor defines for assembly files,
 *        preprocessor undefines,
 *        include paths,
 *        include paths for assembly files,
 *        exclude paths,
 *        misc compiler controls,
 *        platform processor
 *        context map
 *        west defines
*/
struct BuildType {
  std::string compiler;
  std::string optimize;
  std::string debug;
  std::string warnings;
  std::string languageC;
  std::string languageCpp;
  bool lto = false;
  std::vector<std::string> defines;
  std::vector<std::string> definesAsm;
  std::vector<std::string> undefines;
  std::vector<std::string> addpaths;
  std::vector<std::string> addpathsAsm;
  std::vector<std::string> delpaths;
  std::vector<MiscItem> misc;
  ProcessorItem processor;
  std::vector<std::pair<std::string, std::string>> variables;
  std::vector<ContextName> contextMap;
  std::vector<std::string> westDefs;
};

/**
 * @brief target types containing
 *        platform board,
 *        platform device,
 *        additional memory,
 *        target set,
 *        build options
*/
struct TargetType {
  std::string board;
  std::string device;
  std::vector<MemoryItem> memory;
  std::vector<TargetSetItem> targetSet;  
  BuildType build;
};

/**
 * @brief directories item containing
 *        intdir directory,
 *        outdir directory,
 *        tmpdir directory,
 *        cbuild directory,
 *        cprj directory,
 *        rte directory,
 *        output base directory
*/
struct DirectoriesItem {
  std::string intdir;
  std::string outdir;
  std::string tmpdir;
  std::string cbuild;
  std::string cprj;
  std::string rte;
  std::string outBaseDir;
};

/**
 * @brief component item containing
 *        component identifier,
 *        component build settings,
 *        type inclusion
 *        component instances
*/
struct ComponentItem {
  std::string component;
  std::string condition;
  std::string fromPack;
  BuildType build;
  TypeFilter type;
  int instances = 1;
};

/**
 * @brief output item containing
 *        base name,
 *        list of types [elf, hex, bin, lib]
*/
struct OutputItem {
  std::string baseName;
  std::vector<std::string> type;
};

/**
 * @brief generators option item containing
 *        generator id,
 *        path to generated files,
 *        name of generator import file,
 *        map to run-time context
*/
struct GeneratorOptionsItem {
  std::string id;
  std::string path;
  std::string name;
  std::string map;
  bool operator<(const GeneratorOptionsItem& item) const {
    return (this->id < item.id) || (this->path < item.path) || (this->name < item.name);
  }
};

/**
 * @brief generators item containing
 *        base directory,
 *        options map
*/
struct GeneratorsItem {
  std::string baseDir;
  std::map<std::string, GeneratorOptionsItem> options;
};

/**
 * @brief executes item containing
 *        execute description,
 *        command string,
 *        boolean run always,
 *        list of input files,
 *        list of output files,
 *        type inclusion
*/
struct ExecutesItem {
  std::string execute;
  std::string run;
  bool always;
  std::vector<std::string> input;
  std::vector<std::string> output;
  std::vector<std::string> dependsOn;
  TypeFilter typeFilter;
};

/**
 * @brief layer item containing
 *        layer name,
 *        layer type,
 *        optional flag,
 *        type inclusion
*/
struct LayerItem {
  std::string layer;
  std::string type;
  bool optional;
  TypeFilter typeFilter;
};

/**
 * @brief connect item containing
 *        connect functionality description
 *        set <config-id>.<select> value for config option identification
 *        info display description
 *        vector of provided connections pairs,
 *        vector of consumed connections pairs
*/
struct ConnectItem {
  std::string connect;
  std::string set;
  std::string info;
  std::vector<std::pair<std::string, std::string>> provides;
  std::vector<std::pair<std::string, std::string>> consumes;
};

/**
 * @brief linker item containing
 *        auto linker script generation,
 *        regions file,
 *        script file,
 *        preprocessor defines,
 *        for compiler control,
 *        type inclusion
*/
struct LinkerItem {
  bool autoGen;
  std::string regions;
  std::string script;
  std::vector<std::string> defines;
  std::vector<std::string> forCompiler;
  TypeFilter typeFilter;
};

/**
 * @brief setup item containing
 *        setup description name,
 *        for compiler control,
 *        setup build settings,
 *        type inclusion,
 *        output name and type,
 *        list of linker entries
*/
struct SetupItem {
  std::string description;
  std::vector<std::string> forCompiler;
  BuildType build;
  TypeFilter type;
  OutputItem output;
  std::vector<LinkerItem> linker;
};

/**
 * @brief yaml mark
 *        parent file
 *        line
 *        column
*/
struct YamlMark {
  std::string parent;
  int line;
  int column;
};

/**
 * @brief file node containing
 *        file path,
 *        for compiler control,
 *        file category,
 *        link directives,
 *        file build settings,
 *        type filter,
 *        yaml mark
*/
struct FileNode {
  std::string file;
  std::vector<std::string> forCompiler;
  std::string category;
  std::string link;
  BuildType build;
  TypeFilter type;
  YamlMark mark;;
};

/**
 * @brief group node containing
 *        group name,
 *        for compiler control,
 *        children files,
 *        children groups,
 *        group build settings,
 *        type filter
*/
struct GroupNode {
  std::string group;
  std::vector<std::string> forCompiler;
  std::vector<FileNode> files;
  std::vector<GroupNode> groups;
  BuildType build;
  TypeFilter type;
};

/**
 * @brief west descriptor
*/
struct WestDesc {
  std::string projectId;
  std::string app;
  std::string board;
  std::string device;
  std::vector<std::string> westDefs;
  std::vector<std::string> westOpt;
};

/**
 * @brief context descriptor containing
 *        cproject filename,
 *        type filter
*/
struct ContextDesc {
  std::string cproject;
  TypeFilter type;
  WestDesc west;
};

/**
 * @brief cbuild pack descriptor containing
 *        filename,
 *        full path to cbuild pack file,
 *        containing directory,
 *        list of resolved packs
*/
struct CbuildPackItem {
  std::string name;
  std::string path;
  std::string directory;

  std::vector<ResolvedPackItem> packs;
};

/**
 * @brief default item containing
 *        cdefault path,
 *        compiler,
 *        list of selectable compilers,
 *        misc
*/
struct CdefaultItem {
  std::string path;
  std::string compiler;
  std::vector<std::string> selectableCompilers;
  std::vector<MiscItem> misc;
};

typedef std::vector<std::pair<std::string, BuildType>> BuildTypes;
typedef std::vector<std::pair<std::string, TargetType>> TargetTypes;
/**
 * @brief solution item containing
 *        csolution name,
 *        csolution path,
 *        csolution description,
 *        csolution directory,
 *        created-for string,
 *        output directories,
 *        list of selectable compilers,
 *        build types,
 *        target types,
 *        target properties,
 *        list of cprojects,
 *        list of west apps,
 *        list of contexts descriptors,
 *        list of packs,
 *        cdefault enable switch,
 *        generator options,
 *        list of executes,
*/
struct CsolutionItem {
  std::string name;
  std::string path;
  std::string description;
  std::string directory;
  std::string createdFor;
  DirectoriesItem directories;
  std::vector<std::string> selectableCompilers;
  BuildTypes buildTypes;
  TargetTypes targetTypes;
  TargetType target;
  std::vector<std::string> cprojects;
  std::vector<std::string> westApps;
  std::vector<ContextDesc> contexts;
  std::vector<PackItem> packs;
  bool enableCdefault;
  GeneratorsItem generators;
  CbuildPackItem cbuildPack;
  std::vector<ExecutesItem> executes;
  std::vector<std::string> ymlOrderedBuildTypes;
  std::vector<std::string> ymlOrderedTargetTypes;
};

/**
 * @brief cproject item containing
 *        project name,
 *        project path,
 *        project directory,
 *        rte base directory,
 *        project output name and types,
 *        project target properties,
 *        list of required components,
 *        list of user groups,
 *        list of layers,
 *        list of setups,
 *        list of connections,
 *        list of packs,
 *        list of linker entries,
 *        generator options,
 *        list of executes
*/
struct CprojectItem {
  std::string name;
  std::string path;
  std::string directory;
  std::string rteBaseDir;
  OutputItem output;
  TargetType target;
  std::vector<ComponentItem> components;
  std::vector<GroupNode> groups;
  std::vector<LayerItem> clayers;
  std::vector<SetupItem> setups;
  std::vector<ConnectItem> connections;
  std::vector<PackItem> packs;
  std::vector<LinkerItem> linker;
  GeneratorsItem generators;
  std::vector<ExecutesItem> executes;
};

/**
 * @brief clayer item containing
 *        layer name,
 *        layer description,
 *        layer path,
 *        layer type,
 *        layer directory,
 *        layer output type,
 *        layer target properties,
 *        list of required components,
 *        list of user groups,
 *        list of connections
 *        list of packs,
 *        list of linker entries,
 *        board filter,
 *        device filter,
 *        generator options
*/
struct ClayerItem {
  std::string name;
  std::string description;
  std::string path;
  std::string type;
  std::string directory;
  std::string outputType;
  TargetType target;
  std::vector<ComponentItem> components;
  std::vector<GroupNode> groups;
  std::vector<ConnectItem> connections;
  std::vector<PackItem> packs;
  std::vector<LinkerItem> linker;
  std::string forBoard;
  std::string forDevice;
  GeneratorsItem generators;
};

/**
 * @brief cbuildset item containing
 *        list of contexts,
 *        toochain used,
*/
struct CbuildSetItem {
  std::string generatedBy;
  std::vector<std::string> contexts;
  std::string compiler;
};

/**
 * @brief gdbserver defaults item containing
 *        gdbserver port
 *        active flag
*/
struct GdbServerDefaults {
  std::string port;
  bool active = false;
};

/**
 * @brief telnet defaults item containing
 *        telnet port
 *        telnet mode
 *        active flag
*/
struct TelnetDefaults {
  std::string port;
  std::string mode;
  bool active = false;
};

/**
 * @brief debug-adapter defaults item containing
 *        gdbserver defaults
 *        telnet defaults
 *        debug protocol(jtag or swd)
 *        debug clock speed
*/
struct DebugAdapterDefaultsItem {
  GdbServerDefaults gdbserver;
  TelnetDefaults telnet;
  std::string protocol;
  std::string clock;
  CustomItem custom;
};

/**
 * @brief debug-adapter item containing
 *        name
 *        list of alias
 *        template
 *        gdbserver
 *        defaults
*/
struct DebugAdapterItem {
  std::string name;
  std::vector<std::string> alias;
  std::string templateFile;
  DebugAdapterDefaultsItem defaults;
};

/**
 * @brief debug-adapters item containing
 *        list of debug-adapters
*/
typedef std::vector<DebugAdapterItem> DebugAdaptersItem;

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
   * @brief parse cdefault
   * @param checkSchema false to skip schema validation
   * @param input cdefault.yml file
  */
  bool ParseCdefault(const std::string& input, bool checkSchema);

  /**
   * @brief parse cproject
   * @param input cproject.yml file
   * @param checkSchema false to skip schema validation
   * @param boolean parse single project, default false
  */
  bool ParseCproject(const std::string& input, bool checkSchema, bool single = false);

  /**
   * @brief parse csolution
   * @param checkSchema false to skip schema validation
   * @param input csolution.yml file
   * @param frozenPacks false to allow missing cbuild-packs.yml file
  */
  bool ParseCsolution(const std::string& input, bool checkSchema, bool frozenPacks);

  /**
   * @brief parse clayer
   * @param checkSchema false to skip schema validation
   * @param input clayer.yml file
  */
  bool ParseClayer(const std::string& input, bool checkSchema);

  /**
   * @brief parse generic clayer files
   * @param checkSchema false to skip schema validation
   * @param input clayer.yml file
  */
  bool ParseGenericClayer(const std::string& input, bool checkSchema);

  /**
   * @brief parse cbuild set file
   * @param checkSchema false to skip schema validation
   * @param input path to *.cbuild-set.yml file
  */
  bool ParseCbuildSet(const std::string& input, bool checkSchema);

  /**
   * @brief parse debug-adapters file
   * @param checkSchema false to skip schema validation
   * @param input path to *.debug-adapters.yml file
  */
  bool ParseDebugAdapters(const std::string& input, bool checkSchema);

  /**
   * @brief get cdefault
   * @return cdefault item
  */
  CdefaultItem& GetCdefault(void);

  /**
   * @brief get csolution
   * @return csolution item
  */
  CsolutionItem& GetCsolution(void);

  /**
   * @brief get cprojects
   * @return cprojects map
  */
  std::map<std::string, CprojectItem>& GetCprojects(void);

  /**
   * @brief get clayers
   * @return clayers map
  */
  std::map<std::string, ClayerItem>& GetClayers(void);

  /**
   * @brief get generic clayers
   * @return clayers map
  */
  std::map<std::string, ClayerItem>& GetGenericClayers(void);

  /**
   * @brief get cbuildset
   * @return cbuildset item
  */
  CbuildSetItem& GetCbuildSetItem(void);

  void Clear() {
    m_cdefault = {};
    m_csolution = {};
    m_cbuildSet = {};
    m_cprojects.clear();
    m_clayers.clear();
    m_genericClayers.clear();
  }
  /**
   * @brief get debug adapters
   * @return debug adapters list
  */
  DebugAdaptersItem& GetDebugAdaptersItem(void);

protected:
  CdefaultItem m_cdefault;
  CsolutionItem m_csolution;
  CbuildSetItem m_cbuildSet;
  DebugAdaptersItem m_debugAdapters;
  std::map<std::string, CprojectItem> m_cprojects;
  std::map<std::string, ClayerItem> m_clayers;
  std::map<std::string, ClayerItem> m_genericClayers;
};

#endif  // PROJMGRPARSER_H
