/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdDerivedFrom_H
#define SvdDerivedFrom_H

#include "SvdItem.h"
#include <string>


class SvdPeripheral;
class SvdDerivedFrom : public SvdItem
{
public:
  SvdDerivedFrom(SvdItem* parent);
  virtual ~SvdDerivedFrom();

  virtual bool    Construct(XMLTreeElement* xmlElement);
  virtual bool    ProcessXmlAttributes(XMLTreeElement* xmlElement);

  std::list<std::string>&  GetSearchName  () { return m_searchName; }
  size_t          GetSearchNameItems      () { return m_searchName.size(); }
  bool            CalculateDerivedFrom    ();
  SvdItem*        GetDerivedFromItem      () { return m_derivedFromItem; }
  void            SetDerivedFromItem      (SvdItem *item) { m_derivedFromItem = item; }
  bool            DeriveItem              (SvdItem *from);
  virtual bool    CopyItem                (SvdItem *from);
  bool            GetCalculated           () { return m_bCalculated; }
  void            SetCalculated           () { m_bCalculated = true; }

protected:

private:
  SvdItem*                m_derivedFromItem;
  bool                    m_bCalculated;
  std::list<std::string>  m_searchName;
};

#endif // SvdDerivedFrom_H
