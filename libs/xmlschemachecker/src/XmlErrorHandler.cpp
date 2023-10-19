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
  message("M511", exc);
}

void XmlErrorHandler::warning(const SAXParseException &exc)
{
  message("M510", exc);
}

void XmlErrorHandler::fatalError(const SAXParseException &exc)
{
  message("M511", exc);
}

void XmlErrorHandler::resetErrors()
{
  //does nothing
}

void XmlErrorHandler::message(const std::string& msgId, const XERCES_CPP_NAMESPACE::SAXParseException& exc)
{
  char* file = xercesc::XMLString::transcode(exc.getSystemId());
  char* msg = xercesc::XMLString::transcode(exc.getMessage());
  ErrLog::Get()->SetFileName(file);
  LogMsg(msgId, MSG(msg), exc.getLineNumber());
  ErrLog::Get()->SetFileName("");
  xercesc::XMLString::release(&file);
  xercesc::XMLString::release(&msg);
}

// end of XmlErrorHandler.cpp
