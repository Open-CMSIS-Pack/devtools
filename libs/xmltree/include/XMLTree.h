#ifndef XMLTree_H
#define XMLTree_H
/******************************************************************************/
/** @file  XmlTree.h
  * @brief A simple XML interfacer that reads data into a tree structure
*/
/******************************************************************************/
/*
  * The reader should be kept semantics-free:
  * no special processing based on tag, attribute or value std::string
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "XmlTreeItem.h"
#include "IXmlItemBuilder.h"
#include "ISchemaChecker.h"

#include <string>
#include <list>
#include <map>
#include <set>

/**
 * @brief class used control parsing process
*/
class XMLTreeCallback
{
public:
  virtual ~XMLTreeCallback() {};
  // the function is called after each file is parsed, return 0 to stop further processing

  /**
   * @brief called after each file is parsed, return 0 to stop further processing
   * @param fileName XML file name
   * @param success status
   * @return default returns true to continue parsing
  */
  virtual bool FileParsed(const std::string& fileName, bool success) { return true; }
};

/**
 * @brief class responsible for adjusting attribute and text of XML items
*/
class XmlValueAdjuster
{
public:
  virtual ~XmlValueAdjuster() {};

  /**
   * @brief convert slashes string to operating system specific ones
   * @param s string containing slashes
   * @return converted string
  */
  static std::string SlashesToOsSlashes(const std::string& s);

  /**
   * @brief convert slashes in string to backslashes
   * @param fileName string containing slashes
   * @return converted string
  */
  static std::string SlashesToBackSlashes(const std::string& fileName);

  /**
   * @brief convert backslashes in string to slashes
   * @param fileName string containing backslashes
   * @return converted string
  */
  static std::string BackSlashesToSlashes(const std::string& fileName);

  /**
   * @brief check if path needs to be converted regarding slash or backslash
   * @param fileName string representing path
   * @return true if conversion is needed, otherwise false
  */
  static bool IsPathNeedConversion(const std::string& fileName);

  /**
   * @brief check if path is absolute
   * @param fileName path
   * @return true if path is absolute
  */
  static bool IsAbsolute(const std::string& fileName);

  /**
   * @brief check if string has URL syntax
   * @param fileName URL string
   * @return true if string has URL syntax
  */
  static bool IsURL(const std::string& fileName);

  /**
   * @brief check if tag is a file path
   * @param tag name of tag
   * @param name attribute name
   * @return the default implementation returns false
  */
  virtual bool IsPath(const std::string& tag, const std::string& name) const;

  /**
   * @brief adjust attribute value
   * @param tag name of tag
   * @param name attribute name
   * @param value attribute value
   * @param lineNumber line number in the XML file
   * @return adjusted attribute value. The default implementation just returns the given attribute value
  */
  virtual std::string AdjustAttributeValue(const std::string& tag, const std::string& name, const std::string& value, int lineNumber);

  /**
   * @brief adjust path
   * @param fileName file name with path
   * @param lineNumber line number
   * @return adjusted path. The default implementation converted slashes depending on operating system
  */
  virtual std::string AdjustPath(const std::string& fileName, int lineNumber);
};


/**
 * @brief class that encapsulates an XML element, a default implementation of XmlTreeItem<> class
*/
class XMLTreeElement : public XmlTreeItem<XMLTreeElement>
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent element
  */
  XMLTreeElement(XMLTreeElement* parent = NULL);

  /**
   * @brief constructor
   * @param parent pointer to parent element
   * @param tag name of tag
  */
  XMLTreeElement(XMLTreeElement* parent, const std::string& tag);
  ~XMLTreeElement() override;

  /**
   * @brief getter for this instance
   * @return pointer to the instance of type XMLTreeElement
  */
  XMLTreeElement* GetThis() const override { return const_cast<XMLTreeElement*>(this); }

  /**
   * @brief create a new instance of type XMLTreeElement
   * @param tag name of tag
   * @return pointer to instance of type XMLTreeElement
  */
  XMLTreeElement* CreateItem(const std::string& tag) override { return new XMLTreeElement(GetThis(), tag); }
};

