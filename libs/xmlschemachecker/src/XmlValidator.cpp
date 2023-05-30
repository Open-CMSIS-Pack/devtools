/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>
#include "XmlValidator.h"
#include "XmlErrorHandler.h"

#include "xercesc/parsers/XercesDOMParser.hpp"
#include "xercesc/framework/LocalFileInputSource.hpp"
#include "xercesc/sax/ErrorHandler.hpp"
#include "xercesc/sax/SAXParseException.hpp"

#include <sstream>
#include "ErrLog.h"

using namespace std;
using namespace XERCES_CPP_NAMESPACE;

XmlValidator::XmlValidator()
{
  XMLPlatformUtils::Initialize();

  m_domParser = new XercesDOMParser();
  m_errorHandler = new XmlErrorHandler();
  m_domParser->setErrorHandler(m_errorHandler);
  m_domParser->setValidationScheme(XercesDOMParser::Val_Always);
  m_domParser->setDoNamespaces(true);
  m_domParser->setDoSchema(true);
  m_domParser->setValidationConstraintFatal(false);   // report all errors
  m_domParser->setValidationSchemaFullChecking(true);
}

XmlValidator::~XmlValidator()
{
  delete m_domParser;
  delete m_errorHandler;
  XMLPlatformUtils::Terminate();
}

/**
 * @brief Validate the xml file against the specified schema file
 * @param schemaFile the schema file to validate against
 * @param xmlFile the xml file to validate
 * @return passed / failed
 */
bool XmlValidator::Validate(const string& xmlFile, const string& schemaFile)
{
  LogMsg("M084");

  try {
    m_domParser->setExternalNoNamespaceSchemaLocation(schemaFile.c_str());
    m_domParser->parse(xmlFile.c_str());

    auto errCnt = m_domParser->getErrorCount();

    LogMsg("M016");
    LogMsg("M024", ERR(errCnt));

    if (errCnt == 0) {
      return true;
    } else {
      return false;
    }
  }
  catch (const XMLException& e) {
    stringstream ss;
    ss << "Exception: " << e.getMessage();
    LogMsg("M511", MSG(ss.str()));
    return false;
  }
  catch (const SAXException& e) {
    stringstream ss;
    ss << "Exception: " << e.getMessage() << endl;
    LogMsg("M511", MSG(ss.str()));
    return false;
  }
}
