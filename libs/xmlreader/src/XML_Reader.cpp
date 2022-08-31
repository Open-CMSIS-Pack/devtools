/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstring>
#include <cstdarg>
#include <cstdlib>

#include "XML_Reader.h"

using namespace std;
using namespace XmlTypes;


const string XML_Reader::EMPTYSTR = "";

const map<const string, const char> XML_Reader::m_specialChars = {
  { "amp"  , '&'  },
  { "lt"   , '<'  },
  { "gt"   , '>'  },
  { "apos" , '\'' },
  { "quot" , '\"' },
};

const map<const UTFCode, const string> XML_Reader::m_UTFCodeText = {
  { UTFCode::UTF_NULL,  "No UTF or not detected" },
  { UTFCode::UTF8    ,  "UTF8"                   },
  { UTFCode::UTF16_BE,  "UTF16_BE"               },
  { UTFCode::UTF16_LE,  "UTF16_LE"               },
  { UTFCode::UTF32_BE,  "UTF32_BE"               },
  { UTFCode::UTF32_LE,  "UTF32_LE"               },
  { UTFCode::UTF7    ,  "UTF7"                   },
};


XML_InputSourceReader::XML_InputSourceReader() :
  m_source(nullptr),
  m_size(0L)
{
}

XML_InputSourceReader::~XML_InputSourceReader()
{
  XML_InputSourceReader::Close();
}

void XML_InputSourceReader::Close()
{
  m_source = nullptr;
  m_size = 0L;
}

Err XML_InputSourceReader::Open(InputSource_t* source)
{
  if(!source) {
    return Err::ERR_CRITICAL;
  }

  m_source = source;

  return DoOpen();
}

Err XML_InputSourceReader::DoOpen()
{
  if(!m_source || !m_source->xmlString) {
    return Err::ERR_CRITICAL;
  }

  m_size = strlen(m_source->xmlString);
  if(m_size == 0) {
    return Err::ERR_NO_INPUT_FILE;
  }

  return Err::ERR_NOERR;
}

size_t XML_InputSourceReader::ReadLine(char* buf, size_t maxLen)
{
  if(!buf || !m_source || m_source->seekPos >= m_size) {
    return 0;
  }

  const char* ptr = m_source->xmlString + m_source->seekPos;
  size_t readSize = m_size - m_source->seekPos;
  if(readSize > (maxLen - 1)) {
    readSize = maxLen - 1;
  }

  m_source->seekPos += readSize;
  memcpy(buf, ptr, readSize);
  buf[readSize] = 0;

  return readSize;
}

XML_Reader::XML_Reader(XML_InputSourceReader* inputSourceReader) :
  m_bIsPrevText(false),
  m_bPrevTagIsSingle(false),
  m_bFirstTry(true),
  m_streamBufPos(0),
  m_streamBufLen(0),
  m_streamBufMaxlen(MBYTE(2)),
  m_streamBuf(nullptr),
  m_InputSourceReader(inputSourceReader)
{
  m_streamBuf = new char[m_streamBufMaxlen];

  if(!inputSourceReader) {
    m_InputSourceReader = new XML_InputSourceReader();
  }

  m_SourceStack.clear();
  InitMessageTable();
}

XML_Reader::~XML_Reader()
{
  UnInit();

  delete m_InputSourceReader;
  delete[] m_streamBuf;
}

void XML_Reader::InitMessageTable()
{
  PdscMsg::AddMessages(msgTable);
  PdscMsg::AddMessagesStrict(msgStrictTable);
}

const string& XML_Reader::GetAttributeTag()
{
  Trim(m_xmlData.attrTag);

  if(m_xmlData.attrTag.empty()) {
    return EMPTYSTR;
  }

  return m_xmlData.attrTag;
}

const string& XML_Reader::GetAttributeData()
{
  Trim(m_xmlData.attrData);
  if(m_xmlData.attrData.empty()) {
    return EMPTYSTR;
  }

  return m_xmlData.attrData;
}

