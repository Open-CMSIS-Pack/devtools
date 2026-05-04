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

/**
 * @brief encapsulates data types and structures needed for XML Reader
*/
namespace XmlTypes {

  /**
   * @brief error types that happen during read
  */
  enum class Err {
    ERR_NOERR = 0,                // no error occurred
    ERR_EOF,                      // end of file
    ERR_NO_INPUT_FILE,            // no input file given
    ERR_OPEN_FAILED,              // opening file failed
    ERR_NOXML,                    // not an XML file
    ERR_CRITICAL,                 // unspecified internal error
  };

  /**
   * @brief types of XML tags
  */
  enum class TagType {
    TAG_NONE,                     // no type
    TAG_BEGIN,                    // begin tag: <foo>
    TAG_END,                      // end tag: </foo>
    TAG_SINGLE,                   // single tag: <foo/>
    TAG_ATTRIBUTE,                // attribute: foo="bar"
    TAG_TEXT,                     // text: <foo>text</foo>
    TAG_DOC_HEADER,               // document header
    TAG_DOC_DESCRIPTION,          // document description
    TAG_COMMENT,                  // XML comment
    TAG_XML_HEADER_END            // end of XML header
  };

  /**
   * @brief types for Unicode detection: UTF8, 16LE/BE
  */
  enum class UTFCode {
    UTF_NULL,                     // no Unicode
    UTF8,                         // UTF 8
    UTF16_BE,                     // UTF 16, big endian
    UTF16_LE,                     // UTF 16, little endian
    UTF32_BE,                     // UTF 32, big endian
    UTF32_LE,                     // UTF 32, little endian
    UTF7                          // UTF 7
  };

  /**
   * @brief internal structure for XML processing current XML element
  */
  struct XmlData_t {
    TagType       type;           // type of XML tag
    std::string   tagData;        // tag name or data
    std::string   beginTag;       // name of corresponding begin tag
    std::string   attribute;      // complete attribute string containing all attributes of current tag
    std::string   attrTag;        // holds next attribute tag
    std::string   attrData;       // holds next attribute data
    size_t        attrLen;        // string length of complete attribute string
    uint32_t      lineNo;         // line number in XML file
    size_t        readPos;        // current position in read buffer
    size_t        prevReadPos;    // previous position in read buffer
    size_t        attrReadPos;    // read position in attribute string
    size_t        fileSize;       // size of XML file
  };

  /**
   * @brief XML reader interface
  */
  struct XmlNode_t {
    TagType       type;           // type of XML tag
    std::string   tag;            // tag name
    std::string   data;           // tag data
    uint32_t      lineNo;         // line number in XML file
    bool          bHasChildren;   // sub-nodes present
    bool          bEndOfFile;     // end of file reached
    bool          bHasAttributes; // node has attributes
  };

  /**
   * @brief input source (file or character string buffer). Caches current state in case of XML include.
  */
  struct InputSource_t {
    size_t        seekPos;        // seek position in file / buffer
    uint32_t      lineNo;         // last line number read
    size_t        attrReadPos;    // last read position in attribute string
    std::string   fileName;       // name of input file
    std::string   attribute;      // last read attribute string
    const char*   xmlString;      // string to parse instead of file
    std::string   tag;            // last read tag
    TagType       type;           // type of last read tag
  };
}


/**
 * @brief input source (file or character string buffer). Caches current state in case of XML include.
 *        class to read string(s) from InputSource_t
 *        default processes "const string&" as input source
 */
class XML_InputSourceReader {
public:
  /**
   * @brief constructor
  */
  XML_InputSourceReader();

  /**
   * @brief virtual destructor
  */
  virtual ~XML_InputSourceReader();

  /**
   * @brief close source
  */
  virtual void Close();

  /**
   * @brief open source
   * @param source input source to be opened
   * @return Err type error / success
  */
  virtual XmlTypes::Err Open(XmlTypes::InputSource_t* source);

  /**
   * @brief reads a buffer into cache
   * @param buf destination buffer
   * @param maxLen max length to be read
   * @return actual size read
  */
  virtual size_t ReadLine(char* buf, size_t maxLen);

  /**
   * @brief get size of input source (file or buffer)
   * @return size of input source
  */
  size_t GetSize() const {
    return m_size;
  }

  /**
   * @brief checks if the input source is valid
   * @return input source is valid or not
  */
  virtual bool IsValid() const {
    return true;
  }

protected:
  /**
   * @brief Opens the input source
   * @return Err type error / success
  */
  virtual XmlTypes::Err DoOpen();

  XmlTypes::InputSource_t* m_source = nullptr;
  size_t m_size = 0;
};

/**
 * @brief reads and basically analyzes an XML file.
 *        XML control elements and special characters are translated.
 *        The reader checks closing vs. opening tags and the XML stack.
 *
 * Theory of operation:
 * An input file or buffer (e.g. running on WebAssembly) specifies the input buffer.
 * The reader acts as stream reader and buffers portions of the input file.
 * Once started, it runs on a "GetNext()" basis, returning the next XML element (see TagType), e.g.:
 * - begin or single tag
 *   -- flag is set if attributes are present
 * - end tag
 * - data (text) between tags
 *
 * Each end tag is checked against the corresponding begin tag (stored on a stack) for XML consistency.
 *
 * Instantiate the reader:
 * #include "XML_InputSourceReaderFile.h"
 * XML_InputSourceReader* inputSourceReader = new XML_InputSourceReaderFile()
 * XML_Reader* pXmlReader = new XML_Reader(inputSourceReader);

 * By using the following functions, an XML file can be scanned through, or a DOM Model can be created:
 * while(node.bEndOfFile) {
 *   pXmlReader->GetNextNode(node);
 *   // process node
 *   if(pXmlReader->HasAttributes()) {
 *     while(pXmlReader->ReadNextAttribute()) {
 *       pXmlReader->GetAttributeTag();
 *       pXmlReader->GetAttributeData();
 *       // process attribute
 *     }
 *   }
 * }
 */
