/*
* Copyright (c) 2020-2021 Arm Limited. All rights reserved.
*
* SPDX-License-Identifier: Apache-2.0
*/

#ifndef XML_READER_H
#define XML_READER_H

#include "ErrLog.h"

#include <cstdint>
#include <string>
#include <list>
#include <map>
#include <algorithm>

constexpr size_t KBYTE(size_t val) { return val * 1024; }
constexpr size_t MBYTE(size_t val) { return KBYTE(val) * KBYTE(1); }

namespace XmlTypes {
  enum class Err {
    ERR_NOERR = 0,
    ERR_EOF,
    ERR_NO_INPUT_FILE,
    ERR_OPEN_FAILED,
    ERR_NOXML,
    ERR_CRITICAL,
  };

  enum class TagType {
    TAG_NONE,
    TAG_BEGIN,
    TAG_END,
    TAG_SINGLE,
    TAG_ATTRIBUTE,
    TAG_TEXT,
    TAG_DOC_HEADER,
    TAG_DOC_DESCRIPTION,
    TAG_COMMENT,
    TAG_XML_HEADER_END
  };

  // Unicode detection: UTF8, 16LE/BE
  enum class UTFCode {
    UTF_NULL,
    UTF8,
    UTF16_BE,
    UTF16_LE,
    UTF32_BE,
    UTF32_LE,
    UTF7
  };

  struct XmlData_t {
    TagType       type;
    std::string   tagData;
    std::string   beginTag;
    std::string   attribute;
    std::string   attrTag;
    std::string   attrData;
    size_t        attrLen;
    uint32_t      lineNo;
    size_t        readPos;
    size_t        prevReadPos;
    size_t        readOffset;
    size_t        attrReadPos;
    size_t        fileSize;
  };

  struct XmlNode_t {
    TagType       type;
    std::string   tag;
    std::string   data;
    uint32_t      lineNo;
    bool          bHasChildren;
    bool          bEndOfFile;
    bool          bHasAttributes;
  };

  struct InputSource_t {
    size_t        seekPos;
    uint32_t      lineNo;
    size_t        attrReadPos;
    std::string   fileName;
    std::string   attribute;
    const char*   xmlString; // string to parse instead of file
    std::string   tag;
    TagType       type;
  };
}

// class to read string(s) from InputSource_t
// default processes const string& as input source
class XML_InputSourceReader {
public:
  XML_InputSourceReader();
  virtual ~XML_InputSourceReader();

  virtual void Close();
  virtual XmlTypes::Err Open(XmlTypes::InputSource_t* source);
  virtual size_t ReadLine(char* buf, size_t maxLen);

  size_t GetSize() const {
    return m_size;
  }

  virtual bool IsValid() const {
    return true;
  }

protected:
  virtual XmlTypes::Err DoOpen();
  XmlTypes::InputSource_t* m_source = nullptr;
  size_t m_size = 0;
};

class XML_Reader {
public:
  explicit XML_Reader(XML_InputSourceReader* inputSourceReader);
  ~XML_Reader();

  XmlTypes::Err Init(const std::string& fileName, const std::string& xmlString);
  void UnInit();

  bool GetNextNode(XmlTypes::XmlNode_t& node);
  uint32_t GetLineNumber();
  bool Recover();

  bool HasAttributes();
  bool ReadNextAttribute(bool bInorePrefixes = true); // ignore xmlns: and other prefixes in attributes (default = true)
  const std::string& GetAttributeTag();
  const std::string& GetAttributeData();

  // Static functions
  static bool ConvertSpecialChar(const std::string& specialChar, char& found);
  static XmlTypes::UTFCode CheckUTF(const std::string& text);
  static inline size_t Trim(std::string& text)
  {
    text.erase(text.begin(), find_if(text.begin(), text.end(), [](char c) {
      return !isspace(c & 0xff);
      }));

    text.erase(find_if(text.rbegin(), text.rend(), [](char c) {
      return !isspace(c & 0xff);
      }).base(), text.end());

    return text.length();
  }

private:
  XmlTypes::Err Open();
  XmlTypes::Err Close();
  XmlTypes::InputSource_t* GetCurrentSource();
  XmlTypes::Err NextSource(const std::string& fileName, const std::string& xmlString);
  XmlTypes::Err PrevSource();

  void InitMessageTable();
  bool InitDocument();

  bool ReadNext();
  bool NextEntry();
  size_t ReadLine();

  bool PushTag(const std::string& tag);
  bool PopTag(std::string& tag);
  void PrintTagStack();

  size_t GetAttributeLen();
  bool Getc(char& c);
  void CorrectCnt(int32_t corr);

private:
  bool m_bIsPrevText;
  bool m_bPrevTagIsSingle;
  bool m_bFirstTry;
  size_t m_streamBufPos;
  size_t m_streamBufLen;
  size_t m_streamBufMaxlen;
  char *m_streamBuf;

  XmlTypes::XmlData_t m_xmlData;
  std::list <std::string> m_xmlTagStack;
  std::list<XmlTypes::InputSource_t> m_SourceStack;
  XML_InputSourceReader* m_InputSourceReader;

  static const std::map<const std::string, const char> m_specialChars;
  static const std::map<const XmlTypes::UTFCode, const std::string> m_UTFCodeText;
  static const std::string EMPTYSTR;

  static const MsgTable msgTable;
  static const MsgTableStrict msgStrictTable;
};

#endif // !XML_READER_H
