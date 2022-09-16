/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SfdGenerator_H
#define SfdGenerator_H

#include "CodeGenerator.h"
#include "SfdGenAPI.h"
#include "FileIo.h"

#include <string>
#include <set>
#include <list>
#include <map>


#define TEXTBUF_SIZE      4096
#define OUT_BUF_SIZE      (128 * 1024)
#define SPACES_PER_TAB      2


// --------------------------  long strings  ----------------------------------

//#define LOC_STRING_EDIT  "( (%s)((%s>>%i) & 0x%X), ( (%s &= (%s)~(0x%X<<%i)), (%s |= (%s)((%s:GuiVal & 0x%X)<<%i)) ) )"
// Parameters:
// String: LOC_String, 
// Read  exp.        : (cast),  RegisterName, DownShift,         AND-Operation,
// Write exp. delete : RegisterName, (cast), DeleteMaskAnd,      UpShift,
// Write exp. write  : RegisterName, (cast), guiEditSizeString,  AND-Operation, UpShift


//( (unsigned int)((SC4_RST >> 0) & 0x87 ), ((SC4_RST = (SC4_RST & (unsigned int)~(0x0   << 0  )) | (unsigned int)((Gui_u32:GuiVal & 0x0  ) << 0  )) ) )
const std::string LOC_STRING_EDIT2 = "( (%s)((%s >> %i) & 0x%X), ((%s = (%s & (%s)~(0x%X << %i )) | (%s)((%s:GuiVal & 0x%X) << %i )) ) )";
// Parameters:
// String: LOC_String, 
// Read  exp.        : (cast),       RegisterName, DownShift, AND-Operation,
// Write exp.        : RegisterName, RegisterName, (cast),    Mask,          UpShift, (cast), guiEditSizeString, AND-Operation, UpShift

//( (unsigned char)((NVIC_IP0>>16) & 0xFF), ((NVIC_IP0 = (NVIC_IP0 & ~(0xFFUL<<16 )) | ((unsigned int)(Gui_u8:GuiVal & 0xFF) << 16 ) ) ))
const std::string LOC_STRING_EDIT3 = "( (%s)((%s >> %i) & 0x%X), ((%s = (%s & ~(0x%XUL << %i )) | ((%s)(%s:GuiVal & 0x%X) << %i ) ) ))";

//( (unsigned char)((NVIC_IP0>>16) & 0xFF)
const std::string LOC_STRING_EDIT3_RO = "( (%s)((%s >> %i) & 0x%X) )";


const std::string LOC_STRING_OBIT = "( (%s) %s )";
// Parameters:
// String: LOC_String, 
// Read  exp.        : (cast),  RegisterName


const std::string ADDRESS_STRING = "unsigned %s %s __AT (0x%08X);\n";
const std::string ADDRESS_STRING_PRE  = "unsigned %s %s_%s __AT (0x%08X);\n";
// Parameters:
// RegisterName, RegisterAddress


// --------------------------------------  Endgroup Stack  -------------------------------------------
#define ITEMSTACK_MAX         100

// --------------------------  defines  ----------------------------------
#define MAIN_MENU_MAX_ENTRIES 100
#define SUB_MENUE_MAX_ENTRIES 100
#define MAX_INC_FILES         128

// -------------------------------  GUI Items  ------------------------------------
#define GUI_EDIT_ITEM_8         0
#define GUI_EDIT_ITEM_16        1
#define GUI_EDIT_ITEM_32        2
                                
#define CAST_CHAR               0
#define CAST_SHORT              1
#define CAST_INT                2
#define CAST_INT64              3
#define CAST_LONG               4


class FileIo;

class SfdGenerator : public CodeGenerator {
public:
   SfdGenerator(FileIo *fileIo);
  ~SfdGenerator();

  // ----------------------  Generator internal, use only if really necessary  --------------
  void GenerateHeader           (const std::string& text, uint32_t what, int32_t lineNo = -1);
  void Generate_NewLine         (void);
  void Generate_EnableDisable   (void);
  void GenerateEndGroup         (void);
  void DoGenerateEndGroup       (sfd::Index item);
  void Generate_EnableDisableText(const std::string& text, const char* txt0, const char* txt1);
  void Generate_OptionSelectText(const std::string& text, uint32_t value);