bool XML_Reader::HasAttributes()
{
  return GetAttributeLen() > 0;
}

uint32_t XML_Reader::GetLineNumber ()
{
  return m_xmlData.lineNo;
}

size_t XML_Reader::GetAttributeLen()
{
  return m_xmlData.attrLen;
}

InputSource_t* XML_Reader::GetCurrentSource()
{
  if(!m_SourceStack.empty()) {
    return &m_SourceStack.back();
  }

  static InputSource_t nullInputSource = { 0 };

  return &nullInputSource;
}

Err XML_Reader::Init(const string& fileName, const std::string& xmlString)
{
  if(fileName.empty() && xmlString.empty()) {
    return Err::ERR_NO_INPUT_FILE;
  }

  m_SourceStack.clear();

  return NextSource(fileName, xmlString);
}

void XML_Reader::UnInit()
{
  if(m_SourceStack.empty()) {
    return;
  }

  Close();
}

Err XML_Reader::NextSource(const string& fileName, const std::string& xmlString)
{
  Close();

  InputSource_t source;
  source.fileName = fileName;
  source.seekPos = 0;
  source.lineNo = 1;
  source.xmlString = xmlString.c_str();
  m_SourceStack.push_back(source);

  return Open();
}

Err XML_Reader::PrevSource()
{
  Err err = Err::ERR_NOERR;

  Close();
  m_SourceStack.pop_back();

  if(!m_SourceStack.empty()) {
    err = Open();
  } else {
    err = Err::ERR_EOF;
  }

  return err;
}

Err XML_Reader::Open()
{
  if(m_SourceStack.empty()) {
    return Err::ERR_NO_INPUT_FILE;
  }

  InputSource_t* source = GetCurrentSource();
  if(!source) {
    return Err::ERR_NO_INPUT_FILE;
  }

  Err err = m_InputSourceReader->Open(source);
  if (err != Err::ERR_NOERR) {
    return err;
  }

  m_xmlData.fileSize = m_InputSourceReader->GetSize();
  if(!m_xmlData.fileSize) {
    m_xmlData.fileSize = 1L;     // prevent div/0
  }

  // set position
  size_t seekPos  = source->seekPos;
  m_xmlData.lineNo = source->lineNo;
  m_xmlData.prevReadPos = seekPos;
  m_xmlData.readPos = seekPos;

  ReadLine();

  if(m_xmlData.lineNo < 2) {      // "<?xml" must be in the first line, used to detect XML document
    if(!InitDocument()) {
      return Err::ERR_NOXML;
    }
  }

  if(GetCurrentSource()->attrReadPos) {
    size_t len = source->attribute.length() * sizeof(char);
    m_xmlData.attribute = source->attribute;
    m_xmlData.attrReadPos = source->attrReadPos;
    m_xmlData.attrLen = len;
  }

  m_xmlData.type = source->type;
  m_xmlData.tagData = source->tag;

  return Err::ERR_NOERR;
}

Err XML_Reader::Close()
{
  if(m_SourceStack.empty()) {
    return Err::ERR_NO_INPUT_FILE;
  }

  InputSource_t* source = GetCurrentSource();
  if(!source) {
    return Err::ERR_NO_INPUT_FILE;
  }

  source->lineNo = m_xmlData.lineNo;
  source->seekPos = m_xmlData.prevReadPos + m_streamBufPos;
  source->type = m_xmlData.type;
  source->tag = m_xmlData.tagData;

  if (m_xmlData.attrLen) {
    source->attribute = m_xmlData.attribute;
    source->attrReadPos = m_xmlData.attrReadPos;
  } else {
    source->attribute.clear();
    source->attrReadPos = 0;
  }

  m_InputSourceReader->Close();

  return Err::ERR_NOERR;
}