/**
 * @brief class representing a single XML file
*/
class XMLTreeDoc : public  XMLTreeElement
{
public:
  /**
   * @brief constructor
   * @param parent pointer to XML root element
   * @param xmlFile XML file name
  */
  XMLTreeDoc(XMLTreeElement* parent, const std::string& xmlFile) :
    XMLTreeElement(parent), m_xmlFile(xmlFile), m_bValid(false) {};

public:
  /**
   * @brief getter for XML file name stored in the instance
   * @return string containing XML file name
  */
  const std::string& GetRootFileName() const  override { return m_xmlFile; }

  /**
   * @brief check if instance is valid
   * @return true if instance is valid, otherwise false
  */
  bool IsValid() const  override { return m_bValid; }

  /**
   * @brief setter for instance validity
   * @param valid instance validity
  */
  void SetValid(bool valid)  override { m_bValid = valid; }

private:
  std::string m_xmlFile; // XML file with path
  bool m_bValid;
};

/**
 * @brief class used to create a XML tree
*/
class XMLTreeParserInterface;
class XMLTree : public  XMLTreeElement
{
protected:
  XMLTree(IXmlItemBuilder* itemBuilder); // protected constructor preventing instantiating the base class

public:
  ~XMLTree() override;

  void Clear() override; // does not destroy XML parser!
  virtual bool Init() ;

  /**
   * @brief return item builder factory
   * @return pointer to IXmlItemBuilder
  */
  IXmlItemBuilder* GetXmlItemBuilder() const { return m_XmlItemBuilder; }

 /**
  * @brief set item builder factory
  * @param pointer to IXmlItemBuilder
  * @param takeOnerschip flag to make XMLTree to delete the builder
 */
  void SetXmlItemBuilder(IXmlItemBuilder* itemBuilder, bool takeOnerschip = false);

  /**
   * @brief return XmlValueAdjuster used by the class
   * @return pointer to XmlValueAdjuster object
  */
  XmlValueAdjuster* GetXmlValueAdjuster() const { return m_XmlValueAdjuster; }

  /**
   * @brief set XmlValueAdjuster instance to be used by the class
   * @param adjuster pointer to XmlValueAdjuster instance
  */
  void SetXmlValueAdjuster(XmlValueAdjuster* adjuster) { m_XmlValueAdjuster = adjuster; }

  /**
   * @brief getter for associated schema file name
   * @return associated schema file name
  */
  const std::string& GetSchemaFileName() const { return m_schemaFile; }

  /**
   * @brief sets schema checker to validate input files
   * @param schemaChecker schema checker implementing ISchemaChecker interface
  */
  void SetSchemaChecker (ISchemaChecker* schemaChecker) {
    m_schemaChecker = schemaChecker;
  }

  /**
   * @brief getter for schema checker set by SetSchemaChecker method
   * @return schema checker implementing ISchemaChecker interface or nullptr
  */
  ISchemaChecker* GetSchemaChecker() const { return m_schemaChecker;}

  /**
   * @brief getter for associated XML file name
   * @return associated XML file name
  */
  const std::string& GetCurrentFile() const;

  /**
   * @brief setter for associated XML schema file name
   * @param xsdFile associated XML schema file name
  */
  void SetSchemaFileName(const std::string& xsdFile);

  /**
   * @brief setter for list of tags to be ignored
   * @param ignoreTags list of tags
  */
  void SetIgnoreTags(const std::set<std::string>& ignoreTags);

  /**
   * @brief setter for member of type XMLTreeCallback
   * @param callback pointer to XMLTreeCallback instance to set
  */
  void SetCallback(XMLTreeCallback* callback) { m_callback = callback; }

  /**
   * @brief add a XML file name to the list of files in the instance and parse it
   * @param fileName XML file name to be added
   * @param bParse true to parse the XML file
   * @return true if file isn't parsed or parsing XML file is successful
  */
  bool AddFileName(const std::string& fileName, bool bParse = false);

