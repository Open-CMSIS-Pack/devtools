#ifndef RteItem_H
#define RteItem_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteItem.h
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteCallback.h"

#include "RteAttributes.h"
#include "RteUtils.h"

#include "XmlTreeItem.h"

#include <set>
#include <ostream>
#include <functional>

/**
 * @brief state of a pack
*/
enum PackageState {
  PS_INSTALLED,  // pack is installed
  PS_AVAILABLE,  // pack is listed in .web folder (available to download)
  PS_DOWNLOADED, // pack is downloaded to .download folder
  PS_UNKNOWN,    // state is unknown
  PS_COUNT = PS_UNKNOWN, // count of automatic states
  PS_GENERATED   // generated pack
};


class RteCondition;
class RteConditionContext;
class RteComponent;
class RteApi;
class RtePackage;
class RteModel;
class RteItem;
class RteTarget;
class RteProject;
class RteDeviceItem;
class RteDeviceFamily;
class RteDependencyResult;
class RteFileInstance;
class RteFile;
class XMLTreeElement;


/**
 * @brief Abstract visitor class. Allows to perform operations over RteItem tree according to visitor design pattern
*/
class RteVisitor
{
public:
  /**
   * @brief default virtual destructor
  */
  virtual ~RteVisitor() {};

  /**
   * @brief virtual method to perform processing over supplied RteItem during visit
   * @param item pointer to RteItem to process
   * @return one of VISIT_RESULT values
  */
  virtual VISIT_RESULT Visit(RteItem* item) = 0;
};



/**
 * @brief base RTE Data Model class to describe an XML element in a *.pdsc and *.cprj files.
*/
class RteItem : public RteAttributes
{

public:
  typedef std::function<bool(RteItem* c0, RteItem* c1)> CompareRteItemType;

  /**
   * @brief result of evaluating conditions and condition expressions
  */
  enum ConditionResult {
    UNDEFINED,            // not evaluated yet
    R_ERROR,              // error evaluating condition ( recursion detected, condition is missing)
    FAILED,               // HW or compiler not match
    MISSING,              // no component is installed
    MISSING_API,          // no required api is installed
    MISSING_API_VERSION,  // no api of required version is installed
    UNAVAILABLE,          // component is installed, but filtered out
    UNAVAILABLE_PACK,     // component is installed, pack is not selected
    INCOMPATIBLE,         // incompatible component is selected
    INCOMPATIBLE_VERSION, // incompatible version of component is selected
    INCOMPATIBLE_VARIANT, // incompatible variant of component is selected
    CONFLICT,             // more than one exclusive component selected
    INSTALLED,            // matching component is installed, but not selectable because not in active bundle
    SELECTABLE,           // matching component is installed, but not selected
    FULFILLED,            // required component selected or no dependency exists
    IGNORED               // condition/expression is irrelevant for the current context
  };

public:

  /**
   * @brief constructor
   * @param parent pointer to parent RteItem or nullptr if this item has no parent
  */
  RteItem(RteItem* parent);

  /**
   * @brief virtual destructor
  */
  virtual ~RteItem() override;

public:

  /**
   * @brief get line number corresponding to position of the item's tag in associated file (is supported by XML parser)
   * @return line number or 0 if not set
  */
  int GetLineNumber() const { return m_lineNumber; }

  /**
   * @brief set line number corresponding to position of the item's tag in associated file
   * @param lineNumber 1-based integer
  */
  void SetLineNumber(int lineNumber) { m_lineNumber = lineNumber; }

  /**
   * @brief get item's text
   * @return item's text string, empty if not set
  */
  const std::string& GetText() const { return m_text; }

  /**
   * @brief set item's text
   * @param text string to set
  */
  void SetText(const std::string& text) { m_text = text; }

  /**
   * @brief get number of children
   * @return number of children
  */
  int GetChildCount() const { return (int)m_children.size(); }

  /**
   * @brief returns list of children
   * @return list of RteItem pointers, may be empty
  */
  const std::list<RteItem*>& GetChildren() const { return m_children; }

  /**
   * @brief get list of children of a child item with given tag
   * @param tag child's tag to query
   * @return list of RteItem pointers
  */
  const std::list<RteItem*>& GetGrandChildren(const std::string& tag) const;

  /**
   * @brief get a child with corresponding tag and attribute
   * @param tag item's tag
   * @param attribute item's attribute key
   * @param value item's attribute value
   * @return pointer to child RteItem or nullptr
  */
  RteItem* GetChildByTagAndAttribute(const std::string& tag, const std::string& attribute, const std::string& value) const;