size_t XML_Reader::ReadLine()
{
  if(!m_InputSourceReader->IsValid()) {
    LogMsg("M410", GetLineNumber());
    return 0;
  }

  size_t len = m_InputSourceReader->ReadLine(m_streamBuf, m_streamBufMaxlen);
  if(len == 0) {
    return 0;
  }

  m_xmlData.prevReadPos = m_xmlData.readPos;
  m_xmlData.readPos += len;
  m_streamBufLen = len;
  m_streamBufPos = 0;

  return len;
}

bool XML_Reader::Getc(char& c)
{
  if(m_streamBufPos >= m_streamBufLen) {
    if(ReadLine() == 0) {
      return false;
    }
  }

  if(m_streamBufPos < m_streamBufLen) {
    c = m_streamBuf[m_streamBufPos++];
  }

  if(c == '\t') {
    c = ' ';       // convert TAB to SPACE
  }

  return true;
}

void XML_Reader::CorrectCnt(int32_t corr)
{
  if((m_streamBufPos + corr) < m_streamBufMaxlen) {
    m_streamBufPos += corr;
  }
}

bool XML_Reader::ConvertSpecialChar(const string& specialChar, char& found)
{
  // default value
  found = '&';

  if (specialChar.empty()) {
    return false;
  }

  auto it = m_specialChars.find(specialChar);
  if (it != m_specialChars.end()) {
    found = it->second;
    return true;
  }

  if (specialChar[0] == '#') {
    bool bHex = false;
    size_t idx = 1;

    if (specialChar[1] == 'x') {   // The x must be lowercase in XML documents. (Wikipedia)
      bHex = true;
      idx = 2;
    }

    uint32_t num = 0;
    try {
      num = stoi(specialChar.substr(idx), nullptr, bHex ? 16 : 10);
    }
    catch (const std::exception&) {
      return false;
    }

    if (num < 128) {
      found = (char)num;
      return true;
    }
  }

  return false;
}

UTFCode XML_Reader::CheckUTF(const string& text)
{
  if (text.empty()) {
    return UTFCode::UTF_NULL;
  }

  const char* str = text.c_str();

  UTFCode utfDetected = UTFCode::UTF_NULL;
  switch (*str++) {
    case (char)0xEF: {
      switch (*str++) {
        case (char)0xBB: {
          switch (*str++) {
            case (char)0xBF: {
              utfDetected = UTFCode::UTF8;
            } break;
            default:
              break;
          }
        } break;
        default:
          break;
      }
    } break;

    case (char)0xFE: {
      switch (*str++) {
        case (char)0xFF: {
          utfDetected = UTFCode::UTF16_BE;
        } break;
        default:
          break;
      }
    } break;

    case (char)0xFF: {
      switch (*str++) {
        case (char)0xFE: {
          utfDetected = UTFCode::UTF16_LE;
        } break;
        default:
          break;
      }
    } break;
    default:
      break;
  }

  if (utfDetected != UTFCode::UTF_NULL) {
    string txt = "UTF Error";

    auto find = m_UTFCodeText.find(utfDetected);
    if (find != m_UTFCodeText.end()) {
      txt = find->second;
    }

    LogMsg("M411", VAL("UTF", txt));     //"\nUnicode: Preamble for %s should not be used, already specified via '<?xml'"
    if (utfDetected != UTFCode::UTF8) {
      LogMsg("M413", VAL("UTF", txt));   // "\nUTF Format not supported: %s", UTFCode_str[isUtf]
    }
  }

  if (*str) {
    LogMsg("M412", VAL("STR", str));      //"\nUnicode: Unsupported format or extra characters found before '<?xml': \"%s\"\n", str
  }

  return utfDetected;
}

