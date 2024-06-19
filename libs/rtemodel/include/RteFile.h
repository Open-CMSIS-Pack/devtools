#ifndef RteFile_H
#define RteFile_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteFile.h
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteItem.h"

class RteComponent;
class RteTarget;

/**
 * @brief RTE data model class to represent a file (source, header, library, etc.) elements
*/
class RteFile : public RteItem
{

public:

  /**
   * @brief file category, corresponds to "category" attribute
  */
  enum class Category {
    DOC,                // document file or URL
    HEADER,             // C/C++ header file
    INCLUDE,            // include path
    LIBRARY,            // library file
    OBJECT,             // object file
    SOURCE,             // source file without concrete language type
    SOURCE_ASM,         // assembler source file
    SOURCE_C,           // C source file
    SOURCE_CPP,         // C++ source file
    LINKER_SCRIPT,      // linker script or scatter file
    UTILITY,            // utility file or executable
    SVD,                // SVD file : deprecated
    IMAGE,              // image file
    PRE_INCLUDE_GLOBAL, // global C/C++ pre-include file (for entire project)
    PRE_INCLUDE_LOCAL,  // local C/C++ pre-include file (for files of contributing component)
    GEN_SOURCE,         // source file which is exclusively used by the generator
    GEN_HEADER,         // header file which is exclusively used by the generator
    GEN_PARAMS,         // parameter file which is exclusively used by the generator
    GEN_ASSET,          // asset file which is exclusively used by the generator
    OTHER               // uncategorized file or path
  };

 /**
   * @brief file role, corresponds to "attr" attribute
  */
  enum class Role {
    ROLE_NONE,      // file has no specific role
    ROLE_COPY,      // file must be copied to the project
    ROLE_CONFIG,    // file is a "config" file, must be copied to the project and edited by the user
    ROLE_TEMPLATE,  // file represents a template or a part of it, can be copied to project by user request
    ROLE_INTERFACE  // file is an interface
  };

  /**
   * @brief file scope, corresponds to "scope" attribute
  */
  enum class Scope {
    SCOPE_NONE,     // scope is not specified
    SCOPE_PUBLIC,   // include path is added to the command line for building any modules of the specified language
                    // header suggested for inclusion by other modules and is considered the contract of the component
    SCOPE_PRIVATE   // include path is added to the command line for building any module of the component for the specified language
                    // header is an internal header file which must not be explicitly included by modules outside of the scope of the component
  };

  /**
   * @brief file language, corresponds to "language" attribute
  */
  enum class Language {
    LANGUAGE_NONE,  // not explicitly specified language
    LANGUAGE_ASM,   // The file information is passed to an assembler
    LANGUAGE_C,     // The file information is passed to a C compiler
    LANGUAGE_CPP,   // The file information is passed to a C++ compiler
    LANGUAGE_C_CPP, // The file information is passed to both C - as well as C++ compiler
    LANGUAGE_LINK   // The file information is passed to a linker
  };

  /**
  * @brief constructor
  * @param parent pointer to RteItem parent
 */
  RteFile(RteItem* parent);

  /**
   * @brief validate item after construction
   * @return true if valid
  */
   bool Validate() override;

  /**
   * @brief check if file has copy role
   * @return true if GetRole() == RteFile::COPY
  */
  bool IsForcedCopy() const;
  /**
   * @brief check if file has config role
   * @return true if GetRole() == RteFile::CONFIG
  */
  bool IsConfig() const;
  /**
   * @brief check if file is a template
   * @return true if GetRole() == RteFile::TEMPLATE
  */
  bool IsTemplate() const;
  /**
   * @brief check if file must be added to project
   * @return true if to add to project
  */
  bool IsAddToProject() const;

  /**
   * @brief get file category
   * @return RteFile::Category
  */
  Category GetCategory() const;

  /**
   * @brief get file category string
   * @return file category as string
  */
  const std::string& GetCategoryString() const;

  /**
   * @brief get file role
   * @return RteFile::Role
  */
  Role GetRole() const;

  /**
   * @brief get file scope
   * @return RteFile::Scope
  */
  Scope GetScope() const;

  /**
   * @brief get file language
   * @return RteFile::Language
  */
  Language GetLanguage() const;

  /**
   * @brief construct file ID out of "name" attribute and version
   * @return string ID
  */
   std::string ConstructID() override;

  /**
   * @brief get file name
   * @return file name
   */
  const std::string& GetName() const override;


  /**
   * @brief get file version string
   * @return "version" attribute value if set, otherwise parent component version
  */
   const std::string& GetVersionString() const override;
  /**
   * @brief construct a comment string that can be shown in a window or view displaying project structure next to filename
   * @return constructed file comment string if the file belongs to a component, empty string otherwise
  */
  virtual std::string GetFileComment() const;
  /**
   * @brief construct a comment that can be displayed in an editor window's context menu next to corresponding header file name
   * @return constructed a header file comment string if the file belongs to a component, empty string otherwise
  */
  virtual std::string GetHeaderComment() const;
  /**
   * @brief collect collection of paths to library source code files
   * @param paths set of path strings
  */
  void GetAbsoluteSourcePaths(std::set<std::string>& paths) const;
  /**
   * @brief construct full path including filename for a file that should be used when adding file to a project
   * @param deviceName name of device used in the project
   * @param instanceIndex file index for components with multiple instantiation.
   * Gets appended as a string to the base filename before extension it its value >=0.
   * Value of < 0 indicates that parent component can have only one instance
   * @param rteFolder the "RTE" folder path used for placing files
   * @return full project-relative path including filename and extension
  */
  std::string GetInstancePathName(const std::string& deviceName, int instanceIndex, const std::string& rteFolder) const;

