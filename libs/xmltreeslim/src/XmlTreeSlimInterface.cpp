/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "XMLTreeSlim.h"
#include "XmlTreeSlimInterface.h"

#include "ErrLog.h"

#include <sstream>

using namespace std;
using namespace XmlTypes;


const MsgTable XMLTreeSlimInterface::msgTable = {
  { "M421", { MsgLevel::LEVEL_ERROR,    CRLF_B,  "XML Hierarchy Error: Missing End Tags."                                      }  },
  { "M422", { MsgLevel::LEVEL_ERROR,    CRLF_B,  "Error reading file '%NAME%'"                                                 }  },
};

const MsgTableStrict XMLTreeSlimInterface::msgStrictTable = {
//{ "M421",   MsgLevel::LEVEL_ERROR },
};


class XMLTreeSlimErrorConsumer : public IErrConsumer
{
public:
  XMLTreeSlimErrorConsumer(XMLTreeSlimInterface* xmlTreeIf) : m_treeIf(xmlTreeIf) { }
  // consumes (outputs or ignores message), returns true if consumed and should not be output in the regular way
  bool Consume(const PdscMsg& msg, const std::string& fileName) override {
    MsgLevel msgLevel = msg.GetMsgLevel();
    if (msgLevel <= MsgLevel::LEVEL_INFO || msgLevel == MsgLevel::LEVEL_TEXT)
      return false; // let the original process it
    bool bWarning = msgLevel < MsgLevel::LEVEL_ERROR;

    ostringstream ss;
    if (!fileName.empty()) {
      ss << fileName;
      int lineNo = msg.GetLineNo();
      if (lineNo != -1) {
        ss << "(" << lineNo << ")";
      }
      ss << ": ";
    }
    ss << (bWarning ? "warning" : "error") << " " << msg.GetMsgNum() << " : " << msg.PDSC_FormatMessage();

    m_treeIf->Error(ss.str(), bWarning);
    return true; // consumed
  }

private:
  XMLTreeParserInterface *m_treeIf;

};


XMLTreeSlimInterface::XMLTreeSlimInterface(XMLTree* tree, bool bRedirectErrLog,
  bool bIgnoreAttributePrefixes, XML_InputSourceReader* inputSourceReader) :
  XMLTreeParserInterface(tree),
  m_pXmlReader(nullptr),
  m_bIgnoreAttributePrefixes(bIgnoreAttributePrefixes),
  recursion(0),
  m_errConsumer(0)
{
  InitMessageTable();
  if (bRedirectErrLog) {
    m_errConsumer = new XMLTreeSlimErrorConsumer(this);
  }
  m_pXmlReader = new XML_Reader(inputSourceReader);
}

XMLTreeSlimInterface::~XMLTreeSlimInterface()
{
  if (m_errConsumer) {
    delete m_errConsumer;
    m_errConsumer = nullptr;
  }
  delete m_pXmlReader;
  m_pXmlReader = nullptr;
}

bool XMLTreeSlimInterface::Init()
{
  // does nothing for underlying XML_Parser
  return true;
}

void XMLTreeSlimInterface::InitMessageTable()
{
  PdscMsg::AddMessages(msgTable);
  PdscMsg::AddMessagesStrict(msgStrictTable);
}

void XMLTreeSlimInterface::Clear() // does not destroy XML parser!
{
 // does nothing for underlying XML_Parser
}

bool XMLTreeSlimInterface::Parse(const std::string& fileName, const std::string& xmlString)
{
  XmlTypes::XmlNode_t node;
  bool ret = false;
  m_errorStrings.clear();
  m_nErrors = 0;
  m_nWarnings = 0;

  ErrLog::Get()->SetFileName(fileName);
  IErrConsumer* prevConsumer = ErrLog::Get()->GetErrConsumer();
  if (m_errConsumer) {
    ErrLog::Get()->SetErrConsumer(m_errConsumer);
  }

  m_xmlFile = fileName;

  XmlTypes::Err err = m_pXmlReader->Init(fileName, xmlString);
  if (err != XmlTypes::Err::ERR_NOERR) {
    LogMsg("M422", VAL("NAME", fileName));
  } else {
    ret = m_pXmlReader->GetNextNode(node);
    ret = ParseElement(node);

    if (!ret) {
      LogMsg("M422", NAME(fileName));
    }

    if (recursion) {
      LogMsg("M421");
      m_pXmlReader->PrintTagStack();
    }
  }

  m_pXmlReader->UnInit();

  ErrLog::Get()->SetErrConsumer(prevConsumer);
  ErrLog::Get()->SetFileName("");

  m_xmlFile = "";
  return ret;
}


void XMLTreeSlimInterface::ReadAttributes(const string& tag)
{
  int status = 0;
  string attr, value;

  IXmlItemBuilder* builder = m_tree->GetXmlItemBuilder();

  if (m_pXmlReader->HasAttributes()) {
    do {
      status =m_pXmlReader->ReadNextAttribute(m_bIgnoreAttributePrefixes);
      if (status) {
        attr =m_pXmlReader->GetAttributeTag();
        value =m_pXmlReader->GetAttributeData();

        if (!attr.empty() && !value.empty()) {
          value = AdjustAttributeValue(tag, attr, value,m_pXmlReader->GetLineNumber());
          builder->AddAttribute(attr, value);
        }
      }
    } while (status);
  }
}

bool XMLTreeSlimInterface::ParseElement(XmlTypes::XmlNode_t& elementNode)
{
  IXmlItemBuilder* builder = m_tree->GetXmlItemBuilder();
  builder->PreCreateItem();
  bool success = DoParseElement(elementNode);
  builder->PostCreateItem(success);
  return success;
}


bool XMLTreeSlimInterface::DoParseElement(XmlTypes::XmlNode_t& elementNode)
{
  XmlTypes::XmlNode_t node;

  recursion++;

  IXmlItemBuilder* builder = m_tree->GetXmlItemBuilder();

  const string& tag = elementNode.tag;
  builder->CreateItem(tag);
  builder->SetLineNumber(elementNode.lineNo);

  if (elementNode.bHasAttributes)
    ReadAttributes(tag);
  builder->AddItem(); // add item to parent's child collection here, when all attributes are set
  if (elementNode.type == TagType::TAG_SINGLE) {
    recursion--;
    return true;
  }

  do {
    m_pXmlReader->GetNextNode(node);
    if (node.bEndOfFile)
      return false;

    switch (node.type) {
    case TagType::TAG_BEGIN:
    case TagType::TAG_SINGLE:
      if (!ParseElement(node)) {
        return false;
      }
    break;

    case TagType::TAG_TEXT: {
      const string& text = node.data;
      string text2 = AdjustAttributeValue(tag, XMLTree::EMPTY_STRING, text, m_pXmlReader->GetLineNumber());
      builder->SetText(text2);
    } break;

    case TagType::TAG_END: {
      recursion--;
      return true;
    } break;
    default:
      break;
    }
  } while (!node.bEndOfFile);

  recursion--;

  return true;
}
