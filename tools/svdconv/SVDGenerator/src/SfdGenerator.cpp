/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdarg.h>
#include "CodeGenerator.h"
#include "SfdGenerator.h"
#include "SvdTypes.h"

using namespace std;
using namespace sfd;


// -------------------------------  GUI Items  ------------------------------------
const string SfdGenerator::GuiEditItems[] = {
  "Gui_u8",
  "Gui_u16",
  "Gui_u32"
};

const string SfdGenerator::CastItems[] = {
  "unsigned char",
  "unsigned short",
  "unsigned int",
  "unsigned int",
  "unsigned long",
};

const string SfdGenerator::menuDepthString[] = {
  "Main Menu",
  "Sub Menu",
};

const string SfdGenerator::gen_HEADER_LINE_FILE =      "************************************************************************************************";
const string SfdGenerator::gen_HEADER_LINE_PART =      "------------------------------------------------------------------------------------------------";
const string SfdGenerator::gen_HEADER_TEXT_FILE =      "*****";
const string SfdGenerator::gen_HEADER_TEXT_PART =      "-----";
const string SfdGenerator::VIEWS_NAME           = "Register Views";
const string SfdGenerator::TREES_NAME           = "Register Trees";
const string SfdGenerator::NUMBERS_NAME         = "Numbers";
const string SfdGenerator::NUMBERS_STRING       = "Num";


// ------------------------------  language basics  --------------------------------------------
const string SfdGenerator::items[] = {
  "",              // EMPTY        
  "m",             // MEMBER       
  "b",             // BLOCK        
  "view",          // VIEW         
  "item",          // ITEM         
  "i",             // INFO         
  "check",         // CHECK        
  "loc",           // LOC          
  "edit",          // EDIT         
  "text",          // TXT          
  "tb",            // TABLE        
  "th",            // THEAD        
  "c",             // TCOLUMN      
  "row",           // TROW         
  "combo",         // COMBO        
  "itree",         // ITREE        
  "rtree",         // RTREE        
  "g",             // GRP          
  "name",          // NAME         
  "r",             // ACC_R        
  "w",             // ACC_W        
  "rw",            // ACC_RW       
  "qitem",         // QITEM        
  "irqtable",      // IRQTABLE     
  "nvicPrioBits",  // NVICPRIOBITS 
  "disableCond",   // DISABLECOND  
  "h",             // HEADING      
  "e",             // HEADINGENABLE
  "o",             // OPTION
  "q",             // Q-OPTION
};                            
                              

SfdGenerator::SfdGenerator(FileIo *fileIo) :
  m_fileIo(fileIo),
  m_tabCount(1),
  m_itemStackPointer(0)
{
  memset(itemStack, 0x00, sizeof(itemStack));
}

SfdGenerator::~SfdGenerator()
{
}

void SfdGenerator::GenerateHeader(const std::string& text, uint32_t what, int32_t lineNo /*= -1*/)
{
  size_t i=0;
  string genLine;
  string genText;
  string outText;

  size_t textLen = text.length();
  size_t genLen  = gen_HEADER_LINE_FILE.length();

  if(what == HEADER) {
    genLine = gen_HEADER_LINE_FILE;
    genText = gen_HEADER_TEXT_FILE;
  } else if(what == PART) {
    genLine = gen_HEADER_LINE_PART;
    genText = gen_HEADER_TEXT_PART;
  } else if(what == SUBPART) {
    genLine = gen_HEADER_LINE_PART;
  }

  outText = "\n\n";

  if((what == HEADER) | (what == PART)) {
    outText += "\n// ";
    outText += genLine;
    outText += "\n// ";
    outText += genText;

    for(i=0; i < ( ((genLen/2)-(textLen/2))-(strlen(genText.c_str())) ); i++) {
      outText += ' ';
    }
    outText += text;

    i += textLen + (2 * genText.length());
    for (; i < genLen; i++) {
      outText += ' ';
    }
    outText += genText;
    outText += "\n// ";
    outText += genLine;
  }
  else if(what == SUBPART) {
    outText += "\n// ";
    int32_t spcLen = (genLen/2)-(textLen/2);
    if(spcLen > 4) {
      spcLen -= 4;
    } else {
      spcLen = 1;
    }

    // --> TODO: delete reproducer for old bug (double append text)
    m_fileIo->WriteText(outText);
    outText.clear();
    // <--

    for(i=0; i<(size_t)spcLen; i++) {                                       // leave two spaces left/right to text
      outText += '-';
    }
    outText += "  ";
    outText += text;
    outText += "  ";

    i+= text.length() + 4;

    // --> TODO: delete reproducer for old bug (double append text)
    if(i >= genLen) {
      m_fileIo->WriteText(outText);
    }
    // <--

    for(; i<genLen; i++) {
      outText += '-';
    }
  }

  if(lineNo != -1) {
    outText += "\n// SVD Line: ";
    outText += to_string(lineNo);
  }

  outText += '\n';
  m_fileIo->WriteText(outText);
}

