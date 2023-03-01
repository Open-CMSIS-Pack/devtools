/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef XMLERRORHANDLER_H
#define XMLERRORHANDLER_H

#include "xercesc/sax/ErrorHandler.hpp"

class XmlErrorHandler : public XERCES_CPP_NAMESPACE::ErrorHandler {
public:
    XmlErrorHandler() {}
    ~XmlErrorHandler() {}

    void error(const XERCES_CPP_NAMESPACE::SAXParseException& exc);
    void warning(const XERCES_CPP_NAMESPACE::SAXParseException& exc);
    void fatalError(const XERCES_CPP_NAMESPACE::SAXParseException& exc);
    void resetErrors();

};

#endif //XMLERRORHANDLER_H
