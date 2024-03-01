#ifndef IXmlItemBuilder_H
#define IXmlItemBuilder_H
/******************************************************************************/
/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "XmlItem.h"
#include <string>

/**
 * @brief abstract factory interface class to create XmlItem derived objects
*/
class IXmlItemBuilder
{
public:
  /**
   * @brief virtual destructor
  */
  virtual ~IXmlItemBuilder(){};

  /**
   * @brief clears and initializes builder
   * @param bDeleteContent indicates if to delete created items, default is false
  */
  virtual void Clear(bool bDeleteContent = false) { m_fileName.clear();}

  /**
   * @brief setter for file name
   * @param fileName name of file
  */
  virtual void SetFileName(const std::string& fileName) { m_fileName = fileName;}

  /**
   * @brief getter for file name
   * @return file name
  */
  const std::string& GetFileName() const { return m_fileName;}

  /**
   * @brief pure virtual function to create an item specified by tag
   * @param tag name of new tag
   * @return true if item is successfully created
  */
  virtual bool CreateItem(const std::string& tag) = 0;

  /**
   * @brief check if the interface created member root item
   * @return true if root item has been already created
  */
  virtual bool HasRoot() const = 0;

  /**
   * @brief pure virtual function to add created item to parent's children
  */
  virtual void AddItem() = 0;

  /**
   * @brief pure virtual function for adding an attribute
   * @param key attribute name
   * @param value attribute value
  */
  virtual void AddAttribute(const std::string& key, const std::string& value) = 0;

  /**
   * @brief pure virtual function for setting text of current item
   * @param text string to set
  */
  virtual void SetText(const std::string& text) = 0;

  /**
   * @brief pure virtual function got called before a new item is created
  */
  virtual void PreCreateItem() = 0;

  /**
   * @brief pure virtual function got called after a new item has been created
   * @param success flag indicating if item has been created successfully
  */
  virtual void PostCreateItem(bool success) = 0;

  /**
   * @brief pure virtual function for setting line number of the current item
   * @param lineNumber line number to set
  */
  virtual void SetLineNumber(int lineNumber) = 0;
protected:
  std::string m_fileName;

};


#endif // IXmlItemBuilder_H
