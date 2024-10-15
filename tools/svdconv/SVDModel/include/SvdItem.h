/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdItem_H
#define SvdItem_H

#include "SvdTypes.h"
#include "XmlTreeItem.h"
#include "SvdUtils.h"

#include <string>
#include <list>
#include <map>


// Configuration
#define NAME_MAXLEN         32                          // suggestion for a good short name


struct Value {
  bool  bValid;

  union {
    unsigned long       ul;
    uint32_t           u32;
    signed char          c;
    unsigned char       uc;
    signed short       i16;
    unsigned short     u16;
    unsigned short      ui;
    signed long          l;
    int32_t              i;
    int64_t            i64;
    uint64_t           u64;
    float                f;
    double               d;
  };

  Value() :
    bValid(false),
    u64(0)
    {}
};


enum SVD_LEVEL {
  L_UNDEF         = 0,
  L_Device,
  L_Peripherals,
  L_Peripheral,
  L_Registers,
  L_Cluster,
  L_Register,
  L_Fields,
  L_Field,
  L_EnumeratedValues,
  L_EnumeratedValue,
  L_Cpu,
  L_AddressBlock,
  L_Interrupt,
  L_Dim,
  L_DerivedFrom,
  L_SvdSauRegionsConfig,
  L_SvdSauRegion,
  L_DimArrayIndex,
};


class SvdDerivedFrom;
class SvdDimension;
class SvdVisitor;
class XMLTreeElement;
class SvdDevice;


class SvdElement {
public:
  SvdElement();
  virtual ~SvdElement();

  virtual int32_t GetLineNumber() const { return m_lineNumber; }
  virtual void SetLineNumber(uint32_t lineNumber) { m_lineNumber = lineNumber;}

  virtual int32_t  GetColNumber() const { return m_colNumber; }
  virtual void SetColNumber(uint32_t colNumber) { m_colNumber = colNumber;}

  virtual bool SetName (const std::string &name);
  virtual const std::string &GetName();

  virtual bool SetTag(const std::string &tag);
  virtual const std::string &GetTag() { return m_tag; }

  virtual const std::string& GetText() const {return m_text;}
  virtual void  SetText(const std::string& text){ m_text = text;}

  virtual void Invalidate();
  virtual void SetValid(bool bValid);
  bool IsValid() const { return m_bValid; }

  // constructs this item out of XML data
  virtual bool Construct(XMLTreeElement* xmlElement);

private:
  bool m_bValid;
  int32_t m_lineNumber;  // 1 - based line number in XML file
  int32_t m_colNumber;   // 1 - based column number in XML file

  std::string m_text;
  std::string m_tag;
  std::string m_name;
};


class SvdItem : public SvdElement {
public:
  SvdItem(SvdItem* parent);
  virtual ~SvdItem();

  static const uint32_t VALUE32_NOT_INIT;
  static const uint64_t VALUE64_NOT_INIT;


  virtual bool                          Construct                         (XMLTreeElement* xmlElement);
  virtual bool                          ProcessXmlChildren                (XMLTreeElement* xmlElement);
  virtual bool                          ProcessXmlElement                 (XMLTreeElement* xmlElement);
  virtual bool                          ProcessXmlAttributes              (XMLTreeElement* xmlElement);

  bool                                  AddAttribute                      (const std::string& name, const std::string& value, bool insertEmpty = true);
  const std::list<SvdItem*>&            GetChildren                       () const                { return m_children; }
  size_t                                GetChildCount                     () const                { return m_children.size();}
  void                                  AddItem                           (SvdItem* item);
  bool                                  AcceptVisitor                     (SvdVisitor* visitor);
  void                                  DebugModel                        (const std::string &value);
  void                                  Invalidate                        ();
  void                                  ClearChildren                     ();
  bool                                  CopyChilds                        (SvdItem *from, SvdItem *hook);

