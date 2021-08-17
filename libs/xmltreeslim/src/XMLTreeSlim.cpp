/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 * 
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "XMLTreeSlim.h"
#include "XmlTreeSlimInterface.h"

#include "XML_InputSourceReaderFile.h"
#include "ErrLog.h"

XMLTreeSlim::XMLTreeSlim(IXmlItemBuilder* itemBuilder, bool bRedirectErrLog, bool bIgnoreAttributePrefixes) :
  XMLTree(itemBuilder)
{
  m_p = new XMLTreeSlimInterface(this, bRedirectErrLog, bIgnoreAttributePrefixes,
    new XML_InputSourceReaderFile());
}

// End of XMLTreeSlim.cpp
