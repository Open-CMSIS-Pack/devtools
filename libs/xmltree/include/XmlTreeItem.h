#ifndef XmlTreeItem_H
#define XmlTreeItem_H
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
#include "XmlItem.h"

#include <list>
#include <string>

/**
 * @brief enumerators for visitor class
*/
enum VISIT_RESULT {
  CONTINUE_VISIT, // continue processing
  SKIP_CHILDREN,  // skip processing of the children
  CANCEL_VISIT    // abort any further processing
};


/**
 * @brief Abstract visitor class, allows to perform operations over XmlItem tree according to visitor design pattern
*/
template<class TV>
class XmlItemVisitor
{
public:
  /**
   * @brief destructor
  */
  virtual ~XmlItemVisitor() {};

  /**
   * @brief callback function to perform additional operations on instance of template type TV
   * @param item pointer to instance of template type TV
   * @return enumerator for continuing, skip children, or abort processing
  */
  virtual VISIT_RESULT Visit(TV* item) = 0;
};


/**
 * @brief class that represents tree structure of XmlItem elements
 * @param TITEM template class must be derived from XmlTreeItem
*/
template<class TITEM>
class XmlTreeItem : public XmlItem
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent element
  */
  XmlTreeItem(TITEM* parent = nullptr) : m_parent(parent) {}

  /**
   * @brief constructor
   * @param parent pointer to parent element
   * @param tag name of tag
  */
  XmlTreeItem(TITEM* parent, const std::string& tag) :XmlItem(tag), m_parent(parent) {}

  /**
   * @brief destructor
  */
  virtual ~XmlTreeItem() {
    m_parent = nullptr;
    XmlTreeItem::Clear();
  }

  /**
   * @brief clear children and attributes of the instance
  */
  virtual void Clear() override
  {
    for (auto it = m_children.begin(); it != m_children.end(); it++) {
      delete *it;
    }
    m_children.clear();
    XmlItem::Clear();
  }

  /**
   * @brief pure virtual getter for this pointer
   * @return this pointer
  */
  virtual TITEM* GetThis() const = 0;

  /**
   * @brief pure virtual function for creating a tag
   * @param tag name of tag
   * @return pointer to an instance of the template class
  */
  virtual TITEM* CreateItem(const std::string& tag) = 0;

  /**
   * @brief getter for member m_parent of template type
   * @return pointer to an instance of the template class
  */
  TITEM* GetParent() const { return m_parent; }

  /**
   * @brief getter for root item
   * @return root item of template type TITEM
  */
  TITEM* GetRoot() const {
    if (!GetParent())
      return GetThis();
    else
      return GetParent()->GetRoot();
  }

  /**
   * @brief change parent instance to another one
   * @param newParent pointer to another instance of template type TITEM
  */
  virtual void Reparent(TITEM* newParent) {
    TITEM* parent = GetParent();
    if (parent) {
      parent->RemoveChild(GetThis(), false);
    }
    m_parent = newParent;
    if (newParent)
      newParent->AddChild(GetThis());
  }

  /**
   * @brief copy content of the instance to another one
   * @param newItem pointer to another instance
  */
  virtual void CopyTo(TITEM* newItem) const {
    if (!newItem || newItem == this) // no destination or this object
      return;
    newItem->Clear();
    newItem->SetText(GetText());
    newItem->SetAttributes(GetAttributes());
    for (auto it = m_children.begin(); it != m_children.end(); it++) {
      TITEM* child = (*it);
      if (child) {
        child->Clone(newItem);
      }
    }
  }

  /**
   * @brief create a new item and copy content to it
   * @param newParent parent of the cloned item
   * @return pointer to cloned item of template type TIEM
  */
  virtual TITEM* Clone(TITEM* newParent) const {
    TITEM* parent = GetParent();
    if (!parent)
      return nullptr; // cannot create top-level item: no factory is available, use XmlTreeItemBuilder to clone
    if (parent == newParent)
      return GetThis(); // nothing to clone

    TITEM* clone = parent->CreateItem(GetTag());
    if (clone && clone != this) { // some factories could return 'this'
      clone->Reparent(newParent);
    }
    else {
      clone = newParent;
    }
    CopyTo(clone);
    return clone;
  }

  //child hierarchy

  /**
   * @brief getter for a static list of pointer to instances of template type TITEM
   * @return static list of pointer to instances of template type TITEM
  */
  const std::list<TITEM*>& GetEmptyList() const { static const std::list<TITEM*> sEmptyList; return sEmptyList; }

  /**
   * @brief  getter for member m_children
   * @return list of pointer to instances of template type TITEM
  */
  const std::list<TITEM*>& GetChildren() const { return m_children; }

  /**
   * @brief getter for grand children of a child specified by tag
   * @param tag name of child tag
   * @return list of pointer to instances of template type TITEM or empty list if child does not exist
  */
  const std::list<TITEM*>& GetGrandChildren(const std::string& tag) const {
    TITEM* child = GetFirstChild(tag);
    if (child)
      return child->GetChildren();
    return GetEmptyList();
  }

  /**
   * @brief check if instance has children
   * @return true if instance has children, otherwise false
  */
  bool HasChildren() const { return !m_children.empty(); }

  /**
   * @brief add a new child to the instance
   * @param child pointer to a new child of template type TITEM
   * @param prepend true to insert child at the front, false to append child at the list end
   * @return pointer to the new child
  */
  virtual TITEM* AddChild(TITEM* child, bool prepend) {
    // returns TITEM* to allow something like newChild = AddChild(CreateItem(tag))
    if (child && child != GetThis() && child->GetParent() == GetThis()) {
      if (prepend)
        m_children.push_front(child);
      else
        m_children.push_back(child);
    }
    return child;
  }

  /**
   * @brief append a new child at the end of the list
   * @param child pointer to a new child of template type TITEM
   * @return pointer to the new child
  */
  virtual TITEM* AddChild(TITEM* child) {
    return AddChild(child, false);
  }

  /**
   * @brief getter for the first child in the list of the instance
   * @return pointer to the first child
  */
  virtual TITEM* GetFirstChild() const
  {
    auto it = m_children.begin();
    if (it != m_children.end())
      return *it;
    return nullptr;
  }

  /**
   * @brief getter for a child element specified by tag
   * @param tag name of child element
   * @return pointer to child element or nullptr if child does not exist
  */
  virtual TITEM* GetFirstChild(const std::string& tag) const
  {
    for (auto it = m_children.begin(); it != m_children.end(); it++) {
      TITEM* child = (*it);
      if (child && tag == child->GetTag()) {
        return child;
      }
    }
    return nullptr;
  }

  /**
   * @brief getter for a child specified by tag, create it if not existing
   * @param tag name of the child to be returned
   * @return pointer to a child of template type TITEM
  */
  virtual TITEM* GetOrCreateChild(const std::string& tag)
  {
    TITEM* child = GetFirstChild(tag);
    if (child)
      return child;
    child = CreateItem(tag);
    AddChild(child);
    return child;
  }

  /**
   * @brief remove child specified by its instance pointer
   * @param childToDelete pointer to child instance
   * @param bDelete true to delete child instance, false to only remove it from the list of children
  */
  virtual void RemoveChild(TITEM* childToDelete, bool bDelete)
  {
    for (auto it = m_children.begin(); it != m_children.end(); it++) {
      TITEM* child = (*it);
      if (child == childToDelete) {
        m_children.erase(it);
        if (bDelete)
          delete childToDelete;
        return;
      }
    }
    return;
  }

  /**
   * @brief remove child specified by its instance reference
   * @param tag name of child to be removed
   * @param bDelete true to delete child instance, false to only remove it from the list of children
  */
  virtual void RemoveChild(const std::string& tag, bool bDelete)
  {
    for (auto it = m_children.begin(); it != m_children.end(); it++) {
      TITEM* child = (*it);
      if (child && tag == child->GetTag()) {
        m_children.erase(it);
        if (bDelete)
          delete child;
        return;
      }
    }
  }

  /**
   * @brief recursively look for an instance of template type TITEM specified by tag and tag text
   * @param tag name of tag
   * @param text text of tag
   * @return pointer to the instance found, otherwise nullptr
  */
  virtual TITEM* FindItem(const char* tag, const char* text) const
  {
    if (!tag || GetTag() == tag) {
      if (!text || GetText() == text)
        return GetThis();
    }
    for (auto it = m_children.begin(); it != m_children.end(); it++) {
      TITEM* e = (*it)->FindItem(tag, text);
      if (e)
        return e;
    }
    return nullptr;
  }

  /**
   * @brief getter for file name associated with this instance
   * @return file name string
  */
  virtual const std::string& GetFileName() const
  {
    TITEM* root = GetRoot();
    if (root)
      return root->GetFileName();
    return EMPTY_STRING;
  }

  /**
   * @brief function used to control the processing
   * @param visitor class based on visitor design pattern for additional operations
   * @return true if successfully processed otherwise false
  */
  virtual bool AcceptVisitor(XmlItemVisitor<TITEM>* visitor) {
    if (visitor) {
      VISIT_RESULT res = visitor->Visit(GetThis());
      if (res == CANCEL_VISIT) {
        return false;
      }
      if (res == CONTINUE_VISIT) {
        for (auto it = m_children.begin(); it != m_children.end(); it++) {
          if (!(*it)->AcceptVisitor(visitor))
            return false;
        }
      }
      return true;
    }
    return false;
  }

  /**
   * @brief setter for text of a child given by tag
   * @param tag name of child instance
   * @param text text to set
  */
  virtual void SetChildText(const std::string& tag, const std::string& text) {
    GetOrCreateChild(tag)->SetText(text);
  }

  /**
   * @brief check if instance is empty
   * @return true if instance is empty otherwise false
  */
  virtual bool IsEmpty() const override { return !HasChildren() && XmlItem::IsEmpty(); }

  /**
   * @brief check if the given name is an attribute one
   * @param keyOrTag attribute name
   * @return true if string corresponds to attribute name
  */
  virtual bool IsAttributeKey(const std::string& keyOrTag) const override {
    return !GetText().empty() || HasAttribute(keyOrTag);
  };

  /**
   * @brief getter for first child text
   * @param tag name of child
   * @return text of the first child specified by tag or empty string if child is not found
  */
  const std::string& GetChildText(const std::string& tag) const {
    TITEM* child = GetFirstChild(tag);
    if (child)
      return child->GetText();
    return EMPTY_STRING;
  }

  /**
   * @brief setter for attribute or child on first level
   * @param keyOrTag name of attribute or child
   * @param value string to set
  */
  virtual void SetItemValue(const std::string& keyOrTag, const std::string& value) override {
    if (IsAttributeKey(keyOrTag)) {
      AddAttribute(keyOrTag, value);
    } else {
      SetChildText(keyOrTag, value);
    }
  }

  /**
   * @brief getter for attribute or child text
   * @param keyOrTag name of attribute or child on first level
   * @return value string of attribute or child text
  */
  const std::string& GetItemValue(const std::string& keyOrTag) const override {
    if (HasAttribute(keyOrTag)) {
      return GetAttribute(keyOrTag);
    }
    return GetChildText(keyOrTag);
  }

protected:
  // set new parent without changing the hierarchy. use with care!
  void SetParent(TITEM* parent) { m_parent = parent; }

protected:
  TITEM* m_parent;
  std::list<TITEM*> m_children; // child elements
};

#endif // XmlTreeItem_H