  virtual bool                          Validate                          ();
  virtual bool                          CopyItem                          (SvdItem *from);
  virtual bool                          CheckItem                         ();
  virtual bool                          Calculate                         ();
  virtual bool                          CalculateDim                      ();
  virtual SvdDevice*                    GetDevice                         () const;
  virtual std::string                   GetNameCalculated                 ();
  virtual const std::string&            GetAlternate                      ();
  virtual const std::string&            GetPrependToName                  ();
  virtual const std::string&            GetAppendToName                   ();
  virtual const std::string&            GetHeaderDefinitionsPrefix        ();
  virtual uint64_t                      GetAddress                        ()                      { return 0; }
  virtual const std::string&            GetAlternateGroup                 ()                      { return SvdUtils::EMPTY_STRING;  }
  virtual uint32_t                      GetSize                           ()                      { return GetEffectiveBitWidth() / 8;  }
  virtual uint64_t                      GetResetValue                     ()                      { return 0;  }
  virtual uint64_t                      GetResetMask                      ()                      { return 0;  }
  virtual SvdTypes::Access              GetAccess                         ()                      { return SvdTypes::Access::UNDEF;     }
  virtual SvdTypes::ModifiedWriteValue  GetModifiedWriteValue             ()                      { return SvdTypes::ModifiedWriteValue::UNDEF;   }
  virtual SvdTypes::ReadAction          GetReadAction                     ()                      { return SvdTypes::ReadAction::UNDEF; }

  template <typename T>
  bool GetDeriveItem(T *&item, const std::list<std::string>& searchName, SVD_LEVEL svdLevel, std::string& lastSearchName);

  bool                                  IsNameRequired                      ();
  bool                                  IsDescrAllowed                      ();

  const std::string&                    GetSvdLevelStr                      ();
  const std::string&                    GetSvdLevelStr                      (SVD_LEVEL level);

  bool                                  CopyDerivedFrom                     (SvdItem *item, SvdItem *from);
  bool                                  CopyDim                             (SvdItem *item, SvdItem *from);
  bool                                  IsDerived                           ()                    { return m_derivedFrom? true : false; }
  bool                                  IsDimed                             ()                    { return m_dimension  ? true : false; }
  bool                                  SetDerivedFrom                      (SvdDerivedFrom *derivedFrom);
  SvdDerivedFrom*                       GetDerivedFrom                      ();
  bool                                  SetDimension                        (SvdDimension *dimension);
  SvdDimension*                         GetDimension                        ();

  bool                                  SetDescription                      (const std::string &descr);
  const std::string&                    GetDescription                      ();
  bool                                  SetDisplayName                      (const std::string &name);
  const std::string&                    GetDisplayName                      ();
  std::string                           GetHierarchicalName                 ();
  std::string                           GetHierarchicalNameResulting        ();
  std::string                           TryGetHeaderStructName              (SvdItem *item);
  const std::string&                    GetPeripheralName                   ();
  bool                                  SetProtection                       (SvdTypes::ProtectionType protection) { m_protection = protection; return true; }
  SvdTypes::ProtectionType              GetProtection                       ()                    { return m_protection; }
  std::string                           GetDeriveName                       ();
  SvdItem*                              GetParent                           ()                    { return m_parent; }
  void                                  SetParent                           (SvdItem* parent)     { m_parent = parent; }
  void                                  SetSvdLevel                         (SVD_LEVEL svdLevel)  { m_svdLevel = svdLevel; }
  SVD_LEVEL                             GetSvdLevel                         ()                    { return m_svdLevel; }

  bool                                  FindChild                           (SvdItem *&item, const std::string &name);
  bool                                  FindChild                           (const std::list<SvdItem*> childs, SvdItem *&item, const std::string &name);
  bool                                  FindChildFromItem                   (SvdItem *&item, const std::string &name);

  void                                  SetModified                         ();
  bool                                  IsModified                          () { return m_modified; }

  uint64_t                              GetAbsoluteAddress                  ();
  uint64_t                              GetAbsoluteOffset                   ();
  bool                                  GetAbsoluteName                     (std::string &absName, char delimiter);

  bool                                  SetCopiedFrom                       (SvdItem* item)       { m_copiedFrom = item; return true; }
  SvdItem*                              GetCopiedFrom                       ()                    { return m_copiedFrom; }
  bool                                  IsCopiedFrom                        ()                    { return m_copiedFrom? true : false; }

