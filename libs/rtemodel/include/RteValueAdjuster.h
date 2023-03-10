#ifndef RteValueAdjuster_H
#define RteValueAdjuster_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteValueAdjuster.h
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "XMLTree.h"

/**
 * @brief calls extending XmlValueAdjuster class to process string adjustments when reading *.pdsc and *.cprj files
*/
class RteValueAdjuster : public XmlValueAdjuster
{
public:
  /**
   * @brief default constructor
   * @param bConvertPaths flag telling if to convert paths to OS format
  */
  RteValueAdjuster(bool bConvertPaths = true);

  /**
   * @brief check if convert paths to OS format, usually slashes to backslashes
   * @return true if path conversion is required (default true)
  */
  bool IsConvertPathsToOS() const { return m_bConvertPaths; }
  /**
   * @brief set flag to convert paths to OS format
   * @param bConvertPaths flag telling if to convert paths to OS format
  */
  void SetConvertPathsToOS(bool bConvertPaths) { m_bConvertPaths = bConvertPaths; }

  /**
   * @brief check if element value or attribute value represents a path
   * @param tag element tag
   * @param name attribute name, can be empty
   * @return true if value represents path
  */
   bool IsPath(const std::string& tag, const std::string& name) const override;

  /**
    * @brief adjust attribute value
    * @param tag name of tag
    * @param name attribute name
    * @param value attribute value
    * @param lineNumber line number in the XML file
    * @return adjusted attribute value.
   */
   std::string AdjustAttributeValue(const std::string& tag, const std::string& name, const std::string& value, int lineNumber) override;

protected:
  bool m_bConvertPaths; //flag telling if to convert paths to OS format
};
#endif // RteValueAdjuster_H
