/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdInterrupt_H
#define SvdInterrupt_H

#include <map>
#include <string>


class SvdInterrupt : public SvdItem
{
public:
  SvdInterrupt(SvdItem* parent);
  virtual ~SvdInterrupt();

  virtual bool Construct(XMLTreeElement* xmlElement);
  virtual bool ProcessXmlElement(XMLTreeElement* xmlElement);
  virtual bool ProcessXmlAttributes(XMLTreeElement* xmlElement);

  virtual bool  CopyItem(SvdItem *from) { return false; }
  virtual bool  CalculateDim();
  virtual bool  CheckItem();

  uint32_t  GetValue ()                { return m_value; }
  bool      SetValue (uint32_t value)  { m_value = value; return true; }

protected:

private:
  uint32_t   m_value;
};

#endif // SvdInterrupt_H
