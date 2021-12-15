/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRXMLPARSER_H
#define PROJMGRXMLPARSER_H

#include "RteValueAdjuster.h"
#include "XMLTreeSlim.h"

#include <memory>

/**
 * @brief extension to XMLTreeSlim
*/
class ProjMgrXmlParser : public XMLTreeSlim
{
public:
  /**
   * @brief class constructor
  */
  ProjMgrXmlParser();

  /**
   * @brief class destructor
  */
  virtual ~ProjMgrXmlParser();

private:
  std::unique_ptr<XmlValueAdjuster> m_adjuster;
};

#endif /* PROJMGRXMLPARSER_H */