  /**
   * @brief construct absolute include path string that can be used in'-I' compiler option.
   * @return string constructed from "path" attribute if set, otherwise from "name" attribute
  */
  std::string GetIncludePath() const;

  /**
   * @brief constructs relative header file pathname
   * @return value of "name" attribute combined with "path" attribute (if set)
  */
  std::string GetIncludeFileName() const;

  /**
   * @brief helper static method to convert string to RteFile::Category value
   * @param category string with category
   * @return RteFile::Category value
  */
  static Category CategoryFromString(const std::string& category);
  /**
   * @brief helper static method to convert string to RteFile::Role value
   * @param role string with role
   * @return RteFile::Role value
  */
  static Role     RoleFromString(const std::string& role);

  /**
   * @brief helper static method to convert string to RteFile::Scope value
   * @param scope string with scope
   * @return RteFile::Scope value
  */
  static Scope     ScopeFromString(const std::string& scope);

  /**
   * @brief helper static method to convert string to RteFile::Language value
   * @param language string with language
   * @return RteFile::Language value
  */
  static Language  LanguageFromString(const std::string& language);

};


/**
 * @brief class to support <files> and <group> elements
*/
class RteFileContainer : public RteItem
{
public:
  /**
  * @brief constructor
  * @param parent pointer to RteItem parent
 */
  RteFileContainer(RteItem* parent);

  /**
   * @brief get file with given filename
   * @param name relative filename
   * @return pointer to RteFile if found, nullptr otherwise
  */
  RteFile* GetFile(const std::string& name) const;

  /**
   * @brief find file by its original absolute pathname
   * @param absPathName absolute pathname
   * @return pointer to RteFile if found, nullptr otherwise
  */
  RteFile* GetFileByOriginalAbsolutePath(const std::string& absPathName) const;

  /**
   * @brief get parent file container or group
   * @return pointer to RteFileContainer if has parent container or nullptr otherwise
  */
  RteFileContainer* GetParentContainer() const;

  /**
   * @brief get group name
   * @return group name
  */
  const std::string& GetName() const override;

  /**
   * @brief construct hierarchical group name in the format TopGroup:SubGroup:SubSubGroup
   * @return combined group hierarchical group name with ':' as delimiter
  */
  std::string GetHierarchicalGroupName() const;

  /**
   * @brief collect recursively include path strings extracted from all INCLUDE and HEADER files in the container
   * @param incPaths set to fill with include path strings
  */
  void GetIncludePaths(std::set<std::string>& incPaths) const;

  /**
   * @brief collect recursively all files in the container with LINKER_SCRIPT category
   * @param linkerScripts set to fill with pointers to include RteFile items
  */
  void GetLinkerScripts(std::set<RteFile*>& linkerScripts) const;

  /**
   * @brief create a new instance of type RteItem
   * @param tag name of tag
   * @return pointer to instance of type RteItem
  */
  RteItem* CreateItem(const std::string& tag) override;
};

/**
 * @brief class to support source template instantiation in the project
*/
class RteFileTemplate
{
public:
  /**
   * @brief constructor
   * @param select selected template name corresponding "select" attribute of an RteFile with TEMPLATE category
  */
  RteFileTemplate(const std::string& select);

  /**
   * @brief get selected template name
   * @return selected template name
  */
  const std::string& GetSelectString() const { return m_select; }

  /**
   * @brief get template files
   * @return set of RteFile pointers
  */
  const std::set<RteFile*>& GetFiles() const { return m_files; }

  /**
   * @brief add file to template
   * @param f pointer to RteFile to add to template
  */
  void AddFile(RteFile* f);

  /**
   * @brief get number of template copies to instantiate in project
   * @return number of template copies to instantiate
  */
  int GetInstanceCount() const { return m_instanceCount; }
  /**
   * @brief set number of template copies to instantiate in project
   * @param instanceCount of template copies to instantiate
  */
  void SetInstanceCount(int instanceCount) { m_instanceCount = instanceCount; }

protected:
  std::string m_select; // select attribute used as name
  std::set<RteFile*> m_files; // files in template (all must have the same "select" attribute)
  int m_instanceCount; // number of copies to instantiate in project
};

/**
 * @brief class to represent collection of source code templates provided by a component
*/
class RteFileTemplateCollection
{
public:
  /**
   * @brief constructor
   * @param c pointer to RteComponent to collect templates from
  */
  RteFileTemplateCollection(RteComponent* c);
  /**
   * @brief virtual destructor
  */
  virtual ~RteFileTemplateCollection();

  /**
   * @brief get component providing templates
   * @return pointer to RteComponent item
  */
  RteComponent* GetComponent() const { return m_component; }

  /**
   * @brief get collection of available templates
   * @return map of template name to RteFileTemplate pointer pairs
  */
  const std::map<std::string, RteFileTemplate*>& GetTemplates() const { return m_templates; }

  /**
   * @brief get template with given name
   * @param select template name
   * @return pointer to RteFileTemplate if found, nullptr otherwise
  */
  RteFileTemplate* GetTemplate(const std::string& select) const;

  /**
   * @brief add file to a template corresponding file's "select" attribute
   * @param f pointer to RteFile to add
   * @param instanceCount number of copies to instantiate in project, corresponds to number of component instances
  */
  void AddFile(RteFile* f, int instanceCount);

protected:
  RteComponent* m_component; // component providing templates
  std::map<std::string, RteFileTemplate*> m_templates; // template collection
};

#endif // RteFile_H