void SfdGenerator::Generate_NewLine(void)
{
  string outText = "\n//";

  for(uint32_t i=0; i<m_tabCount; i++) {
    for(uint32_t j=0; j<SPACES_PER_TAB; j++) {
      outText += ' ';      
    }
  }

  m_fileIo->WriteText(outText);
}

void SfdGenerator::Generate_EnableDisable(void)
{
  Generate<RAW >("<%i=> %i: %s>", 0, 0, "Disable");
  Generate<RAW >("<%i=> %i: %s>", 1, 1, "Enable");
}

void SfdGenerator::Generate_EnableDisableText(const std::string& text, const char* txt0, const char* txt1)
{
  Generate<RAW >("<%i=> %s", 0, txt0);
  Generate<RAW >("<%i=> %s", 1, txt1);
}

void SfdGenerator::Generate_OptionSelectText(const std::string& text, uint32_t value)
{
  Generate<RAW >("<%i=> %s", value, text);
}


void SfdGenerator::MakeLocationEdit(const std::string& regName, uint32_t firstBit, uint32_t lastBit, uint32_t accRead, uint32_t accWrite)
{
  string guiEditSizeString;
  string castString;
  uint32_t bitWidth = 0;

  bitWidth = lastBit - firstBit + 1;

       if(bitWidth < 9)  { guiEditSizeString = GuiEditItems[GUI_EDIT_ITEM_8];  castString = CastItems[CAST_CHAR];  }
  else if(bitWidth < 17) { guiEditSizeString = GuiEditItems[GUI_EDIT_ITEM_16]; castString = CastItems[CAST_SHORT]; }
  else if(bitWidth < 33) { guiEditSizeString = GuiEditItems[GUI_EDIT_ITEM_32]; castString = CastItems[CAST_INT];   }
  else if(bitWidth < 65) { guiEditSizeString = GuiEditItems[GUI_EDIT_ITEM_32]; castString = CastItems[CAST_INT64]; }
  else                   { guiEditSizeString = GuiEditItems[GUI_EDIT_ITEM_32]; castString = CastItems[CAST_LONG];  }

  if(!accWrite) {
    Generate<LOC             >(LOC_STRING_EDIT3_RO,
                              castString.c_str(), regName.c_str(),    firstBit,          accRead);          // read
  }
  else {
    Generate<LOC             >(LOC_STRING_EDIT3,
                              castString.c_str(), regName.c_str(),    firstBit,          accRead,           // read
                              regName.c_str(),    regName.c_str(),    accWrite,          firstBit,          // mask
                              CastItems[CAST_LONG], guiEditSizeString.c_str(), accWrite, firstBit);         // write
  }
}

void SfdGenerator::MakeLocationObit(const std::string& regName, uint32_t bitWidth)
{
  string guiEditSizeString;
  string castString;
  string name;

       if(bitWidth < 9)  { guiEditSizeString = GuiEditItems[GUI_EDIT_ITEM_8];  castString = CastItems[CAST_CHAR];  }
  else if(bitWidth < 17) { guiEditSizeString = GuiEditItems[GUI_EDIT_ITEM_16]; castString = CastItems[CAST_SHORT]; }
  else                   { guiEditSizeString = GuiEditItems[GUI_EDIT_ITEM_32]; castString = CastItems[CAST_INT];   }

  Generate<LOC               >(LOC_STRING_OBIT, castString.c_str(), regName.c_str());
}

