/******************************************************************************/
/*
 * Copyright (c) 2020-2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "XMLTree.h"

#include "XmlTreeItemBuilder.h"

using namespace std;

XMLTreeElement::XMLTreeElement(XMLTreeElement* parent) :
  XmlTreeItem<XMLTreeElement>(parent)
{
  if (parent)
    parent->AddChild(this);
}

XMLTreeElement::XMLTreeElement(XMLTreeElement* parent, const string& tag) :
  XmlTreeItem<XMLTreeElement>(parent, tag)
{
}


XMLTreeElement::~XMLTreeElement()
{
  XMLTreeElement::Clear();
}

class XMLTreeElementBuilder : public XmlTreeItemBuilder<XMLTreeElement>
{
public:
  /**
   * @brief constructor
   * @param xmlTree pointer to XMLTree that calls this builder
  */
  XMLTreeElementBuilder(XMLTree* xmlTree) :XmlTreeItemBuilder<XMLTreeElement>(), m_xmlTree(xmlTree) {};

  /**
   * @brief pure virtual function to create an XMLTreeElement specified by tag
   * @param tag name of new tag
   * @return pointer to created XMLTreeElement
  */
  XMLTreeElement* CreateRootItem(const string& tag) override
  {
    XMLTreeDoc* pDoc = new XMLTreeDoc(m_xmlTree, GetFileName());
    pDoc->SetTag(tag);
    return pDoc;
  }

private:
  XMLTree* m_xmlTree;
};



XMLTree::XMLTree(IXmlItemBuilder* itemBuilder) :
  XMLTreeElement(NULL),
  m_nErrors(0),
  m_nWarnings(0),
  m_callback(0),
  m_XmlItemBuilder(itemBuilder),
  m_bInternalBuilder(false),
  m_XmlValueAdjuster(nullptr),
  m_schemaChecker(nullptr),
  m_p(0)
{
  if (!m_XmlItemBuilder) {
    m_XmlItemBuilder = new XMLTreeElementBuilder(this);
    m_bInternalBuilder = true;
  }
}

XMLTree::~XMLTree()
{
  delete m_p;
  if (m_bInternalBuilder) {
    delete m_XmlItemBuilder;
  }
}

void XMLTree::Clear() // does not destroy XML parser!
{
  m_p->Clear();
  m_nErrors = 0;
  m_nWarnings = 0;
  m_errorStrings.clear();
  m_fileNames.clear();
  XMLTreeElement::Clear();
}

void XMLTree::SetXmlItemBuilder(IXmlItemBuilder* itemBuilder, bool takeOnerschip )
{
  if (m_XmlItemBuilder != itemBuilder) {
    if (m_bInternalBuilder) {
      delete m_XmlItemBuilder;
    }
    m_XmlItemBuilder = itemBuilder;
  }
  m_bInternalBuilder = m_XmlItemBuilder ? takeOnerschip : false;
}


void XMLTree::SetSchemaFileName(const string& schemaFile)
{
  if (!schemaFile.empty()) {
    m_schemaFile = XmlValueAdjuster::SlashesToOsSlashes(schemaFile);
  } else {
    m_schemaFile.clear();
  }
}

void XMLTree::SetIgnoreTags(const set<string>& ignoreTags)
{
  m_p->SetIgnoreTags(ignoreTags);
}

bool XMLTree::Init()
{
  if(!m_p) {
    m_p = CreateParserInterface();
  }
  if(m_p) {
    return m_p->Init();
  }
  return false;
}


bool XMLTree::AddFileName(const string& fileName, bool bParse)
{
  if (fileName.empty())
    return true;
  for (auto f : m_fileNames) {
    if (f == fileName)
      return true;
  }
  m_fileNames.push_back(fileName);
  if (bParse)
    return DoParse(fileName, EMPTY_STRING);
  return true;
}

bool XMLTree::SetFileNames(const list<string>& fileNames, bool bParse)
{
  m_fileNames = fileNames;
  if (bParse)
    return ParseAll();
  return true;
}


