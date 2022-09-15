/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdDimension_H
#define SvdDimension_H

#include "SvdItem.h"
#include "SvdTypes.h"
#include <string>
#include <set>

struct ExprText {
  int32_t pos;
  std::string text;

  ExprText() : pos(0) {}
};


class SvdExpression {
public:
  SvdExpression();
  ~SvdExpression();

  bool                    CopyItem                (SvdExpression *from);

  SvdTypes::Expression    GetType                 ()                              { return m_type;                            }
  bool                    SetType                 (SvdTypes::Expression type)     { m_type              = type;  return true; }

  const std::string&      GetName                 ()                              { return m_name.text;                       }
  const std::string&      GetDisplayName          ()                              { return m_displayName.text;                }
  const std::string&      GetDescription          ()                              { return m_description.text;                }

  int32_t                     GetNameInsertPos        ()                              { return m_name.pos;                        }
  int32_t                     GetDisplayNameInsertPos ()                              { return m_displayName.pos;                 }
  int32_t                     GetDescriptionInsertPos ()                              { return m_description.pos;                 }

  bool                    SetName                 (const std::string &name)       { m_name.text         = name;  return true; }
  bool                    SetDisplayName          (const std::string &name)       { m_displayName.text  = name;  return true; }
  bool                    SetDescription          (const std::string &descr)      { m_description.text  = descr; return true; }

  bool                    SetNameInsertPos        (int32_t pos)                       { m_name.pos          = pos;   return true; }
  bool                    SetDisplayNameInsertPos (int32_t pos)                       { m_displayName.pos   = pos;   return true; }
  bool                    SetDescriptionInsertPos (int32_t pos)                       { m_description.pos   = pos;   return true; }

private:
  SvdTypes::Expression  m_type;

  ExprText              m_name;
  ExprText              m_displayName;
  ExprText              m_description;
};



class SvdDimension : public SvdItem
{
public:
  SvdDimension(SvdItem* parent);
  virtual ~SvdDimension();

  virtual bool                    Construct(XMLTreeElement* xmlElement);

  virtual bool                    CopyItem(SvdItem *from);
  virtual bool                    CheckItem();

  bool                            CalculateDimIndex();
  bool                            CalculateDimIndexFromTo();
  virtual bool                    CalculateDim();
  bool                            CalculateNameFromExpression();
  bool                            CalculateDisplayNameFromExpression();
  bool                            CalculateDescriptionFromExpression();
  std::string                     CreateName(const std::string &insert);
  std::string                     CreateDisplayName(const std::string &insert);
  std::string                     CreateDescription(const std::string &insert);
  bool                            AddToMap  (const std::string &dimIndex);

  SvdExpression*                  GetExpression         ()  { return  &m_expression;    }
  uint32_t                        GetDim                ()  { return  m_dim;           }
  uint32_t                        GetDimIncrement       ()  { return  m_dimIncrement;  }
  std::string&                    GetFrom               ()  { return  m_from;          }
  std::string&                    GetTo                 ()  { return  m_to;            }
  const std::string&              GetDimIndex           ()  { return  m_dimIndex;      }
  const std::list<std::string>&   GetDimIndexList       ()  { return  m_dimIndexList;  }
  const std::string&              GetDimName            ()  { return  m_dimName;       }
  bool                            ClearDimIndexList     ()  { m_dimIndexList.clear(); return true; }

  bool                            SetDim                (uint32_t                       dim         )  { m_dim          = dim         ; return true; }
  bool                            SetDimIncrement       (uint32_t                       dimIncrement)  { m_dimIncrement = dimIncrement; return true; }
  bool                            SetFrom               (std::string                    from        )  { m_from         = from        ; return true; }
  bool                            SetTo                 (std::string                    to          )  { m_to           = to          ; return true; }
  bool                            SetDimIndex           (const std::string&             dimIndex    )  { m_dimIndex     = dimIndex    ; return true; }
  bool                            SetDimIndexList       (const std::list<std::string>&  dimIndexList)  { m_dimIndexList = dimIndexList; return true; }
  bool                            SetDimName            (const std::string&             dimName     )  { m_dimName      = dimName     ; return true; }

  int32_t                             GetAddressBitsUnits   ();
  int32_t                             CalcAddressIncrement  ();
  bool                            IsTagAllowed          (const std::string& tag);
  bool                            InitAllowedTags       ();
    
protected:

private:
  static std::map<SVD_LEVEL, std::list<std::string> > m_allowedTagsDim;

  uint32_t                m_dim;
  uint32_t                m_dimIncrement;
  uint32_t                m_addressBitsUnitsCache;
  SvdExpression           m_expression;
  std::string             m_from;
  std::string             m_to;
  std::string             m_dimIndex;
  std::list<std::string>  m_dimIndexList;
  std::string             m_dimName;
  std::set<std::string>   m_dimIndexSet;
};

#endif // SvdDimension_H