void SfdGenerator::MakeInterruptItem(const std::string& text, uint32_t number, const std::string& description)
{
  Generate<QITEM|BEGIN       >("%s_IRQ",   text.c_str()       );
  Generate<NAME              >("%s",       text.c_str()       );
  Generate<INFO              >("%s",       description.c_str());
  Generate<LOC               >("%d",       number             );
  Generate<QITEM|END         >(""                             );
  Generate_NewLine();
}

// --------------------------------------  Endgroup Stack  -------------------------------------------
void SfdGenerator::DoGenerateEndGroup(Index item)
{
  switch(item) {
    case EMPTY:
      Generate<EMPTY|END          >("");
      break;
    case MEMBER:
      Generate<MEMBER|END         >("");
      break;
    case BLOCK:
      Generate<BLOCK|END          >("");
      break;
    case VIEW:
      Generate<VIEW|END           >("");
      break;
    case ITEM:
      Generate<ITEM|END           >("");
      break;
    case INFO:
      Generate<INFO|END           >("");
      break;
    case CHECK:
      Generate<CHECK|END          >("");
      break;
    case LOC:
      Generate<LOC|END            >("");
      break;
    case EDIT:
      Generate<EDIT|END           >("");
      break;
    case TXT:
      Generate<TXT|END            >("");
      break;
    case TABLE:
      Generate<TABLE|END          >("");
      break;
    case THEAD:
      Generate<THEAD|END          >("");
      break;
    case TCOLUMN:
      Generate<TCOLUMN|END        >("");
      break;
    case TROW:
      Generate<TROW|END           >("");
      break;
    case COMBO:
      Generate<COMBO|END          >("");
      break;
    case ITREE:
      Generate<ITREE|END          >("");
      break;
    case RTREE       :
      Generate<RTREE|END          >("");
      break;
    case GRP:
      Generate<GRP|END            >("");
      break;
    case NAME:
      Generate<NAME|END           >("");
      break;
    case ACC_R:
      Generate<ACC_R|END          >("");
      break;
    case ACC_W:
      Generate<ACC_W|END          >("");
      break;
    case ACC_RW:
      Generate<ACC_RW|END         >("");
      break;
    case QITEM:
      Generate<QITEM|END          >("");
      break;
    case IRQTABLE:
      Generate<IRQTABLE|END       >("");
      break;
    case NVICPRIOBITS:
      Generate<NVICPRIOBITS|END   >("");
      break;
    case DISABLECOND:
      Generate<DISABLECOND|END    >("");
      break;
    default:
      break;
  }
}

void SfdGenerator::GenerateEndGroup(void)
{
  uint32_t item=0;

  do {
    item = GetItemFromStack();
    if(item) {
      AddItemToStack(EMPTY);    // Add a dummy item
      DoGenerateEndGroup((Index)item);
    }
  } while(item);

  Generate<RAW>("");
}

void SfdGenerator::AddItemToStack(uint32_t item)
{
  if(m_itemStackPointer < ITEMSTACK_MAX) {
    itemStack[m_itemStackPointer++] = item;
  }
}

uint32_t SfdGenerator::GetItemFromStack(void)
{
  if(m_itemStackPointer)  {
    if(m_itemStackPointer < 100) {
      return itemStack[--m_itemStackPointer];
    }
  }
  
  return 0;
}


void SfdGenerator::WriteText(const std::string& text, bool bNewLine)
{
  if(bNewLine) {
    m_fileIo->WriteChar('\n');
  }

  m_fileIo->WriteText(text);
}

void SfdGenerator::CreateItem(sfd::Index elementIndex)
{
  Generate<RAW>("<%s>", items[elementIndex].c_str());
}

void SfdGenerator::CreateItemEnd(sfd::Index elementIndex)
{
  Generate<APPENDTEXT>(" </%s>", items[elementIndex].c_str());
}