  // Location
  void  MakeLocationEdit        (const std::string& regName, uint32_t firstBit, uint32_t lastBit, uint32_t accRead, uint32_t accWrite);
  void  MakeLocationObit        (const std::string& regName, uint32_t bitWidth);

  // Interrupts
  void  MakeInterruptItem       (const std::string& text, uint32_t number, const std::string& description);

  // EndGroup Stack
  uint32_t GetItemFromStack     (void);
  void     AddItemToStack       (uint32_t item);

  void CreateItem               (sfd::Index elementIndex);
  void CreateItemEnd            (sfd::Index elementIndex);

  void WriteText                (const std::string& text, bool bNewLine);
  void CreateBeginItem          (const std::string& text, sfd::Index elementIndex);
  void CreateEndItem            (const std::string& text, sfd::Index elementIndex);
  void CreateCItemTextonly      (const std::string& text, uint32_t num);
  void CreateCItem              (const std::string& text, uint32_t num);
  void CreateCItemEnd           (const std::string& text, sfd::Index elementIndex);
  void CreateOBit               (const std::string& text, uint32_t num);
  void CreateOBitNoRange        (const std::string& text, uint32_t num);
  void CreateORange             (const std::string& text, uint32_t num1, uint32_t num2);
  void CreateIBit               (const std::string& text, uint32_t num);
  void CreateIBitAddr           (const std::string& text, uint32_t num1, uint32_t num2);
  void CreateIBitAddrAcc        (const std::string& text, uint32_t num1, uint32_t num2, SvdTypes::Access acc);
  void CreateIRange             (const std::string& text, uint32_t num1, uint32_t num2);
  void CreateIRangeAddr         (const std::string& text, uint32_t num1, uint32_t num2, uint32_t num3);
  void CreateIRangeAddrAcc      (const std::string& text, uint32_t num1, uint32_t num2, uint32_t num3, SvdTypes::Access acc);
  void CreateInfoAddr           (const std::string& text, uint32_t num1);
  void CreateTextonly           (const std::string& text);

