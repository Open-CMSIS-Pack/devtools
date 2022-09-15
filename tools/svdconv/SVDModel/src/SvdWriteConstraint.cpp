/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdItem.h"
#include "SvdWriteConstraint.h"
#include "XMLTree.h"

using namespace std;


SvdWriteConstraint::SvdWriteConstraint(SvdItem* parent):
  SvdItem(parent)
{
}

SvdWriteConstraint::~SvdWriteConstraint()
{
}

bool SvdWriteConstraint::Construct(XMLTreeElement* xmlElement)
{
  return SvdItem::Construct(xmlElement);
}

bool SvdWriteConstraint::ProcessXmlElement(XMLTreeElement* xmlElement)
{
	const auto& tag = xmlElement->GetTag();

  if(tag == "writeAsRead") {
    return true;
	} 
  else if(tag == "useEnumeratedValues") {
    return true;
	}
  else if(tag == "range") {
    return true;
	}

  return SvdItem::ProcessXmlElement(xmlElement);
}

bool SvdWriteConstraint::ProcessXmlAttributes(XMLTreeElement* xmlElement)
{
  return SvdItem::ProcessXmlAttributes(xmlElement);
}

bool SvdWriteConstraint::CopyItem(SvdItem *from)
{
  return SvdItem::CopyItem(from);
}
