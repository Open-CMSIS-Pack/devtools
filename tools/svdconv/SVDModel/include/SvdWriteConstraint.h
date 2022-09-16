/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SvdWriteConstraint_H
#define SvdWriteConstraint_H


class SvdWriteConstraint : public SvdItem
{
public:
  SvdWriteConstraint(SvdItem* parent);
  virtual ~SvdWriteConstraint();

  bool Construct(XMLTreeElement* xmlElement);
  bool ProcessXmlElement(XMLTreeElement* xmlElement);
  bool ProcessXmlAttributes(XMLTreeElement* xmlElement);

  virtual bool CopyItem(SvdItem *from);

protected:

private:

};

#endif // SvdWriteConstraint_H