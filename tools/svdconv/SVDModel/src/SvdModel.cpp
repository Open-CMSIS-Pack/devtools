/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdDevice.h"
#include "SvdItem.h"
#include "SvdModel.h"
#include "XMLTree.h"
#include "ErrLog.h"

using namespace std;


SvdModel::SvdModel(SvdItem* parent):
  SvdItem(parent),
  m_device(nullptr),
  m_showMissingEnums(false)
{
  SetSvdLevel(L_Device);
}

SvdModel::~SvdModel()
{
}

bool SvdModel::Construct(XMLTreeElement* xmlTree)
{
  if(!xmlTree) {
		return false;
  }

  bool success = true;
  const auto& docs = xmlTree->GetChildren();

	for(const auto xmlElement : docs) {
    if(!xmlElement || !xmlElement->IsValid()) {
			continue;
    }

    SetLineNumber(xmlElement->GetLineNumber());
    SetColNumber(0); // xmlElement->GetColNumber());
	  SetTag(xmlElement->GetTag());
    SetText(xmlElement->GetText());

    if(GetTag() == "device") {
      m_device = new SvdDevice(this);
      bool ok = m_device->Construct(xmlElement);
		  if(ok) {
  		  AddItem(m_device);
		  }

      if(!ok) {
			  delete m_device;
        success = false;
		  }
    }
	}

  CheckItem();

  return success;
}

bool SvdModel::Validate()
{
  SetValid(true);

  return IsValid();
}

bool SvdModel::CalculateModel()
{
  return true;
}

bool SvdModel::CheckItem()
{
  if(!IsValid()) {
    return true;
  }

  const auto numOfDevices = GetChildCount();
  if(!numOfDevices) {
    LogMsg("M340");
  }
  else if(numOfDevices > 1) {
    LogMsg("M341");
  }

  return true;
}

VISIT_RESULT SvdModelCalculate::Visit(SvdItem* item)
{
  return VISIT_RESULT::CONTINUE_VISIT;
}