const string& XMLTree::GetCurrentFile() const
{
  if (m_p)
    return m_p->GetCurrentFile();
  return EMPTY_STRING;
}

bool XMLTree::ParseAll()
{
  bool success = true;
  m_p->Clear();
  // parse all documents
  for (auto f : m_fileNames) {
    bool ok = DoParse(f, EMPTY_STRING);
    if (!ok)
      success = false;
    if (m_callback) {
      if (m_callback->FileParsed(f, ok) == false)
        break;
    }
  }
  return success;
}

bool XMLTree::ParseFile(const string& fileName)
{
  if(m_schemaChecker ) {
    const string& schema = m_schemaFile.empty() ? m_schemaChecker->FindSchema(fileName) : m_schemaFile;
    if(!schema.empty()) {
      m_schemaChecker->ClearErrors();
      if(!m_schemaChecker->ValidateFile(fileName, m_schemaFile)) {
        for(auto& err : m_schemaChecker->GetErrors()) {
          if(err.m_severity == RteError::SevERROR) {
            m_nErrors++;
          } else if(err.m_severity == RteError::SevWARNING) {
            m_nWarnings++;
          }
          m_errorStrings.push_back(err.ToString());
        }
        return false;
      }
    }
  }
  return Parse(fileName, EMPTY_STRING);
}


bool XMLTree::ParseString(const std::string& xmlString) {
  return Parse(EMPTY_STRING, xmlString);
}

bool XMLTree::Parse(const std::string& fileName, const std::string& xmlString)
{
  if(!Init()) {
    return false;
  }
  m_p->Clear();
  return DoParse(fileName, xmlString);
}


bool XMLTree::DoParse(const string& fileName, const std::string& xmlString)
{
  if (fileName.empty() && xmlString.empty()) {
    return false; // nothing to parse
  }

  m_XmlItemBuilder->Clear();
  m_XmlItemBuilder->SetFileName(fileName);
  bool success = m_p->Parse(fileName, xmlString);

  if (success && m_p->HasErrors()) {
    success = false;
  }

  m_nErrors += m_p->GetErrors();
  m_nWarnings += m_p->GetWarnings();

  const list<string>& errorStrings = m_p->GetErrorStrings();
  if (!errorStrings.empty()) {
    m_errorStrings.insert(m_errorStrings.end(), errorStrings.begin(), errorStrings.end());
  }

  return success;
}

bool XMLTree::ParseFromDOMNode(void* pDOMnode)
{
  m_XmlItemBuilder->Clear();
  return m_p->ParseFromDOMNode(pDOMnode);
}

void XMLTree::CreateDOM(void* pDOMDocument, void* pDOMparentElement)
{
  m_p->CreateDOM(pDOMDocument, pDOMparentElement);
}

bool XMLTree::PrintDOM(const string& fileName)
{
  return m_p->PrintDOM(fileName);
}


string XMLTree::AdjustAttributeValue(const string& tag, const string& name, const string& value, int lineNumber) const
{
  if (m_XmlValueAdjuster) {
    return m_XmlValueAdjuster->AdjustAttributeValue(tag, name, value, lineNumber);
  }
  return value;
}

bool XMLTree::IsPath(const string& tag, const string& name) const
{
  if (m_XmlValueAdjuster) {
    return m_XmlValueAdjuster->IsPath(tag, name);
  }
  return false;
}


XMLTreeParserInterface::XMLTreeParserInterface(XMLTree* tree) :
  m_tree(tree),
  m_nErrors(0),
  m_nWarnings(0)
{

}

XMLTreeParserInterface::~XMLTreeParserInterface()
{
  XMLTreeParserInterface::Clear();
}

bool XMLTreeParserInterface::IsTagIgnored(const string& tag) const
{
  return m_IgnoreTags.find(tag) != m_IgnoreTags.end();
}

