#ifndef XmlItem_H
#define XmlItem_H
/******************************************************************************/
/*
  * The classes should be kept semantics-free:
  * no includes of uVision-specific header files
  * no special processing based on tag, attribute or value
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include <string>
#include <map>

/**
 * @brief class that represents XML element with tag, text and attributes
*/
class XmlItem
{
public:
  /**
   * @brief default constructor
  */
  XmlItem() noexcept :m_lineNumber(0) {};

  /**
   * @brief copy constructor, uses default implementation
  */
  XmlItem(const XmlItem&) = default;

  /**
   * @brief assignment operator, uses default implementation
  */
  XmlItem& operator=(const XmlItem&) = default;

  /**
   * @brief move assignment operator, uses default implementation
  */
  XmlItem& operator=(XmlItem&&) noexcept = default;

  /**
   * @brief parametrized constructor
   * @param tag XML tag
  */
  XmlItem(const std::string& tag) : m_tag(tag), m_lineNumber(0) {};

  /**
   * @brief parametrized constructor to instantiate with given attributes
   * @param attributes collection as key to value pairs
  */
  XmlItem(const std::map<std::string, std::string>& attributes) : m_attributes(attributes), m_lineNumber(0) {};

  /**
   * @brief virtual destructor
  */
  virtual ~XmlItem() {};

  /**
   * @brief clears the item, default removes all attributes of the instance
  */
  virtual void Clear();

  /**
   * @brief clear all attributes of the instance
  */
  virtual void ClearAttributes();

  /**
   * @brief called to construct the item with attributes and child elements. Default does nothing.
  */
  virtual void Construct() {/* default does nothing */};

  /**
   * @brief check if XML element is empty, i.e. if text and attributes list are empty
   * @return true if empty
  */
  virtual bool IsEmpty() const { return m_text.empty() && m_attributes.empty(); }

  /**
   * @brief getter for tag name
   * @return string containing tag name
  */
  const std::string& GetTag() const { return m_tag; }

  /**
   * @brief setter for tag name
   * @param tag
  */
  void SetTag(const std::string& tag) { m_tag = tag; }

  /**
  * @brief return item's name
  * @return item name, default returns "name" attribute if exists, otherwise item's tag
 */
  virtual const std::string& GetName() const;


  /**
   * @brief getter for tag text
   * @return string containing tag text
  */
  const std::string& GetText() const { return m_text; }

  /**
   * @brief setter for tag text
   * @param text string to set as item's text
  */
  void SetText(const std::string& text) { m_text = text; }

  /**
   * @brief return collection of attributes as a key-value pairs
   * @return map of name to value pairs
  */
  const std::map<std::string, std::string>& GetAttributes() const { return m_attributes; }

  /**
  * @brief add missing attributes, optionally replace existing
  * @param attributes map of name to value pairs to add
  * @param replaceExisting true to replace existing attributes
  * @return true if any attribute is set or changed
 */
  bool AddAttributes(const std::map<std::string, std::string>& attributes, bool replaceExisting);

  /**
   * @brief add a single attribute to the item
   * @param name attribute name
   * @param value attribute value
   * @param insertEmpty true to insert, false to remove if attribute exists but is empty
   * @return true if attribute is inserted/removed/changed
  */
  bool AddAttribute(const std::string& name, const std::string& value, bool insertEmpty = true);

  /**
   * @brief add new attribute
   * @param name attribute name
   * @param value attribute value
   * @return true if attribute is added
  */
  bool SetAttribute(const char* name, const char* value);
  /**
   * @brief add new attribute
   * @param name attribute name
   * @param value attribute value
   * @param radix transformation radix
   * @return true if attribute is added
  */
  bool SetAttribute(const char* name, long value, int radix = 10);

  /**
   * @brief set all attributes replacing existing ones
   * @param attributes collection as key to value pairs
   * @return true if any attribute collection has changed
  */
  bool SetAttributes(const std::map<std::string, std::string>& attributes);

