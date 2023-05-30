/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>
#include "XmlErrorHandler.h"
#include "xercesc/util/XMLString.hpp"
#include <xercesc/sax/SAXParseException.hpp>
#include "ErrLog.h"

using namespace std;
using namespace XERCES_CPP_NAMESPACE;


void XmlErrorHandler::error(const SAXParseException &exc)
{
  char* file = xercesc::XMLString::transcode(exc.getSystemId());
  char* msg = xercesc::XMLString::transcode(exc.getMessage());
  ErrLog::Get()->SetFileName(file);
  LogMsg("M511", MSG(msg), exc.getLineNumber());
  ErrLog::Get()->SetFileName("");
  xercesc::XMLString::release(&file);
  xercesc::XMLString::release(&msg);
}

void XmlErrorHandler::warning(const SAXParseException &exc)
{
  char* file = xercesc::XMLString::transcode(exc.getSystemId());
  char* msg = xercesc::XMLString::transcode(exc.getMessage());
  ErrLog::Get()->SetFileName(file);
  LogMsg("M510", MSG(msg), exc.getLineNumber());
  ErrLog::Get()->SetFileName("");
  xercesc::XMLString::release(&file);
  xercesc::XMLString::release(&msg);
}

void XmlErrorHandler::fatalError(const SAXParseException &exc)
{
  char* file = xercesc::XMLString::transcode(exc.getSystemId());
  char* msg = xercesc::XMLString::transcode(exc.getMessage());
  ErrLog::Get()->SetFileName(file);
  LogMsg("M511", MSG(msg), exc.getLineNumber());
  ErrLog::Get()->SetFileName("");
  xercesc::XMLString::release(&file);
  xercesc::XMLString::release(&msg);
}

void XmlErrorHandler::resetErrors() {
  cerr.flush();
}
