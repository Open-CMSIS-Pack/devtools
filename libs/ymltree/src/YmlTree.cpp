/******************************************************************************/
/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "YmlTree.h"
#include "YmlTreeParserInterface.h"


YmlTree::YmlTree(IXmlItemBuilder* itemBuilder) :
  XMLTree(itemBuilder)
{
  m_p = new YmlTreeParserInterface(this);
}

// End of YmlTree.cpp