class XML_Reader {
public:
  /**
   * @brief explicit constructor
   * @param inputSourceReader input to read from
  */
  explicit XML_Reader(XML_InputSourceReader* inputSourceReader);


  /**
  * @brief destructor
  */
  ~XML_Reader();

  /**
   * @brief initialize XML reader
   * @param fileName name of input file
   * @param xmlString buffer with XML file
   * @return Err type error / success
  */
  XmlTypes::Err Init(const std::string& fileName, const std::string& xmlString);

  /**
   * @brief uninitialize reader
  */
  void UnInit();

  /**
   * @brief read the next XML node
   * @param node struct to buffer node information
   * @return success true/false
  */
  bool GetNextNode(XmlTypes::XmlNode_t& node);

  /**
   * @brief get current line number
   * @return 1-based line number
  */
  uint32_t GetLineNumber();

  /**
   * @brief recover from error in case of inconsistent XML structure
   * @return success true/false
  */
  bool Recover();

  /**
   * @brief returns if the node has attributes
   * @return has attributes true/false
  */
  bool HasAttributes();

  /**
   * @brief reads the next attribute, e.g. attrTag="attrData"
   * @param bInorePrefixes ignore xmlns: and other prefixes in attributes (default = true)
   * @return success true/false
  */
  bool ReadNextAttribute(bool bInorePrefixes = true);

  /**
   * @brief returns previous read attribute tag
   * @return string containing attribute name
  */
  const std::string& GetAttributeTag();

  /**
   * @brief returns previous read attribute value
   *        attribute-Value Normalization: http://www.w3.org/TR/REC-xml/#AVNormalize
  *         deletes leading and trailing white spaces

   * @return string containing attribute value
  */
  const std::string& GetAttributeData();

  /**
   * @brief converts XML special characters, e.g. "&amp;"
   * @param specialChar string containing the sequence
   * @param found character interpreted from sequence, defaults to '&'
   * @return success true/false
  */
  static bool ConvertSpecialChar(const std::string& specialChar, char& found);

  /**
   * @brief checks BOM header for UTF
   * @param text first characters of the XML buffer
   * @return XmlTypes::UTFCode found
  */
  static XmlTypes::UTFCode CheckUTF(const std::string& text);

  /**
   * @brief deletes leading and trailing whitespace from string
   * @param text input / output buffer
   * @return length of resulting buffer
  */
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

  /**
   * @brief prints out XML stack, for error reporting on an improper XML closing tag
  */
  void PrintTagStack();

private:
  /**
   * @brief opens source from m_SourceStack
   * @return Err type error / success
  */
  XmlTypes::Err Open();

  /**
   * @brief closes current input source
   * @return Err type error / success
  */
  XmlTypes::Err Close();

  /**
   * @brief gets current input source
   * @return input source
  */
  XmlTypes::InputSource_t* GetCurrentSource();

  /**
   * @brief adds a new input source to the reader and switches to it
   * @param fileName file name of new source
   * @param xmlString buffer to new source
   * @return Err type error / success
  */
  XmlTypes::Err NextSource(const std::string& fileName, const std::string& xmlString);

  /**
   * @brief closes current input source and removes it from stack
   * @return Err type error / success
  */
  XmlTypes::Err PrevSource();

  /**
   * @brief initialization of messages table
  */
  void InitMessageTable();

  /**
   * @brief initializes a new XML source and checks if valid
   * @return success true/false
  */
  bool InitDocument();

  /**
   * @brief reads next XML element (tag, data)
   * @return success true/false
  */
  bool ReadNext();

  /**
   * @brief evaluates XML element read by ReadNext()
   * @return success true/false
  */
  bool NextEntry();

  /**
   * @brief reads a buffer into cache
   * @return length of the read buffer
  */
  size_t ReadLine();

  /**
   * @brief pushes tag to stack for XML consistency check and recover function
   * @param tag name of tag
   * @return success true/false
  */
  bool PushTag(const std::string& tag);

  /**
   * @brief pops tag from stack for XML consistency check and recover function
   * @param tag returns name of tag
   * @return success true/false
  */
  bool PopTag(std::string& tag);

  /**
   * @brief get string length of whole attribute(s) for current tag
   * @return length of attribute string
  */
  size_t GetAttributeLen();

  /**
   * @brief Get next character from stream buffer
   * @param c the next character to be processed after Getc()
   * @return success true/false
  */
  bool Getc(char& c);

  /**
   * @brief recalculates offset in current read buffer.
   * @param corr offset, e.g. -1
  */
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

  static const std::map<const std::string, const char> m_specialChars;                  // XML special chars
  static const std::map<const XmlTypes::UTFCode, const std::string> m_UTFCodeText;      // Unicode detection: UTF8, 16LE/BE
  static const std::string EMPTYSTR;

  static const MsgTable msgTable;
  static const MsgTableStrict msgStrictTable;
};

#endif // !XML_READER_H