bool XML_Reader::InitDocument()
{
  uint32_t cnt=0;

  do {
    uint32_t status = NextEntry();
    if (status == 0) {
      break;
    }

    if(m_xmlData.type != TagType::TAG_DOC_DESCRIPTION) {
      if(m_xmlData.type == TagType::TAG_TEXT) {
        CheckUTF(m_xmlData.tagData.c_str());                  // Unicode detection: UTF8, 16LE/BE
      }

      if(m_xmlData.lineNo > 5 || cnt++ > 64) {                // search first lines (or chars if a 1-line-file is used as input file)
        break;
      }
    }
  } while(m_xmlData.type != TagType::TAG_DOC_DESCRIPTION);

  if(m_xmlData.tagData == "xml") {
    return true;
  }

  return false;
}

bool XML_Reader::ReadNext()
{
  char c=0, c_prev=0, c_prev2=0;
  uint32_t isAttribute=0;

  string& workBuf = m_xmlData.tagData;
  TagType& type = m_xmlData.type;

  type = TagType::TAG_TEXT;
  workBuf.clear();
  m_xmlData.attribute.clear();
  m_xmlData.attrReadPos = 0;
  m_xmlData.attrLen = 0;

  bool bOk = true;
  do {                    // search for '<'
    c_prev = c;

    bOk = Getc(c);
    if (!bOk) {
      break;
    }

    if (c == '\r') {        // CR
      workBuf += c;
      continue;
    }
    else if (c == '\t') {   // TAB
      continue;             // ignore
    }
    else if (c == '\n') {   // LF
      m_xmlData.lineNo++;

      if (workBuf.empty() || workBuf.back() != '\r') {
        workBuf += '\r';
      }
      workBuf += c;
      continue;
    }
    else if ((c == '&') && (type == TagType::TAG_TEXT)) {  // parse for special characters
      string specialChar;

      do {
        c_prev = c;

        bOk = Getc(c);
        if (!bOk) {
          break;
        }

        if (c == '\n') {
          m_xmlData.lineNo++;
        }
        else if (c == ';') {
          break;
        }

        specialChar += c;

        if (c == '/' && c_prev == '<') {       // probably found an error
          LogMsg("M414", VAL("SPECIALCHAR", specialChar), MSG("Found END Tag!"), GetLineNumber());   // "Line %d: Problem while parsing XML special characters: '%s'. Found END Tag!\n", GetLineNumber(), m_xmlData.specialChar
          CorrectCnt(-1 * (int)specialChar.length());
          break;
        }

        if (specialChar.length() > 32) {
          LogMsg("M414", VAL("SPECIALCHAR", specialChar), MSG("String too long!"), GetLineNumber());   // "Line %d: Problem while parsing XML special characters: '%s'. String too uint32_t!\n", GetLineNumber(), m_xmlData.specialChar
          CorrectCnt(-1 * (int)specialChar.length());
          break;
        }
      } while (bOk);

      ConvertSpecialChar(specialChar, c);
    }
    else if (c == '<') {
      if (!workBuf.empty()) {
        type = TagType::TAG_TEXT;
        CorrectCnt(-1);
        break;
      }
      else {
        type = TagType::TAG_BEGIN;
        break;
      }
    }
    workBuf += c;
  } while (bOk);

  if (type != TagType::TAG_TEXT) {
    do {
      c_prev2 = c_prev;
      c_prev = c;

      bOk = Getc(c);
      if (!bOk) {
        break;
      }

      if (c == '\r') {
        continue;             // ignore CR
      }
      else if (c == '\t') {
        continue;             // ignore TAB
      }
      else if (c == '\n') {
        m_xmlData.lineNo++;
        //continue;           // ignore LF
      }
      if (c == '!' && c_prev == '<') {
        type = TagType::TAG_DOC_HEADER;
        continue;
      }
      if (c == '?' && c_prev == '<') {
        type = TagType::TAG_DOC_DESCRIPTION;
        continue;
      }
      else if (c == '-') {
        if (c_prev == '-') {
          if (type == TagType::TAG_DOC_HEADER) {
            type = TagType::TAG_COMMENT;
          }
          else if (c_prev2 == '<') {
            LogMsg("M415", GetLineNumber());   // printf("Line %d: '<--' found, should this be a comment '<!--' ?\n", GetLineNumber());
            type = TagType::TAG_COMMENT;
          }
        }
      }
      else if (c == '>') {
        if (type == TagType::TAG_COMMENT) {
          if (c_prev == '-') {
            if (c_prev2 == '-') {
              break;
            }
          }
        }
        else if (c_prev == '/') {
          type = TagType::TAG_SINGLE;
          break;
        }
        else {
          break;
        }
      }
      else if (c == '/') {
        if (type == TagType::TAG_COMMENT) {
          continue;
        }
        else if ((type != TagType::TAG_DOC_HEADER) && (type != TagType::TAG_DOC_DESCRIPTION)) {
          type = TagType::TAG_END;
        }
        continue;
      }
      else if ((c == ' ') && (type != TagType::TAG_DOC_HEADER) && (type != TagType::TAG_COMMENT)) {       // skip Data inside Tag
        do {
          c_prev = c;

          bOk = Getc(c);
          if (!bOk) {
            break;
          }

          if (c == '\n') {
            m_xmlData.lineNo++;
          }
          if (c == '>') {
            if (c_prev == '/') {
              type = TagType::TAG_SINGLE;
            }
            break;
          }
          else {
            m_xmlData.attribute += c;
          }
        } while (bOk);
        break;
      }
      else {
        if ((c == ' ') && ((type == TagType::TAG_BEGIN) || (type == TagType::TAG_END))) {
          isAttribute = 1;    // ignore additional content in tags
        }
        if (!isAttribute) {
          workBuf += c;
        }
      }
    } while (bOk);
  }

  m_xmlData.attrLen = m_xmlData.attribute.length();

  if (!bOk) {
    return false;
  }

  return true;
}