void XMLTreeParserInterface::Clear()
{
  m_xmlFile.clear();
  m_errorStrings.clear();
  m_nErrors = 0;
  m_nWarnings = 0;
}


bool XMLTreeParserInterface::Parse(const string& fileName, const std::string& xmlString)
{
  // default is not implemented
  Error("Parsing XML is not implemented", false);
  return false;
}

void XMLTreeParserInterface::CreateDOM(void* pDoc, void *parent)
{
  // default does nothing
}

bool XMLTreeParserInterface::PrintDOM(const string& fileName)
{
  Error("Printing DOM is not implemented", false);
  return false; // default does not suppoprt operation
}

bool XMLTreeParserInterface::ParseFromDOMNode(void* node)
{
  Error("Parsing from DOM node is not implemented", false);
  return false; // default is not implemented
}


void XMLTreeParserInterface::Error(const string& msg, bool warning)
{
  if (warning)
    m_nWarnings++;
  else
    m_nErrors++;
  m_errorStrings.push_back(msg);
}

string XMLTreeParserInterface::AdjustAttributeValue(const string& tag, const string& name, const string& value, int lineNumber) const
{
  if (m_tree) {
    return m_tree->AdjustAttributeValue(tag, name, value, lineNumber);
  }
  return value;
}

bool XMLTreeParserInterface::IsPath(const string& tag, const string& name) const
{
  if (m_tree) {
    return m_tree->IsPath(tag, name);
  }
  return false;
}


string XmlValueAdjuster::SlashesToOsSlashes(const string& fileName)
{
#ifdef _WIN32
  return SlashesToBackSlashes(fileName);
#else
  return BackSlashesToSlashes(fileName);
#endif
}

string XmlValueAdjuster::SlashesToBackSlashes(const string& fileName)
{
  string name = fileName;
  string::size_type pos = 0;
  for (pos = name.find('/'); pos != string::npos; pos = name.find('/', pos)) {
    name[pos] = '\\';
  }
  return name;
}

string XmlValueAdjuster::BackSlashesToSlashes(const string& fileName)
{
  string name = fileName;
  string::size_type pos = 0;
  for (pos = name.find('\\'); pos != string::npos; pos = name.find('\\', pos)) {
    name[pos] = '/';
  }
  return name;
}


bool XmlValueAdjuster::IsPathNeedConversion(const string& fileName) {
  if (fileName.empty())
    return false;
  if (fileName.at(0) == '\\') // Windows \\server\share
    return false;
  if (fileName.at(0) == 0) // Unix absolute
    return false;
  if (fileName.find(':') != string::npos)  // an url with file: http:, https: or Windows drive e.g. c:\dir
    return false;
  if (fileName.find("www.") == 0)  // an url without http: or https:
    return false;

  return true;
}

bool XmlValueAdjuster::IsAbsolute(const string& fileName)
{
  if (fileName.empty())
    return false;

  if (fileName.at(0) == '\\') // Windows \\server\share or
    return true;
  if (fileName.at(0) == '/') // Unix absolute
    return true;
  if (fileName.find(':') == 1)  // Windows drive e.g. c:\dir
    return true;
  return false;
}


bool XmlValueAdjuster::IsURL(const string& fileName)
{
  if (fileName.empty())
    return false;

  if (fileName.find("http:") == 0 ||
    fileName.find("https:") == 0 ||
    fileName.find("file:") == 0 ||
    fileName.find("www.") == 0) {
    return true;
  }
  return false;
}


bool XmlValueAdjuster::IsPath(const string& tag, const string& name) const
{
  return false;
}

string XmlValueAdjuster::AdjustAttributeValue(const string& tag, const string& name, const string& value, int lineNumber)
{
  return value;
}

string XmlValueAdjuster::AdjustPath(const string& fileName, int lineNumber)
{
  if (IsPathNeedConversion(fileName))
    return SlashesToOsSlashes(fileName);

  return fileName;
}

// End of XMLTree.cpp