  template<uint32_t element, typename ...Args>
  void Generate(const std::string& text, Args&&... args)
  {
    constexpr sfd::Index elementIndex  = (sfd::Index)  (element & sfd::INDEX_MASK   );
    constexpr sfd::Options elementType = (sfd::Options)(element & sfd::OPTIONS_MASK );
    constexpr sfd::Special specialType = (sfd::Special)(element & sfd::SPECIAL_MASK );

    if constexpr (elementType == sfd::MAKE) {
      if constexpr (specialType == sfd::MK_EDITLOC) {
        parse_and_call(&SfdGenerator::MakeLocationEdit, text, std::forward<Args>(args)...);
      }
      else if constexpr (specialType == sfd::MK_OBITLOC) {
        parse_and_call(&SfdGenerator::MakeLocationObit, text, std::forward<Args>(args)...);
      }
      else if constexpr (specialType == sfd::MK_ADDRSTR) {
        parse_and_call(&SfdGenerator::WriteText, text, true, std::forward<Args>(args)...);
      }
      else if constexpr (specialType == sfd::MK_INTERRUPT) {
        parse_and_call(&SfdGenerator::MakeInterruptItem, text, std::forward<Args>(args)...);
      }
      else if constexpr (specialType == sfd::MK_ENABLEDISABLE) {
        parse_and_call(&SfdGenerator::Generate_EnableDisableText, text, std::forward<Args>(args)...);
      }
      else if constexpr (specialType == sfd::MK_OPTSEL) {
        parse_and_call(&SfdGenerator::Generate_OptionSelectText, text, std::forward<Args>(args)...);
      }
    }
    else if constexpr (elementType == sfd::DESCR) {
      if constexpr (specialType == sfd::PART || specialType == sfd::SUBPART || specialType == sfd::HEADER) {
        parse_and_call(&SfdGenerator::GenerateHeader, text, specialType, -1, std::forward<Args>(args)...);
      }
    }
    else if(elementType == sfd::DESCR_LINENO) {
      if constexpr (specialType == sfd::PART || specialType == sfd::SUBPART || specialType == sfd::HEADER) {
        parse_and_call(&SfdGenerator::GenerateHeader, text, specialType, std::forward<Args>(args)...);
      }
    }
    else if constexpr (elementType == sfd::BEGIN) {
      parse_and_call(&SfdGenerator::CreateBeginItem, text, elementIndex, std::forward<Args>(args)...);
    }
    else if constexpr (elementType == sfd::END) {
      parse_and_call(&SfdGenerator::CreateEndItem, text, elementIndex, std::forward<Args>(args)...);
    }
    else if constexpr (elementType == sfd::RAW) {
      Generate_NewLine();
      parse_and_call(&SfdGenerator::WriteText, text, false, std::forward<Args>(args)...);
    }
    else if constexpr (elementType == sfd::DIRECT) {
      parse_and_call(&SfdGenerator::WriteText, text, true, std::forward<Args>(args)...);
    }
    else if constexpr (elementType == sfd::APPENDTEXT) {
      parse_and_call(&SfdGenerator::WriteText, text, false, std::forward<Args>(args)...);
    }
    else if constexpr (elementType == sfd::ENDGROUP) {
      GenerateEndGroup();
    }
    else {
      if constexpr (specialType == sfd::CITEM) {
        m_tabCount++;

        if constexpr (elementType == sfd::ENDIS) {
          Generate_EnableDisable();
        }
        else if constexpr (elementType == sfd::TEXTONLY) {
          parse_and_call(&SfdGenerator::CreateCItemTextonly, text, std::forward<Args>(args)...);
        }
        else {
          parse_and_call(&SfdGenerator::CreateCItem, text, std::forward<Args>(args)...);
        }

        if(m_tabCount) {
          m_tabCount--;
        }
      }
      else if constexpr (specialType == sfd::OBIT) {
        parse_and_call(&SfdGenerator::CreateOBit, text, std::forward<Args>(args)...);
      }
      else if constexpr (specialType == sfd::OBIT_NORANGE) {
        parse_and_call(&SfdGenerator::CreateOBitNoRange, text, std::forward<Args>(args)...);
      }
      else if constexpr (specialType == sfd::ORANGE) {
        parse_and_call(&SfdGenerator::CreateORange, text, std::forward<Args>(args)...);
      }
      else {
        CreateItem(elementIndex);

        if constexpr (elementIndex == sfd::INFO) {
          if constexpr (elementType == sfd::IRANGE_ADDR_ACC) {
            parse_and_call(&SfdGenerator::CreateIRangeAddrAcc, text, std::forward<Args>(args)...);
          }
          else if constexpr (elementType == sfd::IBIT) {
            parse_and_call(&SfdGenerator::CreateIBit, text, std::forward<Args>(args)...);
          }
          else if constexpr (elementType == sfd::IBIT_ADDR) {
            parse_and_call(&SfdGenerator::CreateIBitAddr, text, std::forward<Args>(args)...);
          }
          else if constexpr (elementType == sfd::IBIT_ADDR_ACC) {
            parse_and_call(&SfdGenerator::CreateIBitAddrAcc, text, std::forward<Args>(args)...);
          }
          else if constexpr (elementType == sfd::IRANGE) {
            parse_and_call(&SfdGenerator::CreateIRange, text, std::forward<Args>(args)...);
          }
          else if constexpr (elementType == sfd::IRANGE_ADDR) {
            parse_and_call(&SfdGenerator::CreateIRangeAddr, text, std::forward<Args>(args)...);
          }
          else if constexpr (elementType == sfd::INFO_ADDR) {
            parse_and_call(&SfdGenerator::CreateInfoAddr, text, std::forward<Args>(args)...);
          }
          else {
            parse_and_call(&SfdGenerator::CreateTextonly, text, std::forward<Args>(args)...);
          }
        }
        else {
          parse_and_call(&SfdGenerator::CreateTextonly, text, std::forward<Args>(args)...);
        }

        if constexpr (elementType != sfd::SINGLE) {
          CreateItemEnd(elementIndex);
        }
      }
    }
  }

private:
  FileIo*     m_fileIo;
  uint32_t    m_tabCount;
  int32_t     m_itemStackPointer;
  uint32_t    itemStack[100];

  static const std::string items[];
  static const std::string gen_HEADER_LINE_FILE;
  static const std::string gen_HEADER_LINE_PART;
  static const std::string gen_HEADER_TEXT_FILE;
  static const std::string gen_HEADER_TEXT_PART;
  static const std::string VIEWS_NAME;
  static const std::string TREES_NAME;
  static const std::string NUMBERS_NAME;
  static const std::string NUMBERS_STRING;
  static const std::string GuiEditItems[];
  static const std::string CastItems[];
  static const std::string menuDepthString[];
};

#endif // SfdGenerator_H