  /**
   * @brief static method to query children of RteItem
   * @param item RteItem whose children to query
   * @return list of RteItem pointers
  */
  static const std::list<RteItem*>& GetItemChildren(RteItem* item);

  /**
   * @brief static method to query grandchildren of RteItem for specified child tag
   * @param item pointer to RteItem whose grandchildren to query
   * @param tag child's tag
   * @return list of RteItem pointers
  */
  static const std::list<RteItem*>& GetItemGrandChildren(RteItem* item, const std::string& tag);

  /**
   * @brief checks if this item is in valid state
   * @return true if item is valid
  */
  bool IsValid() const { return m_bValid; }

  /**
   * @brief get child item for specified ID
   * @param id child's ID'
   * @return pointer to RteItem or nullptr
  */
  RteItem* GetItem(const std::string& id) const;

  /**
   * @brief checks if supplied item exists in the list of children
   * @param item pointer to RteItem to check
   * @return true if supplied item is found in the list of children
  */
  bool HasItem(RteItem* item) const;

  /**
   * @brief get the first child item with given tag if any
   * @param tag child's tag
   * @return pointer to RteItem or nullptr
  */
  RteItem* GetItemByTag(const std::string& tag) const;

  /**
   * @brief add item to the list of children
   * @param item pointer to RteItem to add
  */
  void AddItem(RteItem* item) { m_children.push_back(item); }

  /**
   * @brief remove item from the list of children
   * @param item pointer to RteItem to remove
  */
  void RemoveItem(RteItem* item);

  /**
   * @brief get child's attribute value
   * @param tag child's tag
   * @param attribute child's attribute key
   * @return child's attribute value string
  */
  const std::string& GetChildAttribute(const std::string& tag, const std::string& attribute) const;

  /**
   * @brief get child's text
   * @param tag child's tag
   * @return child's text string
  */
  const std::string& GetChildText(const std::string& tag) const;

  /**
   * @brief get rte folder associated with this item
   * @return rte folder
  */
  virtual const std::string& GetRteFolder() const;

  /**
   * @brief get a value that is either attribute value or a child's text
   * @param nameOrTag attribute key or child's tag
   * @return string value, empty if not found
  */
  virtual const std::string& GetItemValue(const std::string& nameOrTag) const;

  /**
   * @brief get url or file path to documentation associated with this item
   * @return url or file path
  */
  virtual const std::string& GetDocValue() const;

  /**
   * @brief get vendor associated with the item
   * @return vendor string
  */
  virtual const std::string& GetVendorString() const override;

  /**
   * @brief sort children of an RteItem instance
   * @param cmp compare function which will be called by std::list::sort
  */
  void SortChildren(CompareRteItemType cmp);

  /**
   * @brief compare two components in ascending order
   * @param c0 component to be compared
   * @param c1 component to be compared
   * @return true if c0 smaller than c1
  */
  static bool CompareComponents(RteItem* c0, RteItem* c1);

public:
  /**
   * @brief get the immediate parent item of this one
   * @return pointer to parent RteItem or nullptr if this item has no parent
  */
  RteItem* GetParent() const { return m_parent; }

  /**
   * @brief change parent for this item
   * @param newParent new parent for this item, can be nullptr
  */
  void Reparent(RteItem* newParent);

  /**
   * @brief get RteCallback available for this item if any
   * @return pointer to RteCallback
  */
  virtual RteCallback* GetCallback() const;

  /**
   * @brief search for RteModel item in the parent chain
   * @return pointer to this or parent RteModel or nullptr
  */
  virtual RteModel* GetModel() const;

  /**
   * @brief search for RtePackage item in the parent chain
   * @return pointer to this or parent RtePackage or nullptr
  */
  virtual RtePackage* GetPackage() const;

  /**
   * @brief search for RteComponent item in the parent chain
   * @return pointer to this or parent RteComponent or nullptr
  */
  virtual RteComponent* GetComponent() const;

  /**
   * @brief get component aggregate ID for this item
   * @return component aggregate ID if this item is an RteComponent or has RteComponent parent
  */
  virtual std::string GetComponentAggregateID() const override;

  /**
   * @brief search for RteProject item in the parent chain
   * @return pointer to this or parent RteProject or nullptr
  */
  virtual RteProject* GetProject() const;

  /**
   * @brief check if this item is deprecated
   * @return true if this item or its package is deprecated
  */
  virtual bool IsDeprecated() const;

