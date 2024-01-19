/******************************************************************************/
/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "XMLTreeSlim.h"
#include "XmlTreeSlimInterface.h"

#include "XML_InputSourceReaderFile.h"
#include "ErrLog.h"

XMLTreeSlim::XMLTreeSlim(IXmlItemBuilder* itemBuilder, bool bRedirectErrLog, bool bIgnoreAttributePrefixes) :
  XMLTree(itemBuilder),
  m_bRedirectErrLog(bRedirectErrLog),
  m_bIgnoreAttributePrefixes(bIgnoreAttributePrefixes)
{
  //immediately create the parser interface
  m_p = XMLTreeSlim::CreateParserInterface();
}

XMLTreeParserInterface* XMLTreeSlim::CreateParserInterface()
{
  return new XMLTreeSlimInterface(this, m_bRedirectErrLog, m_bIgnoreAttributePrefixes,
    new XML_InputSourceReaderFile());
}

// End of XMLTreeSlim.cpp