  const std::string&                    GetNameOriginal                     ();
  std::string                           GetDisplayNameCalculated            (bool bDataCheck = false);
  std::string                           GetHeaderTypeNameCalculated         ();
  std::string                           GetDescriptionCalculated            (bool bDataCheck = false);
  std::string                           GetParentRegisterNameHierarchical   ();

  int32_t                               GetBitWidth                         ()                    { return m_bitWidth;  }
  bool                                  SetBitWidth                         (int32_t bitWidth)    { m_bitWidth = bitWidth; return true; }

  uint64_t                              GetEffectiveResetValue              ();
  uint64_t                              GetEffectiveResetMask               ();
  SvdTypes::Access                      GetEffectiveAccess                  ();
  SvdTypes::ModifiedWriteValue          GetEffectiveModifiedWriteValue      ();
  SvdTypes::ReadAction                  GetEffectiveReadAction              ();
  uint32_t                              GetEffectiveBitWidth                ();
  SvdTypes::ProtectionType              GetEffectiveProtection              ();

  bool                                  SetDimElementIndex                  (uint32_t index)      { m_dimElementIndex = index; return true; }
  uint32_t                              GetDimElementIndex                  ()                    { return m_dimElementIndex; }

  void                                  SetUsedForExpression                (bool bUsed = true)   { m_bUsedForCExpression = bUsed; }
  bool                                  IsUsedForCExpression                ()                    { return m_bUsedForCExpression; }

protected:

private:
  static const std::string  m_svdLevelStr[];

  SvdItem*                  m_parent;
  SvdItem*                  m_copiedFrom;
  SvdDerivedFrom*           m_derivedFrom;
  SvdDimension*             m_dimension;
  SVD_LEVEL                 m_svdLevel;
  int32_t                   m_bitWidth;
  uint32_t                  m_dimElementIndex;
  bool                      m_modified;
  bool                      m_bUsedForCExpression;
  SvdTypes::ProtectionType  m_protection;
  std::list<SvdItem*>       m_children;
  std::string               m_displayName;
  std::string               m_description;

  std::map<std::string, std::string> m_attributes;
};


template <typename T>
bool SvdItem::GetDeriveItem(T *&item, const std::list<std::string>& searchName, SVD_LEVEL svdLevel, std::string& lastSearchName)
{
  if(searchName.empty()) {
    return false;
  }

  SvdItem *parent = GetParent();
  if(!parent) {
    return false;
  }

  T *child = 0;
  const std::string &name = *searchName.begin();
  lastSearchName = name;

  SVD_LEVEL l = parent->GetSvdLevel();
  if(l != L_Peripherals) {
    parent = parent->GetParent();
    if(!parent) parent = GetParent();
  }

  bool found = parent->FindChild(child, name);

  // Search all Peripherals
  if(!found) {
    do {
      SVD_LEVEL lv = parent->GetSvdLevel();
      if(lv == L_Peripherals) {
        break;
      }
      parent = parent->GetParent();
    } while(parent);
  }

  if(parent && parent->FindChild(child, name)) {
    found = false;
    // go down the levels
    for(const auto& searchN : searchName) {
      const auto& childs = parent->GetChildren();
      lastSearchName = searchN;

      // skip levels
      SVD_LEVEL lv = parent->GetSvdLevel();
      switch(lv) {
        case L_Peripheral:
        case L_Register:
          parent = *(childs.begin());
          break;
        default:
          break;
      }

      if(parent && parent->FindChild(parent->GetChildren(), child, searchN)) {
        parent = child;
        found = true;
      }
      else {
        found = false;
        break;
      }
    }

    if(found && (child->GetSvdLevel() == svdLevel || svdLevel  == L_UNDEF)) {
      item = child;
      return true;
    }
  }

  return false;
}


class SvdVisitor
{
public:
  virtual ~SvdVisitor(){};
  virtual VISIT_RESULT Visit(SvdItem* item) = 0; // returns if to continue, process children or abort
};

#endif // SvdItem_H
