#ifndef XMLTREESLIMINTERFACE_H
#define XMLTREESLIMINTERFACE_H
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 /******************************************************************************/

#include "XMLTree.h"
#include "XML_Reader.h"

class IErrConsumer;
class XML_Reader;
class XML_InputSourceReader;
struct XmlNode_t;

/**
 * @brief internal implementation of XML reader that uses xerces to parse XML file and process errors
*/
class XMLTreeSlimInterface : public XMLTreeParserInterface
{
public:
  /**
   * @brief constructor
   * @param tree pointer to an instance of XMLTree
   * @param bRedirectErrLog true to direct errors
   * @param bIgnoreAttributePrefixes true to ignore attribute prefixes
   * @param inputSourceReader pointer to an instance of XML_InputSourceReader
  */
  XMLTreeSlimInterface(XMLTree* tree, bool bRedirectErrLog = false,
    bool bIgnoreAttributePrefixes = true, XML_InputSourceReader* inputSourceReader = nullptr);

  /**
  * constructor
  */
  ~XMLTreeSlimInterface() override;

  /**
   * @brief initialize the instance. The default implementation does nothing
   * @return true if initialization is done
  */
  bool Init() override;

  /**
   * @brief clean up instance but do not destroy XML parser
  */
  void Clear() override;

  /**
   * @brief parse XML file or buffer containing XML formatted text
   * @param fileName name of XML file
   * @param xmlString buffer containing XML formatted text
   * @return true if parsing was successful
  */
  bool Parse(const std::string& fileName, const std::string& xmlString) override;

private:
  bool ParseElement(XmlTypes::XmlNode_t &node);
  bool DoParseElement(XmlTypes::XmlNode_t &node);
  void ReadAttributes(const std::string& tag);

  void InitMessageTable();
  void InitMessageTableStrict();

  XML_Reader* m_pXmlReader;
  bool m_bIgnoreAttributePrefixes;
  int recursion;

  IErrConsumer* m_errConsumer;

  static const MsgTable msgTable;
  static const MsgTableStrict msgStrictTable;
};

#endif //XMLTREESLIMINTERFACE_H
