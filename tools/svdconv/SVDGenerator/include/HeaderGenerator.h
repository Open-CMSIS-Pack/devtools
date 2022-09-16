/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef HeaderGenerator_H
#define HeaderGenerator_H

#include "CodeGenerator.h"
#include "HeaderGenAPI.h"
#include "SvdTypes.h"
#include "FileIo.h"


struct CmsisCfg {
  SvdTypes::CpuType cpuType;              // SvdTypes::GetCpuType()
  uint32_t      cpuRevision;
  uint32_t      mpuPresent;
  uint32_t      fpuPresent;
  uint32_t      nvicPrioBits;
  uint32_t      vendorSystickConfig;
  uint32_t      vtorPresent;
  uint32_t      dspPresent;
  uint32_t      fpuDP;
  uint32_t      icachePresent;
  uint32_t      dcachePresent;
  uint32_t      itcmPresent;
  uint32_t      dtcmPresent;
  uint32_t      sauPresent;
  uint32_t      pmuPresent;
  uint32_t      pmuNumEventCnt;
  uint32_t      mvePresent;
  uint32_t      mveFP;
  SvdTypes::CmsisCfgForce forceGeneration;
  uint32_t      Reserved[28];
};


class HeaderGenerator : public CodeGenerator {
public:
  HeaderGenerator(FileIo *fileIo);
  ~HeaderGenerator();

protected:
  void  GenerateHeader                    (const std::string& text, c_header::Special what);
  void  GenerateFileDescription           (const std::string& fileName);
  void  Generate_NewLine                  (void);
  void  MakeFile                          (const std::string& name);
  void  MakePeripheralMapping             (const std::string& periName, uint32_t baseAddress);
  void  MakePeripheralAddressMapping      (const std::string& periName, uint32_t baseAddress, const std::string& typeName, const std::string& prefix);
  void  MakePeripheralArrayAddressMapping (const std::string& periName, uint32_t baseAddress, const std::string& typeName, const std::string& prefix);
  void  MakePeripheralAddressDefine       (const std::string& periName, uint32_t baseAddress, const std::string& prefix);
  void  MakeDoxyCommentAddress            (const std::string& text,     uint32_t address);
  void  MakeDoxyCommentNumber             (const std::string& text,     uint32_t number);
  void  MakeDoxyComment                   (const std::string& text);
  void  MakeDoxyCommentBitField           (const std::string& text, uint32_t firstBit, uint32_t lastBit);
  void  MakeDoxyCommentBitPos             (const std::string& text, uint32_t pos);
  void  MakeInterruptStruct               (const std::string& text, int32_t number, bool lastEnum);
  void  MakeDoxygenAddPeripheral          (const std::string& text);
  void  MakeDoxygenAddGroup               (const std::string& text);
  void  MakeDoxygenEndGroup               (const std::string& text);
  void  MakeAnnonUnionCompiler            (const std::string& text, bool bStart);
  void  MakeCMSISConfig                   (const std::string& text, const std::string& MCUName, CmsisCfg cmsisCfg);
  void  MakeRegisterStruct                (const std::string& text, SvdTypes::Access, const std::string& dataType, uint32_t size);
  void  MakeFieldStruct                   (const std::string& text, SvdTypes::Access, const std::string& dataType, uint32_t size, uint32_t bitWidth);
  void  MakeFieldUnion                    (const std::string& text, SvdTypes::Access accessType, const std::string& dataType, uint32_t size);
  void  MakeEnumValue                     (const std::string& enumName, uint32_t val, const std::string& name, const std::string& descr);
  void  MakeDefine                        (const std::string& name, uint32_t val, uint32_t base, uint32_t tabOffs, const std::string& descr);
  void  MakeDefineText                    (const std::string& text, const std::string& repl);
  void  MakeHeaderIfDef                   (const std::string& name, bool begin);
  void  MakeHeaderExternC                 (const std::string& name, bool begin);
  void  MakeTypedefToArray                (const std::string& name, uint32_t num);
  void  MakeEnumStructUnionEnd            (const std::string& textBuf, c_header::Index elementIndex, c_header::Special specialType, c_header::Additional langAdditional);
  void  MakeEnumStructUnionEndArray       (const std::string& textBuf, c_header::Index elementIndex, c_header::Special specialType, c_header::Additional langAdditional, uint32_t num);
  void  MakeFieldPosMask                  (const std::string& field, const std::string& peri, const std::string& reg, uint32_t pos, uint32_t mask);
  void  MakeFieldPosMask2                 (const std::string& field, const std::string& peri, const std::string& reg, uint32_t pos, uint32_t mask);
  void  MakeFieldPosMask3                 (const std::string& name, const std::string& field, uint32_t pos, uint32_t mask);
  void  CreateDoxyComment                 (const std::string& str);
  void  CreateDoxyCommentStar             (const std::string& str);
  void  CreateDoxyCommentLineBreak        (const std::string& str);
  void  CreateCError                      (const std::string& str, uint32_t lineNo);
  void  CreateCWarning                    (const std::string& str, uint32_t lineNo);
  void  CreateCComment                    (const std::string& text);
  void  CreateCCommentBegin               (const std::string& text);
  void  CreateCCommentEnd                 (const std::string& text);
  void  GenerateH_CRTabOffset             (uint32_t howMany);
  void  WriteText                         (const std::string& text);
  void  MakeIfNDef                        (const std::string& text, const std::string& descr);
  void  MakeIfNDefHeader                  (const std::string& text, const std::string& descr);
  void  MakeEndIf                         (const std::string& text);
  void  MakeInclude                       (const std::string& text);
  void  MakeIncludeCore                   (const std::string& text, const std::string& cpuName);
  void  MakeIncludeSystem                 (const std::string& text, const std::string& mcuName);
  void  MakeCPlusPlus                     (const std::string& text, bool bStart);

public:
  void SetDebugHeaderfile(bool debugHeaderfile = true) { m_debugHeaderfile = debugHeaderfile; }