  /**
   * @brief collect cached results of evaluating component dependencies for this item
   * @param results map to collect results
   * @param target RteTarget associated with condition context
   * @return overall RteItem::ConditionResult for this item
  */
  virtual ConditionResult GetDepsResult(std::map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const;

  /**
   * @brief evaluate condition attached to this item
   * @param context RteConditionContext to use for evaluation
   * @return evaluated ConditionResult, IGNORED if this item does not have associated condition
  */
  virtual ConditionResult Evaluate(RteConditionContext* context);

  /**
   * @brief get cached condition result for this item
   * @param context condition evaluation context
   * @return cached ConditionResult, previously obtained by Evaluate()
  */
  virtual ConditionResult GetConditionResult(RteConditionContext* context) const;

 public:
  /**
   * @brief get item's ID created by ConstructID() called from Construct()
   * @return item ID string
  */
  virtual const std::string& GetID() const { return m_ID; }

  /**
   * @brief get unique ID of parent or this RteComponent
   * @param withVersion flag to include version information into ID
   * @return component unique ID
  */
  virtual std::string GetComponentUniqueID(bool withVersion) const override;

  /**
   * @brief get item's name to display to user
   * @return name to display
  */
  virtual std::string GetDisplayName() const;


  /**
   * @brief get ID of RtePackage containing this item
   * @param withVersion flag to append version to pack ID
   * @return pack ID if any or empty string
  */
  virtual std::string GetPackageID(bool withVersion = true) const override;

  /**
   * @brief get path to installed pack containing this item
   * @param withVersion flag to include packs's version
   * @return path relative to pack installation directory, path does not need to exist
  */
  virtual std::string GetPackagePath(bool withVersion = true) const;

  /**
   * @brief get PackageState of RtePackage containing this item
   * @return PackageState of parent RtePackage or PS_UNKNOWN if item does not belong to a package
  */
  virtual PackageState GetPackageState() const;

  /**
   * @brief get file name of RtePackage's containing this item
   * @return RtePackage's file name or empty string if item does not belong to a package
  */
  virtual const std::string& GetPackageFileName() const;


  /**
   * @brief get absolute filename associated with this item (only relevant for files, docs and books)
   * @return absolute filename or url
  */
  virtual std::string GetOriginalAbsolutePath() const;

  /**
   * @brief get absolute filename associated with this item
   * @param name filename to append to the absolute path
   * @return constructed absolute path or name if name is a url or already absolute
  */
  virtual std::string GetOriginalAbsolutePath(const std::string& name) const;

  /**
   * @brief check if item provides data matching OS this code is built for: Linux, Windows or Mac OS
   * @return true if item data matches host platform
  */
  virtual bool MatchesHost() const;

  /**
   * @brief expands key sequences ("@L", "%L", etc.) in the supplied string.
   * @param str string to expand
   * @return expanded string
  */
  virtual std::string ExpandString(const std::string& str) const;

  /**
   * @brief get item's description
   * @return description of the item if exist,or empty string
  */
  virtual const std::string& GetDescription() const;

  /**
   * @brief get absolute path to a document fie associated with the item
   * @return absolute path or url
  */
  virtual std::string GetDocFile() const;

  /**
   * @brief composes url to download pack containing the item
   * @param withVersion flag if url should contain version
   * @param extension download file extension
   * @return composed download url
  */
  virtual std::string GetDownloadUrl(bool withVersion, const char* extension) const;

  /**
   * @brief get RteCondition associated with the item
   * @return pointer RteCondition or nullptr if item has no condition
  */
  virtual RteCondition* GetCondition() const;

  /**
   * @brief find RteCondition with given ID in RtePackage containing this item
   * @param id condition ID to search for
   * @return pointer RteCondition or nullptr if no such condition found
  */
  virtual RteCondition* GetCondition(const std::string& id) const;

  /**
   * @brief check if item is associated with a condition that depends on selected device
   * @return true if item's condition depends on selected device
  */
  virtual bool IsDeviceDependent() const;

  /**
  * @brief check if item is associated with a condition that depends on selected board
  * @return true if item's condition depends on selected board
  */
  virtual bool IsBoardDependent() const;

  /**
   * @brief get errors found by Construct() or Validate()
   * @return list of error strings
  */
  const std::list<std::string>& GetErrors() const { return m_errors; }

  /**
   * @brief creates an error string for this item
   * @param severity error severity : "Error", "Warning", "Info", etc.
   * @param errNum error number as a string, e.g. "E042"
   * @param message error message
   * @return created error string
  */
  std::string CreateErrorString(const char* severity, const char* errNum, const char* message) const;

  /**
   * @brief clear internal list of errors
  */
  virtual void ClearErrors() { m_errors.clear(); }

public:

  /**
   * @brief clear internal item structure including children. The method is called from destructor
  */
  virtual void Clear() override;

  /**
   * @brief construct this item out of supplied XML data
   * @param xmlElement XMLTreeElement to construct from
   * @return true if successful
  */
  virtual bool Construct(XMLTreeElement* xmlElement);

  /**
   * @brief validate internal item structure and children recursively and set internal validity flag
   * @return validation result as boolean value
  */
  virtual bool Validate();

  /**
   * @brief reset internal validity flag
  */
  virtual void Invalidate() { m_bValid = false; }

  /**
   * @brief insert this item or its data to supplied RteModel
   * @param model pointer to RteModel, cannot be nullptr
  */
  virtual void InsertInModel(RteModel* model);

  /**
   * @brief accept visitor to perform operations walking through item tree (visitor design pattern)
   * @param visitor pointer to RteVisitor to accept
   * @return true to continue processing, false to abort (cancel visit)
  */
  virtual bool AcceptVisitor(RteVisitor* visitor);

  /**
   * @brief create XMLTreeElement object to export this item to XML
   * @param parentElement parent for created XMLTreeElement
   * @param bCreateContent create XML content out of children
   * @return created XMLTreeElement
  */
  virtual XMLTreeElement* CreateXmlTreeElement(XMLTreeElement* parentElement, bool bCreateContent = true) const;

  /**
   * @brief create child item with given tag and name, add it to children collection
   * @param tag tag to set to created item
   * @param name "name" attribute to set to created item
   * @return pointer to created RteItem
  */
  virtual RteItem* CreateChild(const std::string& tag, const std::string& name = RteUtils::EMPTY_STRING);

protected:
  /**
   * @brief process child elements of supplied XMLTreeElement to extract XML data and create RteItem children
   * @param xmlElement XMLTreeElement whose children to process
   * @return true if successful
  */
  virtual bool ProcessXmlChildren(XMLTreeElement* xmlElement);

  /**
   * @brief process a single XMLTreeElement during construction. The method can recursively call Construct() or ProcessXmlChildren()
   * @param xmlElement XMLTreeElement to process
   * @return true if successful
  */
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement);

