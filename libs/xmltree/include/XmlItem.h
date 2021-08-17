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
  XmlItem() :m_lineNumber(0) {};

  /**
   * @brief constructor
   * @param tag XML tag
  */
  XmlItem(const std::string& tag) : m_tag(tag), m_lineNumber(0) {};

  /**
   * @brief destructor
  */
  virtual ~XmlItem() {};

  /**
   * @brief remove all attributes of the instance
  */
  virtual void Clear();

  /**
   * @brief called to construct a XML node with attributes and child elements. Default does nothing.
  */
  virtual void Construct() {};

  /**
   * @brief check if XML element is empty, i.e. if text and attributes list are empty
   * @return
  */
  virtual bool IsEmpty() { return m_text.empty() && m_attributes.empty(); }

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
   * @brief getter for tag text
   * @return string containing tag text
  */
  const std::string& GetText() const { return m_text; }

  /**
   * @brief setter for tag text
   * @param text text be set
  */
  void SetText(const std::string& text) { m_text = text; }

  /**
   * @brief getter for list of attribute name mapped to attribute value
   * @return list of attribute name mapped to attribute value
  */
  const std::map<std::string, std::string>& GetAttributes() const { return m_attributes; }

  /**
   * @brief setter for list of attribute name mapped to its value
   * @param attributes list of attribute name mapped to its value
  */
  void SetAttributes(const std::map<std::string, std::string>& attributes) { m_attributes = attributes; }

  /**
   * @brief add an attribute to the instance
   * @param name attribute name
   * @param value attribute value
  */
  void AddAttribute(const std::string& name, const std::string& value);

  /**
   * @brief remove the given attribute from the instance
   * @param name attribute name
   * @return true if attribute is removed, false if it does not exist
  */
  bool RemoveAttribute(const std::string& name);

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
   * @brief getter for attribute value as boolean
   * @param name pointer to attribute name
   * @param defaultValue value to be returned if attribute does not exist
   * @return true if attribute value is equal to "1" or "true", otherwise false
  */
  bool      GetAttributeAsBool(const char* name, bool defaultValue = false) const;

  /**
   * @brief getter for attribute value as integer
   * @param name pointer to attribute name
   * @param defaultValue returned value in case attribute does not exist or its value is empty
   * @return attribute value as integer
  */
  int       GetAttributeAsInt(const char* name, int defaultValue = -1) const;

  /**
   * @brief getter for attribute value as unsigned long
   * @param name pointer to attribute name
   * @param defaultValue value to be returned if attribute does not exist or its value is empty
   * @return attribute value as unsigned long
  */
  unsigned  GetAttributeAsUnsigned(const char* name, unsigned defaultValue = 0) const;

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
  bool      GetAttributeAsBool(const std::string& name, bool defaultValue = false) const;

  /**
   * @brief getter for attribute value as integer
   * @param name attribute name
   * @param defaultValue returned value in case attribute does not exist or its value is empty
   * @return attribute value as integer
  */
  int       GetAttributeAsInt(const std::string& name, int defaultValue = -1) const;

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
   * @brief getter for tag text as boolean
   * @param defaultValue value to be returned if text is empty
   * @return tag text as boolean
  */
  bool      GetTextAsBool(bool defaultValue = false) const;

  /**
   * @brief getter for tag text as integer
   * @param defaultValue value to be returned if text is empty
   * @return tag text as integer
  */
  int       GetTextAsInt(int defaultValue = -1) const;

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
  virtual bool GetItemValueAsBool(const std::string& keyOrTag, bool defaultValue = false) const { return GetAttributeAsBool(keyOrTag, defaultValue); }

  /**
   * @brief getter for attribute value or child text as integer if attribute does not exist. The default implementation considers only attribute
   * @param keyOrTag name of attribute or child element
   * @param defaultValue value to be returned if both attribute and child element do not exist
   * @return integer value of attribute or child text. The default implementation considers only attribute
  */
  virtual int GetItemValueAsInt(const std::string& keyOrTag, int defaultValue = -1)  const { return GetAttributeAsInt(keyOrTag, defaultValue); }

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
   * @brief convert string to boolean
   * @param value given string
   * @param defaultValue value to be returned if given string is empty
   * @return true if given string is equal to "1" or "true"
  */
  static bool      StringToBool(const std::string& value, bool defaultValue = false);

  /**
   * @brief convert string to integer
   * @param value given string
   * @param defaultValue value to be returned in case of empty string or conversion error
   * @return converted integer value
  */
  static int       StringToInt(const std::string& value, int defaultValue = -1);

  /**
   * @brief convert string to unsigned integer
   * @param value string to be converted
   * @param defaultValue value to be returned in case of empty string or conversion error
   * @return converted unsigned integer value
  */
  static unsigned  StringToUnsigned(const std::string& value, unsigned defaultValue = 0);

  /**
   * @brief convert string to unsigned long long
   * @param value decimal or hexadecimal string as to be converted
   * @param defaultValue value to be returned in case of empty string or conversion error
   * @return converted value of type unsigned long long
  */
  static unsigned long long StringToULL(const std::string& value, unsigned long long defaultValue = 0L);

  /**
   * @brief trim whitespace characters
   * @param s string to be trimmed
   * @return trimmed string without whitespace characters
  */
  static std::string Trim(const std::string& s);
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

protected:
  std::string m_tag;  // item tag
  std::string m_text; // item text
  std::map<std::string, std::string> m_attributes; // attribute key-value pairs

  int m_lineNumber;  // 1 - based line number in XML file

public:
  static const std::string EMPTY_STRING;

};

#endif // XmlItem_H