  template<uint32_t element, typename ...Args>
  void Generate(const std::string& text, Args&&... args) {
    uint32_t  noNewLine=0;

    constexpr c_header::Index elementIndex        = (c_header::Index)(element & c_header::INDEX_MASK   );
    constexpr c_header::Options elementType       = (c_header::Options)(element & c_header::OPTIONS_MASK );
    constexpr c_header::Special specialType       = (c_header::Special)(element & c_header::SPECIAL_MASK );
    constexpr c_header::Additional langAdditional = (c_header::Additional)(element & c_header::LANGADD_MASK );

    if constexpr ((langAdditional) && (elementType != c_header::END)) {
      Generate<c_header::DIRECT>("%s ", m_langAddH[langAdditional >> c_header::LANGADD_SHIFT]);
      noNewLine = 1;
    }

    if constexpr (elementType == c_header::MAKE) {
      //m_prevClosedStructUnion = false;
      m_prevOpenedStructUnion = false;

      if constexpr (specialType == c_header::MK_PERIMAP) {
        parse_and_call(&HeaderGenerator::MakePeripheralMapping, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_PERIADDRDEF) {
        parse_and_call(&HeaderGenerator::MakePeripheralAddressDefine, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_PERIADDRMAP) {
        parse_and_call(&HeaderGenerator::MakePeripheralAddressMapping, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_PERIARRADDRMAP) {
        parse_and_call(&HeaderGenerator::MakePeripheralArrayAddressMapping, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_DOXY_COMMENT) {
        parse_and_call(&HeaderGenerator::MakeDoxyComment, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_DOXY_COMMENT_BITFIELD) {
        parse_and_call(&HeaderGenerator::MakeDoxyCommentBitField, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_DOXY_COMMENT_BITPOS) {
        parse_and_call(&HeaderGenerator::MakeDoxyCommentBitPos, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_DOXY_COMMENT_ADDR) {
        parse_and_call(&HeaderGenerator::MakeDoxyCommentAddress, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_INTERRUPT_STRUCT) {
        parse_and_call(&HeaderGenerator::MakeInterruptStruct, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_DOXYADDGROUP) {
        parse_and_call(&HeaderGenerator::MakeDoxygenAddGroup, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_DOXYENDGROUP) {
        parse_and_call(&HeaderGenerator::MakeDoxygenEndGroup, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_DOXYADDPERI) {
        parse_and_call(&HeaderGenerator::MakeDoxygenAddPeripheral, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_CMSIS_CONFIG) {
        parse_and_call(&HeaderGenerator::MakeCMSISConfig, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_REGISTER_STRUCT) {
        parse_and_call(&HeaderGenerator::MakeRegisterStruct, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_FIELD_STRUCT) {
        parse_and_call(&HeaderGenerator::MakeFieldStruct, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_FIELD_UNION) {
        parse_and_call(&HeaderGenerator::MakeFieldUnion, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_FIELD_POSMASK) {
        parse_and_call(&HeaderGenerator::MakeFieldPosMask, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_FIELD_POSMASK2) {
        parse_and_call(&HeaderGenerator::MakeFieldPosMask2, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_FIELD_POSMASK3) {
        parse_and_call(&HeaderGenerator::MakeFieldPosMask3, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_ENUMVALUE) {
        parse_and_call(&HeaderGenerator::MakeEnumValue, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_DEFINE) {
        parse_and_call(&HeaderGenerator::MakeDefine, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_DEFINE_TEXT) {
        parse_and_call(&HeaderGenerator::MakeDefineText, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_HEADERIFDEF) {
        parse_and_call(&HeaderGenerator::MakeHeaderIfDef, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_HEADEREXTERNC_BEGIN) {
        parse_and_call(&HeaderGenerator::MakeHeaderExternC, text, true, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_HEADEREXTERNC_END) {
        parse_and_call(&HeaderGenerator::MakeHeaderExternC, text, false, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_TYPEDEFTOARRAY) {
        parse_and_call(&HeaderGenerator::MakeTypedefToArray, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_IFNDEF) {
        parse_and_call(&HeaderGenerator::MakeIfNDef, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_IFNDEF_H) {
        parse_and_call(&HeaderGenerator::MakeIfNDefHeader, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_ENDIF) {
        parse_and_call(&HeaderGenerator::MakeEndIf, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_INCLUDE) {
        parse_and_call(&HeaderGenerator::MakeInclude, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_INCLUDE_CORE) {
        parse_and_call(&HeaderGenerator::MakeIncludeCore, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_INCLUDE_SYSTEM) {
        parse_and_call(&HeaderGenerator::MakeIncludeSystem, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_ANONUNION_COMPILER) {
        parse_and_call(&HeaderGenerator::MakeAnnonUnionCompiler, text, std::forward<Args>(args)...);
      } else if constexpr (specialType == c_header::MK_CPLUSPLUS) {
        parse_and_call(&HeaderGenerator::MakeCPlusPlus, text, std::forward<Args>(args)...);
      }
    }
        
    else if constexpr (elementType == c_header::DESCR) {
      if constexpr(specialType != c_header::NONE) {
        parse_and_call(&HeaderGenerator::GenerateHeader, text, specialType, std::forward<Args>(args)...);
      }
      else {
        parse_and_call(&HeaderGenerator::GenerateFileDescription, text, std::forward<Args>(args)...);
      }
    }

    else if constexpr (elementType == c_header::BEGIN) {
      m_prevClosedStructUnion = false;
      
      if constexpr (elementIndex == c_header::UNION || elementIndex == c_header::STRUCT || elementIndex == c_header::ENUM) {
        if(!noNewLine) {
          Generate_NewLine();
          if(!m_prevOpenedStructUnion) {
            Generate_NewLine();
          }
        }
        Generate<c_header::RAW>("%s", m_itemsH[elementIndex].c_str());
        m_prevOpenedStructUnion = true;
        m_structUnionCnt++;
      }

      if constexpr (elementIndex == c_header::C_COMMENT) {
        parse_and_call(&HeaderGenerator::CreateCCommentBegin, text, std::forward<Args>(args)...);
      }
      else if constexpr (elementIndex == c_header::DOXY_COMMENT) {
        parse_and_call(&HeaderGenerator::CreateDoxyCommentLineBreak, text, std::forward<Args>(args)...);
      }
      else if constexpr (elementIndex == c_header::DOXY_COMMENT_POSMSK) {
        parse_and_call(&HeaderGenerator::CreateDoxyComment, text, std::forward<Args>(args)...);
      }
      else if constexpr (elementIndex == c_header::DOXY_COMMENTSTAR) {
        parse_and_call(&HeaderGenerator::CreateDoxyCommentStar, text, std::forward<Args>(args)...);
      }      
      else {
        Generate<c_header::RAW>(" ");
        m_tabCount++;
        Generate<c_header::RAW>("{");
        m_bracketBegin++;
      }        
    }

    else if constexpr (elementType == c_header::END) {
      if constexpr (elementIndex == c_header::C_COMMENT) {
        parse_and_call(&HeaderGenerator::CreateCCommentEnd, text, std::forward<Args>(args)...);
      }
      else if constexpr (elementIndex == c_header::DOXY_COMMENT) {
        Generate<c_header::RAW>(" */");
      }
      else if constexpr (elementIndex == c_header::DOXY_COMMENTSTAR) {
        Generate<c_header::RAW>("\n*/");
      }      
      else if constexpr(elementIndex == c_header::ENUM || elementIndex == c_header::STRUCT || elementIndex == c_header::UNION) {
        if(m_tabCount) {
          m_tabCount--;
        }
        if(m_bracketBegin) {
          Generate_NewLine();
        }

        Generate<c_header::RAW>("}");
        if(m_bracketBegin) {
          m_bracketBegin--;
        }

        if constexpr (langAdditional == c_header::TYPEDEFARR) {
          parse_and_call(&HeaderGenerator::MakeEnumStructUnionEndArray, text, elementIndex, specialType, langAdditional, std::forward<Args>(args)...);
        }
        else {
          parse_and_call(&HeaderGenerator::MakeEnumStructUnionEnd, text, elementIndex, specialType, langAdditional, std::forward<Args>(args)...);
        }
      }
      else {
        if(m_tabCount) {
          m_tabCount--;
        }
        if(m_bracketBegin) {
          Generate_NewLine();
        }

        Generate<c_header::RAW>("}");
        if(m_bracketBegin) {
          m_bracketBegin--;
        }
      }
    }

    else if constexpr(elementType == c_header::DIRECT) {
      Generate_NewLine();
      Generate<c_header::RAW>(text, std::forward<Args>(args)...);
    }

    else if constexpr(elementType == c_header::RAW) {
      parse_and_call(&HeaderGenerator::WriteText, text, std::forward<Args>(args)...);
    }

    else {
      //m_prevOpenedStructUnion = 0;

      if constexpr(elementIndex == c_header::C_COMMENT) {
        parse_and_call(&HeaderGenerator::CreateCComment, text, std::forward<Args>(args)...);
      }
      else if constexpr(elementIndex == c_header::DOXY_COMMENT) {
        parse_and_call(&HeaderGenerator::MakeDoxyComment, text, std::forward<Args>(args)...);
      }
      else if constexpr(elementIndex == c_header::C_ERROR) {
        parse_and_call(&HeaderGenerator::CreateCError, text, std::forward<Args>(args)...);
      }
      else if constexpr(elementIndex == c_header::C_WARNING) {
        parse_and_call(&HeaderGenerator::CreateCWarning, text, std::forward<Args>(args)...);
      }
      else {
        parse_and_call(&HeaderGenerator::WriteText, text, std::forward<Args>(args)...);
      }
    }
  }

private:
  FileIo*         m_fileIo;
  bool            m_prevOpenedStructUnion;
  bool            m_prevClosedStructUnion;
  bool            m_misraCompliantStruct;
  bool            m_debugHeaderfile;
  uint32_t        m_extraSpaces;
  uint32_t        m_tabCount;
  uint32_t        m_bracketBegin;
  uint32_t        m_structUnionCnt;

  static const std::string m_itemsH[];
  static const std::string m_langAddH[];
  static const std::string m_definePosStr;
  static const std::string m_defineMaskStr;
  static const std::string m_headerCommentBegin;
  static const std::string m_headerCommentEnd;
  static const char m_headerCommentPartChar;
  static const char m_headerCommentSubPartChar;
};

#endif // HeaderGenerator_H