  /**
   * @brief construct item's ID
   * @return constructed string ID
  */
  virtual std::string ConstructID();

  /**
   * @brief check if item provides XML content to be generated by CreateXmlTreeElementContent()
   * @return true if element has children or other data to represent as XML text
  */
  virtual bool HasXmlContent() const;

  /**
   * @brief creates child element for supplied XMLTreeElement
   * @param parentElement XMLTreeElement to generate content for
  */
  virtual void CreateXmlTreeElementContent(XMLTreeElement* parentElement) const;

protected:
  RteItem* m_parent; // parent for this item or NULL if it is a model

  bool m_bValid; // validity flag

  int m_lineNumber; // 1 - based line number in XML file

  std::string m_text; // an item text, can be empty
  std::string m_ID; // an item ID, constructed in ConstructID() called by Construct()

  std::list<std::string> m_errors; // errors or warnings found by Construct() or Validate()

  std::list<RteItem*> m_children; // children (files, components, conditions, etc.)
};


/**
 * @brief container class that adds all sub-items as children including text items (<tag>text</tag>)
*/
class RteItemContainer : public RteItem
{
public:
  /**
   * @brief base constructor
   * @param parent pointer to parent RteItem, can be nullptr for top-level items
  */
  RteItemContainer(RteItem* parent);

  /**
   * @brief process a single XMLTreeElement during construction
   * @param xmlElement pointer to XMLTreeElement to process
   * @return true if successful
   */
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement) override;
};


/**
 * @brief Visitor to print error messages
*/
class RtePrintErrorVistior: public RteVisitor
{
public:
  /**
   * @brief constructor
   * @param callback RteCallback that provides methods to output messages
  */
  RtePrintErrorVistior(RteCallback* callback): m_callback(callback) {}

public:

	virtual VISIT_RESULT Visit(RteItem* rteItem) override
	{
    if(rteItem->IsValid())
      return VISIT_RESULT::SKIP_CHILDREN;
    const std::list<std::string>& errors = rteItem->GetErrors();
    if(errors.empty())
      return CONTINUE_VISIT;
    std::list<std::string>::const_iterator it;
    for( it = errors.begin(); it != errors.end(); it++){
      std::string sErr= (*it);
      if(m_callback) {
        m_callback->OutputMessage(sErr);
      }
    }
    return VISIT_RESULT::CONTINUE_VISIT;
  }

private:
  RteCallback* m_callback; // callback to output  messages
};


#endif // RteItem_H
