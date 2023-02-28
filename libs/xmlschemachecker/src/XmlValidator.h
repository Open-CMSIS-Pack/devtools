/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef XMLVALIDATOR_H
#define XMLVALIDATOR_H

#include "XmlErrorHandler.h"
#include "xercesc/parsers/XercesDOMParser.hpp"

class XmlValidator
{
public:
    XmlValidator();
    ~XmlValidator();

    bool Validate(const std::string& xmlFile, const std::string& schemaFile);

private:
    xercesc::XercesDOMParser* m_domParser;
    XmlErrorHandler* m_errorHandler;
};

#endif //XMLVALIDATOR_H