  /**
   * @brief replace instance attributes with the given ones
   * @param attributes given instance of XmlItem
   * @return true if attributes are set
  */
  bool SetAttributes(const XmlItem &attributes);

  /**
   * @brief remove attribute
   * @param name attribute name
   * @return true if attribute is removed
  */
  bool RemoveAttribute(const char* name);

  /**
   * @brief remove the given attribute from the instance
   * @param name attribute name
   * @return true if attribute is removed, false if it does not exist
  */
  bool RemoveAttribute(const std::string& name);

  /**
   * @brief removes several attributes matching given pattern
   * @param pattern attribute pattern to remove
   * @return true if at least one attribute is removed, false otherwise
  */
  bool EraseAttributes(const std::string& pattern);


  /**
   * @brief getter for attribute
   * @param name pointer to attribute name
   * @return string containing attribute value or empty string if attribute does not exist
  */
  const std::string& GetAttribute(const char* name) const;

  /**
   * @brief getter for attribute
   * @param name attribute name
   * @return string containing attribute value or empty string if attribute does not exist
  */
  const std::string& GetAttribute(const std::string& name) const;

  /**
   * @brief static getter for attribute value
   * @param item reference to XmlItem to get attribute
   * @param name attribute name
   * @return string containing attribute value or empty string if attribute does not exist
  */
  static const std::string& GetAttribute(const XmlItem& item, const std::string& name) {
    return item.GetAttribute(name);
  }

  /**
   * @brief check if the given attribute exists
   * @param name pointer to attribute name
   * @return true if attribute exists
  */
  bool HasAttribute(const char* name) const;

  /**
   * @brief check if the given attribute exists
   * @param name attribute name
   * @return true if attribute exists
  */
  bool HasAttribute(const std::string& name) const;

  /**
 * @brief check if attribute has a certain value
 * @param pattern value to be checked, can contain wild cards
 * @return true if value is found
*/
  virtual bool HasValue(const std::string& pattern) const;

  /**
   * @brief getter for attribute value as boolean
   * @param name pointer to attribute name
   * @param defaultValue value to be returned if attribute does not exist
   * @return true if attribute value is equal to "1" or "true", otherwise false
  */
  bool GetAttributeAsBool(const char* name, bool defaultValue = false) const;

  /**
   * @brief getter for attribute value as integer
   * @param name pointer to attribute name
   * @param defaultValue returned value in case attribute does not exist or its value is empty
   * @return attribute value as integer
  */
  int  GetAttributeAsInt(const char* name, int defaultValue = -1) const;

  /**
   * @brief getter for attribute value as unsigned long
   * @param name pointer to attribute name
   * @param defaultValue value to be returned if attribute does not exist or its value is empty
   * @return attribute value as unsigned long
  */
  unsigned GetAttributeAsUnsigned(const char* name, unsigned defaultValue = 0) const;

  /**
   * @brief getter for attribute value as unsigned long long
   * @param name pointer to attribute name
   * @param defaultValue value to be returned if attribute does not exist or its value is empty
   * @return attribute value as unsigned long long
  */
  unsigned long long GetAttributeAsULL(const char* name, unsigned long long defaultValue = 0L) const;

  /**
   * @brief getter for attribute value as boolean
   * @param name attribute name
   * @param defaultValue value to be returned if attribute does not exist
   * @return true if attribute value is equal to "1" or "true", otherwise false
  */
  bool GetAttributeAsBool(const std::string& name, bool defaultValue = false) const;

  /**
   * @brief getter for attribute value as integer
   * @param name attribute name
   * @param defaultValue returned value in case attribute does not exist or its value is empty
   * @return attribute value as integer
  */
  int  GetAttributeAsInt(const std::string& name, int defaultValue = -1) const;

