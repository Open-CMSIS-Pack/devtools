/******************************************************************************/
/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "XMLTreeSlimString.h"
#include "XmlTreeSlimInterface.h"

#include "ErrLog.h"

XMLTreeSlimString::XMLTreeSlimString(IXmlItemBuilder* itemBuilder, bool bRedirectErrLog, bool bIgnoreAttributePrefixes) :
  XMLTreeSlim(itemBuilder, bRedirectErrLog, bIgnoreAttributePrefixes)
{
  m_p = new XMLTreeSlimInterface(this, bRedirectErrLog, bIgnoreAttributePrefixes);
}

// End of XMLTreeSlimString.cpp