  /**
   * @brief setter for list of XML files to be parsed
   * @param fileNames list of XML files
   * @param bParse true to parse files
   * @return true if parsing all files is successful, otherwise false
  */
  bool SetFileNames(const std::list<std::string>& fileNames, bool bParse = false);

  /**
   * @brief getter for list of XML files in the instance
   * @return list of XML files in the instance
  */
  const std::list<std::string>& GetFileNames() const { return m_fileNames; }

  /**
   * @brief getter for list of error strings
   * @return list of error strings
  */
  const std::list<std::string>& GetErrorStrings() const { return m_errorStrings; }

  /**
   * @brief determine if error occurred
   * @return true if error occurred
  */
  bool HasErrors() const { return m_nErrors > 0; }

  /**
   * @brief determine if there was warning
   * @return true if there was warning
  */
  bool HasWarnings() const { return m_nWarnings > 0; }

  /**
   * @brief getter for number of errors occurred
   * @return number of errors occurred
  */
  int  GetErrors() const { return m_nErrors; }

  /**
   * @brief getter for number of warnings occurred
   * @return number of warnings occurred
  */
  int  GetWarnings() const { return m_nWarnings; }

  /**
   * @brief parse all files
   * @return true if all files are successfully parsed
  */
  bool ParseAll();

  /**
   * @brief parse a XML file
   * @param fileName name of XML file
   * @return true if file is successfully parsed
  */
  bool ParseFile(const std::string& fileName);

  /**
   * @brief parse a buffer containing XML formatted text
   * @param xmlString buffer containing XML formatted text
   * @return true if buffer is successfully parsed
  */
  bool ParseString(const std::string& xmlString);

  /**
   * @brief parse either file or buffer containing XML formatted text
   * @param fileName file name
   * @param xmlString buffer containing XML formatted text
   * @return true if parsing is successfully
  */
  bool Parse(const std::string& fileName, const std::string& xmlString);

  /**
   * @brief create tree from DOM elements under given parent (void* is used to avoid inclusion of Xerces headers)
   * @param pDOMnode pointer to DOM element
   * @return true if parsing is successfully.
  */
  bool ParseFromDOMNode(void* pDOMnode);

  /**
   * @brief create a DOM. The default implementation does nothing.
   * @param pDOMDocument DOM document
   * @param pDOMparentElement DOM parent element
  */
  void CreateDOM(void* pDOMDocument, void* pDOMparentElement);

  /**
   * @brief output DOM into a file
   * @param fileName name of file
   * @return true if successful
  */
  bool PrintDOM(const std::string& fileName);

public:
  /**
   * @brief adjust attribute value
   * @param tag name of tag
   * @param name attribute name
   * @param value attribute value
   * @param lineNumber line number in the XML file
   * @return adjusted attribute value
  */
  virtual std::string AdjustAttributeValue(const std::string& tag, const std::string& name, const std::string& value, int lineNumber) const;


  /**
     * @brief check if tag is a file path
     * @param tag name of tag
     * @param name name of attribute
     * @return default implementation returns false
    */
  virtual bool IsPath(const std::string& tag, const std::string& name) const;

protected:
  // parses file or XML sting (fileName can be used to associate parsed document with the file)
  bool DoParse(const std::string& fileName, const std::string& xmlString);
  /**
   * @brief creates underling parser interface
   * @return XMLTreeParserInterface;
  */
  virtual XMLTreeParserInterface* CreateParserInterface() = 0;

protected:
  std::string m_schemaFile; // schema file with path

  std::list<std::string> m_fileNames; // XML files to parse
  // errors for all docs
  std::list<std::string> m_errorStrings;
  int m_nErrors;
  int m_nWarnings;

  XMLTreeCallback* m_callback;
  IXmlItemBuilder* m_XmlItemBuilder;    // the builders is managed externally unless m_bInternal builder is specified
  bool m_bInternalBuilder; // flag is set if default builder is allocated internally or the builder is set with takeOnership flag
  XmlValueAdjuster* m_XmlValueAdjuster; // the adjuster is managed externally
  ISchemaChecker* m_schemaChecker; // schema checker is managed externally
  XMLTreeParserInterface* m_p; // internal implementation to hide XML parser like xerces from client
};