  /**
   * @brief getter for attribute value as unsigned long
   * @param name attribute name
   * @param defaultValue value to be returned if attribute does not exist or its value is empty
   * @return attribute value as unsigned long
  */
  unsigned  GetAttributeAsUnsigned(const std::string& name, unsigned defaultValue = 0) const;

  /**
   * @brief getter for attribute value as unsigned long long
   * @param name attribute name
   * @param defaultValue value to be returned if attribute does not exist or its value is empty
   * @return attribute value as unsigned long long
  */
  unsigned long long GetAttributeAsULL(const std::string& name, unsigned long long defaultValue = 0L) const;

  /**
   * @brief determine prefix of attribute value
   * @param delimiter character value as delimiter
   * @return prefix of attribute value
  */
  std::string GetAttributePrefix(const char* name, char delimiter = ':') const;
  /**
   * @brief determine suffix of attribute value
   * @param name attribute name
   * @param delimiter character value as delimiter
   * @return suffix of attribute value
  */
  std::string GetAttributeSuffix(const char* name, char delimiter = ':') const;
  /**
   * @brief determine suffix as integer of attribute value
   * @param name attribute name
   * @param delimiter character value as delimiter
   * @return suffix as integer of attribute value
  */
  int GetAttributeSuffixAsInt(const char* name, char delimiter = ':') const;

  /**
   * @brief getter for tag text as boolean
   * @param defaultValue value to be returned if text is empty
   * @return tag text as boolean
  */
  bool  GetTextAsBool(bool defaultValue = false) const;

  /**
   * @brief getter for tag text as integer
   * @param defaultValue value to be returned if text is empty
   * @return tag text as integer
  */
  int  GetTextAsInt(int defaultValue = -1) const;

  /**
   * @brief getter for tag text as unsigned long
   * @param defaultValue value to be returned if text is empty
   * @return tag text as unsigned long
  */
  unsigned  GetTextAsUnsigned(unsigned defaultValue = 0) const;

  /**
   * @brief getter for tag text as unsigned long long
   * @param defaultValue value to be returned if text is empty
   * @return tag text as unsigned long long
  */
  unsigned long long GetTextAsULL(unsigned long long defaultValue = 0L) const;

  /**
   * @brief getter for attribute value or child text if attribute does not exist. The default implementation considers only attribute
   * @param keyOrTag name of attribute or child element
   * @return string containing attribute value or child text if attribute does not exist. The default implementation considers only attribute
  */
  virtual const std::string& GetItemValue(const std::string& keyOrTag) const { return GetAttribute(keyOrTag); }

  /**
   * @brief getter for attribute value or child text as boolean if attribute does not exist. The default implementation considers only attribute
   * @param keyOrTag name of attribute or child element
   * @param defaultValue value to be returned if attribute does not exist or its value is empty
   * @return boolean value of attribute or child text if attribute does not exist. The default implementation considers only attribute
  */
  virtual bool GetItemValueAsBool(const std::string& keyOrTag, bool defaultValue = false) const;

  /**
   * @brief getter for attribute value or child text as integer if attribute does not exist. The default implementation considers only attribute
   * @param keyOrTag name of attribute or child element
   * @param defaultValue value to be returned if both attribute and child element do not exist
   * @return integer value of attribute or child text. The default implementation considers only attribute
  */
  virtual int GetItemValueAsInt(const std::string& keyOrTag, int defaultValue = -1) const;

  /**
   * @brief setter for attribute if exists otherwise create a child element if necessary and set text for it. The default implementation considers only attribute
   * @param keyOrTag name of attribute or child element
   * @param value attribute value or text of child element. The default implementation considers only attribute
  */
  virtual void SetItemValue(const std::string& keyOrTag, const std::string& value) {
    AddAttribute(keyOrTag, value);
  }

  /**
   * @brief check if text of child element is not empty or attribute of the instance exists. The default implementation always return true
   * @param keyOrTag name of attribute or child element
   * @return true if text of child element is not empty or attribute of the instance exists. The default implementation always return true
  */
  virtual bool IsAttributeKey(const std::string& keyOrTag) const { return true; } // default is always true