bool XML_Reader::PushTag(const string& tag)
{
  if (tag.empty()) {
    return false;
  }

  m_xmlTagStack.push_back(tag);

  return true;
}

bool XML_Reader::PopTag(string& tag)
{
  if(m_xmlTagStack.empty()) {
    return false;
  }

  tag = m_xmlTagStack.back();
  m_xmlTagStack.pop_back();

  return true;
}

void XML_Reader::PrintTagStack()
{
  string msg;

  for(auto tag : m_xmlTagStack) {
    if(!msg.empty()) {
      msg += "  -> ";
    }

    msg += tag;
  }

  LogMsg("M406", MSG(msg));   // "XML Stack: "
}

bool XML_Reader::NextEntry ()
{
  bool doTryAgain = true;

  do {
    bool bOk = ReadNext();
    if (!bOk) {
      return false;
    }

    if (!Trim(m_xmlData.tagData)) {                 // any payload?
      m_xmlData.type = TagType::TAG_NONE;
      continue;
    }

    if (m_xmlData.type == TagType::TAG_END) {
      m_bIsPrevText = 0;
      string xmlTag;
      PopTag(xmlTag);

      if(!m_bPrevTagIsSingle && (xmlTag != m_xmlData.tagData)) {
        LogMsg("M417", GetLineNumber());          // "Line %d: Inconsistent XML Structure\n", GetLineNumber()
        PushTag(xmlTag);
        PrintTagStack();
        PopTag(xmlTag);

        if(doTryAgain) {
          if(!xmlTag.empty()) {
            LogMsg("M401", VAL("TAG", xmlTag));   // "Line %d: Did you mean '%s' ?\n", GetLineNumber(), xmlTag
            m_xmlData.tagData = xmlTag;
          }
        }
      }
      else if(m_bPrevTagIsSingle) {
        m_bPrevTagIsSingle = false;
        if(xmlTag != m_xmlData.tagData) {
          LogMsg("M417", GetLineNumber());        // "Line %d: Inconsistent XML Structure\n", GetLineNumber()
          PushTag(xmlTag);
          PrintTagStack();
          PopTag(xmlTag);

          if(doTryAgain) {
            if(!xmlTag.empty() && (xmlTag != m_xmlData.tagData))  {
              LogMsg("M401", VAL("TAG", xmlTag));   // "Line %d: Did you mean '%s' ?\n", GetLineNumber(), xmlTag
              m_xmlData.tagData = xmlTag;
            }
          }
        }
      }
    }
    else if(m_xmlData.type == TagType::TAG_BEGIN) {
      if(!PushTag(m_xmlData.tagData)) {
        LogMsg("M418", GetLineNumber());            // "Line %d: XML Stack deeper than 30 Items! Giving up...\n", GetLineNumber()
        return 0;
      }
      if(m_bIsPrevText) {
        LogMsg("M419", GetLineNumber());            // "Line %d: Begin Tag follows Text. Missing End Tag?\n", GetLineNumber()
        m_bIsPrevText = false;
        string xmlTag;
        PopTag(xmlTag);                             // correct stack
        PopTag(xmlTag);
        PushTag(m_xmlData.tagData);
      }
    }
    else if(m_xmlData.type == TagType::TAG_SINGLE) {
      ;   // Intended statement, for future use
    }

    m_bIsPrevText = (m_xmlData.type == TagType::TAG_TEXT);

  } while((m_xmlData.type != TagType::TAG_TEXT)             &&
          (m_xmlData.type != TagType::TAG_DOC_DESCRIPTION)  &&
          (m_xmlData.type != TagType::TAG_BEGIN)            &&
          (m_xmlData.type != TagType::TAG_END)              &&
          (m_xmlData.type != TagType::TAG_SINGLE)             );

  m_bPrevTagIsSingle = (m_xmlData.type == TagType::TAG_SINGLE);

  return true;
}