/**
 * @brief class to encapsulate calls to concrete XML parser implementation
*/
class XMLTreeParserInterface
{
public:
  /**
   * @brief constructor
   * @param tree pointer to an instance of type XMLTree
  */
  XMLTreeParserInterface(XMLTree* tree);

  /**
   * @brief destructor
  */
  virtual ~XMLTreeParserInterface();

  /**
   * @brief initialize the instance. The default implementation does nothing
   * @return boolean value.
  */
  virtual bool Init() = 0;

  /**
   * @brief clear the instance
  */
  virtual void Clear();

  /**
   * @brief getter for list of error strings
   * @return list of error strings
  */
  const std::list<std::string>& GetErrorStrings() const { return m_errorStrings; }

  /**
   * @brief getter for XML file name
   * @return string containing XML file name
  */
  const std::string& GetCurrentFile() const { return m_xmlFile; }

  /**
   * @brief check if errors occurred
   * @return true if errors occurred
  */
  bool HasErrors() const { return m_nErrors > 0; }

  /**
   * @brief check if warnings are saved
   * @return true if there is any warning
  */
  bool HasWarnings() const { return m_nWarnings > 0; }

  /**
   * @brief getter for number of errors
   * @return error number
  */
  int  GetErrors() const { return m_nErrors; }

  /**
   * @brief getter for warning number
   * @return warning number
  */
  int  GetWarnings() const { return m_nWarnings; }

  /**
   * @brief parse a XML file or a buffer containing XML formatted text
   * @param fileName name of file
   * @param xmlString buffer containing XML formatted text
   * @return true if successfully parsed. The default implementation does nothing and returns false
  */
  virtual bool Parse(const std::string& fileName, const std::string& xmlString);

  /**
   * @brief parse XML formatted text from a DOM node
   * @param node DOM node
   * @return true if successfully parsed. The default implementation does nothing and returns false
  */
  virtual bool ParseFromDOMNode(void* node);

  /**
   * @brief create a DOM node. The default implementation does nothing
   * @param pDoc pointer to an instance of document
   * @param parent pointer to parent element
  */
  virtual void CreateDOM(void *pDoc, void *parent);

  /**
   * @brief output the DOM
   * @param fileName name of output file
   * @return true if successfully output. The default implementation does nothing and returns false
  */
  virtual bool PrintDOM(const std::string& fileName);

  /**
   * @brief setter for list of tags to be ignored
   * @param ignoreTags list of tags to be ignored
  */
  void SetIgnoreTags(const std::set<std::string>& ignoreTags) { m_IgnoreTags = ignoreTags; }

  /**
   * @brief check if tag is to be ignored
   * @param tag name of tag to be checked
   * @return true if tag is to be ignored
  */
  bool IsTagIgnored(const std::string& tag) const;

  /**
   * @brief set error or warning
   * @param msg message string for error or warning to set
   * @param warning true if message is a warning, otherwise false
  */
  void Error(const std::string& msg, bool warning);

  /**
   * @brief adjust attribute value
   * @param tag name of tag
   * @param name attribute name
   * @param value attribute value
   * @param lineNumber line number in the XML file
   * @return adjusted attribute value.
  */
  virtual std::string AdjustAttributeValue(const std::string& tag, const std::string& name, const std::string& value, int lineNumber) const;

  /**
   * @brief check if tag is a file path
   * @param tag name of tag
   * @param name name of attribute
   * @return true if tag is a file path
   */
  virtual bool IsPath(const std::string& tag, const std::string& name) const;

protected:
  XMLTree* m_tree;

  std::string m_xmlFile; // current XML file;
  // errors for current doc
  std::list<std::string> m_errorStrings;
  int m_nErrors;
  int m_nWarnings;

  std::set<std::string> m_IgnoreTags;
};

class XMLTreeVisitor : public XmlItemVisitor<XMLTreeElement>
{
};

#endif // XMLTree_H
