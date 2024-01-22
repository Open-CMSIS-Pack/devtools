/******************************************************************************/
/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "YmlTree.h"
#include "YmlTreeParserInterface.h"


YmlTree::YmlTree(IXmlItemBuilder* itemBuilder) :
  XMLTree(itemBuilder),
  m_pYmlInterface(nullptr)
{
}

XMLTreeParserInterface* YmlTree::CreateParserInterface()
{
  if(!m_pYmlInterface) {
    m_pYmlInterface = new YmlTreeParserInterface(this);
  }
  return m_pYmlInterface;
}

// End of YmlTree.cpp
