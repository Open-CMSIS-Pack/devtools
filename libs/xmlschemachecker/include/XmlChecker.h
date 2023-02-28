/*
 * Copyright (c) 2020-2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef XMLCHECKER_H
#define XMLCHECKER_H

#include <string>

class XmlChecker {
public:

    /**
     * @brief Validates the xml file with respect to schema given
     * @param datafile input xml file to be validated
     * @param schemafile input schema file for given xml file
     * @return true if validation pass, otherwise false
    */
    static bool Validate(const std::string& xmlfile, const std::string& schemafile);
};

#endif //XMLCHECKER_H