bool XML_Reader::Recover()
{
  string errorTag;
  string xmlTag;
  uint32_t recoverCnt=0;

  errorTag = m_xmlData.tagData;
  LogMsg("M407", GetLineNumber());    // "\nLine %d: Recover from Error\n", GetLineNumber()

  if(m_bFirstTry) {                     // try to read a statement then return
    bool bOk = ReadNext();
    if (!bOk) {
      return false;
    }
    m_bFirstTry = false;
    return 0;
  }

  m_bFirstTry = true;
  bool bOk;

  do {
    bOk = ReadNext();
    if (!bOk) {
      return false;
    }

    if (m_xmlData.type == TagType::TAG_END){
      PopTag(xmlTag);
    } else if(m_xmlData.type == TagType::TAG_BEGIN) {
      PushTag(m_xmlData.tagData);
    }

    LogMsg("M013", GetLineNumber());

    if(m_xmlData.type == TagType::TAG_END) {        // try continue after end tag  (Experimental!)
      break;
    }

    if (m_xmlData.type != TagType::TAG_TEXT) {      // search for the prev. accepted begin tag
      if(m_xmlData.tagData == m_xmlData.beginTag) {
        break;
      }

      if(m_xmlData.tagData == errorTag) {           // found end of unknown tag
        LogMsg("M409", VAL("TAG", errorTag),  GetLineNumber());   // "Line %d: Skipping unknown Tag: '%s'\n", GetLineNumber(), errorTag
        break;
      }
    }

    recoverCnt++;
    if(recoverCnt > 100) {
      LogMsg("M408", GetLineNumber());            // "\nLine %d: Recover from Error: giving up after 100 tries...\n", GetLineNumber()
      break;
    }
  } while(bOk);

  LogMsg("M015", GetLineNumber());

  return bOk;
}

