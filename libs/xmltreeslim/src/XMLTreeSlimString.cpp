/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 * 
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "XMLTreeSlimString.h"
#include "XmlTreeSlimInterface.h"

#include "ErrLog.h"

XMLTreeSlimString::XMLTreeSlimString(IXmlItemBuilder* itemBuilder, bool bRedirectErrLog, bool bIgnoreAttributePrefixes) :
  XMLTree(itemBuilder)
{
  m_p = new XMLTreeSlimInterface(this, bRedirectErrLog, bIgnoreAttributePrefixes);
}

// End of XMLTreeSlimString.cpp
