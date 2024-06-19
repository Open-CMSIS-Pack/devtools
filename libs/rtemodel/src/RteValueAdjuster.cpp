/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteValueAdjuster.cpp
* @brief CMSIS RTE Data model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteValueAdjuster.h"

using namespace std;

RteValueAdjuster::RteValueAdjuster(bool bConvertPaths) :
  m_bConvertPaths(bConvertPaths)
{
}


bool RteValueAdjuster::IsPath(const string& tag, const string& name) const
{
  if (tag == "doc" && name.empty()) {
    return true;
  }
  // convert slashes to backslashes for certain attributes
  if (tag == "file" && (name == "name" || name == "src")) {
    return true;
  } else if ((tag == "book" || tag == "algorithm") && name == "name") {
    return true;
  } else if (name == "svd" && tag == "debug") {
    return true;
  } else if (tag == "compile" && name == "header") {
    return true;
  } else if (tag == "environment" && name == "load") {
    return true;
  } else if (tag == "image" && (name == "large" || name == "small")) {
    return true;
  } else if ((tag == "description" || tag == "example") && (name == "folder" || name == "archive" || name == "doc")) {
    return true;
  }
  return false;
}

string RteValueAdjuster::AdjustAttributeValue(const string& tag, const string& name, const string& value, int lineNumber)
{
  if (value.empty())
    return value;

  if (IsPath(tag, name) ) {
    return IsConvertPathsToOS() ? AdjustPath(value, lineNumber) : value;
  }

  if (name.empty())
    return value;

  // adjust FPI and FPU values
  if (name == "Dfpu") {
    if (value == "1" || value == "FPU")
      return "SP_FPU";
    else if (value == "0")
      return "NO_FPU";
    else
      return value;
  } else if (name == "Dmpu") {
    if (value == "1")
      return "MPU";
    else if (value == "0")
      return "NO_MPU";
    else
      return value;
  } else if (name == "Dtz") {
    if (value == "1")
      return "TZ";
    else if (value == "0")
      return "NO_TZ";
    else
      return value;
  } else if (name == "Ddsp") {
    if (value == "1")
      return "DSP";
    else if (value == "0")
      return "NO_DSP";
    else
      return value;
  } else if (name == "Dsecure") {
    if (value == "0")
      return "Non-secure";
    else if (value == "1")
      return "Secure";
    else if (value == "2")
      return "TZ-disabled";
    else
      return value;
  } else if (name == "Dmve") {
    if (value == "0")
      return "NO_MVE";
    else if (value == "1")
      return "MVE";
    else if ((value == "2") || (value == "MVE_SP_FP"))
      return "FP_MVE";
    else if ((value == "3") || (value == "MVE_DP_FP"))
      return "FP_MVE";
    else
      return value;
  } else if (name == "scope") {
    if (value == "hidden")
      return "private";
    else if (value == "visible")
      return "public";
    else
      return value;
  }

  // convert boolean values for consistency
  if (value == "true")
    return "1";
  else if (value == "false")
    return "0";
  return value;
}

// End of RteValueAdjuster.cpp