bool XML_Reader::ReadNextAttribute(bool bInorePrefixes)
{
  uint32_t isTag=1, insideString=0;
  bool foundAttribute = false;
  uint32_t foundAttrString=0;
  char c=0, cPrev=0;
  char stringStartChar=0;

  m_xmlData.attrTag.clear();
  m_xmlData.attrData.clear();

  while(m_xmlData.attrReadPos < m_xmlData.attrLen) {
    cPrev = c;
    c = m_xmlData.attribute[m_xmlData.attrReadPos++];

    if(m_xmlData.attrReadPos == m_xmlData.attrLen) {
      if(c == '?') {
        m_xmlData.type = TagType::TAG_XML_HEADER_END;
        break;
      }
    }

    if((c == '"' || c == '\'') && (!insideString || c == stringStartChar)) {
      if (!insideString) {
        stringStartChar = c;
        insideString = 1;
      }
      else {
        insideString = 0;
        stringStartChar = 0;
      }

      foundAttrString++;
      continue;
    }

    if(foundAttrString >= 2) {          // found end of current attribute
      m_xmlData.attrReadPos--;
      break;
    }

    if((c == ' ') && (!insideString)) {
      continue;                         // skip extra SPACE
    }
    if((c == '=') && (!insideString)) {
      isTag = 0;
      continue;
    }

    if (c == '&') {                     // parse for special characters
      string specialChar;

      do {
        cPrev = c;
        c = m_xmlData.attribute[m_xmlData.attrReadPos++];
        if (c == ';') {
          break;
        }
        specialChar += c;

        if(c == '/' && cPrev == '<') {       // probably found an error
          LogMsg("M414", VAL("SPECIALCHAR", specialChar), MSG("Found END Tag!"), GetLineNumber());   // "Line %d: Problem while parsing XML special characters: '%s'. Found END Tag!\n", GetLineNumber(), m_xmlData.specialChar
          CorrectCnt(-1 * (int)specialChar.length());
          break;
        }

        if (specialChar.length() > 32) {
          LogMsg("M414", VAL("SPECIALCHAR", specialChar), MSG("String too uint32_t!"), GetLineNumber());   // "Line %d: Problem while parsing XML special characters: '%s'. String too uint32_t!\n", GetLineNumber(), m_xmlData.specialChar
          CorrectCnt(-1 * (int)specialChar.length());
          break;
        }
      } while (c != ';');

      ConvertSpecialChar(specialChar, c);
    }

    if(isTag) {
      if(bInorePrefixes && c == ':' ) {            // ignore xmlns: and other prefixes in attributes
        foundAttribute = false;
        m_xmlData.attrTag.clear();
        continue;
      }
      m_xmlData.attrTag += c;
      foundAttribute = true;
    } else {
      m_xmlData.attrData += c;
    }
  }

  if(insideString) {
    LogMsg("M420", VAL("ATTRLINE", m_xmlData.attribute));
  }

  if(m_xmlData.attrReadPos > m_xmlData.attrLen) {
    m_xmlData.attrReadPos = 0;
    m_xmlData.attrLen = 0;
    foundAttribute = false;
    m_xmlData.attribute.clear();
    m_xmlData.attrTag.clear();
    m_xmlData.attrData.clear();
  }

  if(!foundAttribute) {
    m_xmlData.attrReadPos = 0;
  }

  return foundAttribute;
}

bool XML_Reader::GetNextNode (XmlNode_t& node)
{
  const bool bOk = NextEntry();
  if(!bOk) {
    node.bEndOfFile = 1;
    return false;
  }
  node.bEndOfFile = 0;
  node.type = m_xmlData.type;
  node.lineNo = m_xmlData.lineNo;
  node.bHasAttributes = HasAttributes();

  switch(m_xmlData.type) {
    case TagType::TAG_BEGIN:
    case TagType::TAG_END:
    case TagType::TAG_SINGLE:
      node.tag = m_xmlData.tagData;
      break;

    case TagType::TAG_TEXT:
    default:
      node.data = m_xmlData.tagData;
      break;
  }

  return true;
}
