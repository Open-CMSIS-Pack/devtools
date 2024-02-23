#ifndef XmlTreeItemBuilder_H
#define XmlTreeItemBuilder_H
/******************************************************************************/
/*
  * The classes should be kept semantics-free:
  * no special processing based on tag, attribute or value string
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "IXmlItemBuilder.h"

#include <list>

/**
 * @brief abstract build factory to build XmlTreeItem derived classes
 * @param T template class derived from XmlTreeItem
*/
template<class T>
class XmlTreeItemBuilder : public IXmlItemBuilder
{
public:
  /**
   * @brief default constructor
  */
  XmlTreeItemBuilder() : m_pRoot(0), m_pCurrent(0), m_pParent(0) {}

  /**
   * @brief clear the instance
   * @param bDeleteContent true to delete the member m_pRoot of template type T
  */
  void Clear(bool bDeleteContent = false) override {

    if (bDeleteContent && m_pRoot != 0)
      delete m_pRoot;
    m_pRoot = 0;
    m_pParent = 0;
    m_pCurrent = 0;
    m_stack.clear();
  }

  /**
   * @brief getter for member m_pRoot
   * @return member m_pRoot of template type T
  */
  T* GetRoot() const { return m_pRoot; }

  /**
   * @brief setter for member m_pRoot
   * @param root object of template type T
  */
  void SetRoot(T* root) { m_pRoot = root; }

  /**
   * @brief check member m_pRoot
   * @return true if member m_pRoot not equal 0
  */
  bool HasRoot() const override { return m_pRoot != 0; }

  /**
   * @brief add child specified by member m_pCurrent to parent
  */
  void AddItem() override
  {
    if (m_pParent && m_pParent != m_pCurrent)
      m_pParent->AddChild(m_pCurrent);
  }

  /**
   * @brief add attribute together with value to member m_pCurrent of template type T
   * @param key attribute name
   * @param value attribute value to set in instance of template type T
  */
  void AddAttribute(const std::string& key, const std::string& value) override
  {
    if (m_pCurrent)
      m_pCurrent->AddAttribute(key, value);
  }

  /**
   * @brief setter for tag text of member m_pCurrent of template type T
   * @param text string to set in instance of template type T
  */
  void SetText(const std::string& text) override
  {
    if (m_pCurrent)
      m_pCurrent->SetText(text);
  }

  /**
   * @brief this function gets called before a new item is created
  */
  void PreCreateItem() override
  {
    if (m_pParent)
      m_stack.push_front(m_pParent);
    m_pParent = m_pCurrent;
    m_pCurrent = 0;
  }

  /**
   * @brief create a new item specified by tag
   * @param tag name of item
   * @return true if item is successfully created
  */
  bool CreateItem(const std::string& tag) override
  {
    if (!HasRoot()) {
      m_pRoot = m_pCurrent = CreateRootItem(tag);
    }
    else if (m_pParent) {
      m_pCurrent = m_pParent->CreateItem(tag);
    }
    else {
      m_pCurrent = 0;
    }
    if (m_pCurrent && m_pCurrent != m_pParent) {
      m_pCurrent->SetTag(tag);
    }
    return m_pCurrent != 0;
  }

  /**
   * @brief this function get called after an item has been created
   * @param success validity flag to set for item having been created
  */
  void PostCreateItem(bool success) override
  {
    if (m_pCurrent && m_pCurrent != m_pParent) {
      m_pCurrent->Construct();
      m_pCurrent->SetValid(success);
    }

    m_pCurrent = m_pParent;
    auto it = m_stack.begin();
    if (it != m_stack.end()) {
      m_pParent = *it;
      m_stack.pop_front();
    }
    else {
      m_pParent = 0;
    }
  }

  /**
   * @brief setter for line number of the current item in the instance
   * @param lineNumber line number to set
  */
  void SetLineNumber(int lineNumber) override
  {
    if (m_pCurrent && m_pCurrent != m_pParent)
      m_pCurrent->SetLineNumber(lineNumber);
  }

  /**
   * @brief create a new item and copy content to it
   * @param item instance to be cloned
   * @param newParent parent of the cloned item in case item is not a root one
   * @return cloned item of template type T
  */
  virtual T* Clone(T* item, T* newParent) {
    if (item->GetParent())
      return item->Clone(newParent);
    T* clone = CreateRootItem(item->GetTag());
    item->CopyTo(clone);
    return clone;
  }

protected:
  /**
   * @brief pure virtual function to create an item specified by tag
   * @param tag name of new tag
   * @return pointer to created item
  */
  virtual T* CreateRootItem(const std::string& tag) = 0;

protected:
  T* m_pRoot;
  T* m_pCurrent;
  T* m_pParent;

  std::list<T*> m_stack;
};

#endif // XmlTreeItemBuilder_H