void SfdGenerator::CreateBeginItem(const std::string& text, sfd::Index elementIndex)
{
  Generate<RAW>("<%s> %s", items[elementIndex].c_str(), text.c_str());
  m_tabCount++;
  AddItemToStack(elementIndex);
}

void SfdGenerator::CreateEndItem(const std::string& text, sfd::Index elementIndex)
{
  if(m_tabCount) {
    m_tabCount--;
  }
  Generate<RAW>("</%s>", items[elementIndex].c_str());
  GetItemFromStack();
}

void SfdGenerator::CreateCItemTextonly(const std::string& text, uint32_t num)
{
  Generate<APPENDTEXT>("<%i=> %s", num, text.c_str());
}

void SfdGenerator::CreateTextonly(const std::string& text)
{
  //PrepareLineBreaks(genTextBuf);
  Generate<APPENDTEXT>(" %s", text.c_str());
}

void SfdGenerator::CreateCItem(const std::string& text, uint32_t num)
{
  //PrepareLineBreaks(genTextBuf);
  Generate<RAW>("<%i=> %i: %s", num, num, text.c_str());
}

void SfdGenerator::CreateCItemEnd(const std::string& text, sfd::Index elementIndex)
{
  Generate<RAW>(" </%s>", items[elementIndex].c_str());
}

void SfdGenerator::CreateOBit(const std::string& text, uint32_t num)
{
  //PrepareLineBreaks(genTextBuf);
  Generate<RAW>("<o.%i..%i> %s", num, num, text.c_str());
}

void SfdGenerator::CreateOBitNoRange(const std::string& text, uint32_t num)
{
  //PrepareLineBreaks(genTextBuf);
  Generate<RAW>("<o.%i> %s", num, text.c_str());
}

void SfdGenerator::CreateORange(const std::string& text, uint32_t num1, uint32_t num2)
{
  //PrepareLineBreaks(genTextBuf);
  Generate<RAW>("<o.%i..%i> %s", num1, num2, text.c_str());
}

void SfdGenerator::CreateIBit(const std::string& text, uint32_t num)
{
  Generate<APPENDTEXT>(" [Bit %i] %s", num, text.c_str());
}

void SfdGenerator::CreateIBitAddr(const std::string& text, uint32_t num1, uint32_t num2)
{
  //PrepareLineBreaks(genTextBuf);
  Generate<APPENDTEXT>(" [Bit %i] (@ 0x%08X) %s", num1, num2, text.c_str());
}

void SfdGenerator::CreateIBitAddrAcc(const std::string& text, uint32_t num1, uint32_t num2, SvdTypes::Access acc)
{
  //PrepareLineBreaks(genTextBuf);
  const std::string &accStr = SvdTypes::GetAccessTypeSfd(acc);
  Generate<APPENDTEXT>(" [Bit %i] %s (@ 0x%08X) %s", num1, accStr.c_str(), num2, text.c_str());
}

void SfdGenerator::CreateIRange(const std::string& text, uint32_t num1, uint32_t num2)
{
  //PrepareLineBreaks(genTextBuf);
  Generate<APPENDTEXT>(" [Bits %i..%i] %s", num1, num2, text.c_str());
}

void SfdGenerator::CreateIRangeAddr(const std::string& text, uint32_t num1, uint32_t num2, uint32_t num3)
{
  //PrepareLineBreaks(genTextBuf);
  Generate<APPENDTEXT>(" [Bits %i..%i] (@ 0x%08X) %s", num1, num2, num3, text.c_str());
}

void SfdGenerator::CreateIRangeAddrAcc(const std::string& text, uint32_t num1, uint32_t num2, uint32_t num3, SvdTypes::Access acc)
{
  //PrepareLineBreaks(genTextBuf);
  const std::string &accStr = SvdTypes::GetAccessTypeSfd(acc);
  Generate<APPENDTEXT>(" [Bits %i..%i] %s (@ 0x%08X) %s", num1, num2, accStr.c_str(), num3, text.c_str());
}

void SfdGenerator::CreateInfoAddr(const std::string& text, uint32_t num1)
{
  //PrepareLineBreaks(genTextBuf);
  Generate<APPENDTEXT>("(@ 0x%08X) %s", num1, text.c_str());
}