 /**
   * @brief check if instance is valid
   * @return true if instance is valid. The default implementation returns true
  */
  virtual bool IsValid() const { return true; }

  /**
   * @brief set if instance is valid
   * @param bValid valid flag
  */
  virtual void SetValid(bool bValid) {}

  /**
   * @brief getter for 1-based line number of the tag in XML file
   * @return 1-based line number of the tag in XML file
  */
  int  GetLineNumber() const { return m_lineNumber; }

  /**
   * @brief setter for 1-based line number of the tag in XML file
   * @param lineNumber 1-based line number of the tag in XML file
  */
  void SetLineNumber(int lineNumber) { m_lineNumber = lineNumber; }

  /**
  * @brief get number of attributes
  * @return number of attributes as int
 */
  size_t GetAttributeCount() const { return m_attributes.size(); }

  /**
 * @brief check if all given attributes exist in the instance
 * @param attributes given list of attributes
 * @return true if all given attributes exist in the instance
*/
  virtual bool EqualAttributes(const std::map<std::string, std::string>& attributes) const;
  /**
   * @brief check if all attributes of the given instance exist in this instance
   * @param other given instance of XmlItem
   * @return true if all attributes of the given instance exist in this instance
  */
  virtual bool EqualAttributes(const XmlItem& other) const;
  /**
   * @brief check if all attributes of the given instance exist in this instance
   * @param other pointer to given instance of XmlItem
   * @return true if all attributes of the given instance exist in this instance
  */
  virtual bool EqualAttributes(const XmlItem* other) const;

  /**
 * @brief check if given attributes exist in the instance
 * @param attributes given list of attributes
 * @return true if given attributes exist in the instance
*/
  virtual bool CompareAttributes(const std::map<std::string, std::string>& attributes) const;
  /**
   * @brief check if attributes of the given instance exist in this instance
   * @param other given instance of XmlItem
   * @return true if attributes of the given instance exist in this instance
  */
  virtual bool Compare(const XmlItem& other) const;
  /**
   * @brief check if attributes of the given instance exist in this instance
   * @param other pointer to given instance of XmlItem
   * @return true if attributes of the given instance exist in this instance
  */
  virtual bool Compare(const XmlItem* other) const;

  /**
  * @brief concatenate instance attributes
  * @parem quote insert quotes around value strings
  * @return string containing all attributes
 */
  std::string GetAttributesString(bool quote = false) const;
  /**
   * @brief concatenate attributes in a string conformed to XML syntax
   * @return XML string containing attributes
  */
  std::string GetAttributesAsXmlString() const;

  /**
   * @brief set filename associated with the root item this instance belongs to
   * @param rootFileName absolute file name string
  */
  virtual void SetRootFileName(const std::string& rootFileName) {
    AddAttribute(".", rootFileName, false);  // default adds "." attribute
  }

  /**
   * @brief get absolute filename associated with the root item this instance belongs to
   * @return file name string, empty if no file is associated
  */
  virtual const std::string& GetRootFileName() const { return GetAttribute("."); }

  /**
   * @brief return absolute path of the file this item is read from or associated with
   * @param withTrailingSlash true if returned path should contain trailing slash
   * @return file path string, empty if no file is associated (default)
  */
  virtual const std::string GetRootFilePath(bool withTrailingSlash = true) const;

protected:
  /**
   * @brief perform changes in internal data after calls to SetAttributes(), AddAttributes() and ClearAttributes()
  */
  virtual void ProcessAttributes() {/* default does nothing */};

protected:
  std::string m_tag;  // item tag
  std::string m_text; // item text
  std::map<std::string, std::string> m_attributes; // attribute key-value pairs

  int m_lineNumber;  // 1 - based line number in XML file

public:
  static const std::string EMPTY_STRING;
};

#endif // XmlItem_H
