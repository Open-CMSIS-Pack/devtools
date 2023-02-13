/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SvdGenerator.h"
#include "HeaderGenerator.h"
#include "SvdUtils.h"
#include "SvdTypes.h"
#include "FileIo.h"

#include "ErrLog.h"

#include <string>
#include <set>
#include <list>
#include <map>
using namespace std;
using namespace c_header;


#define HEADER_INLINE_COMMENT_LEN         (120+6+2) // new 150: max len for inline comments (len + "/*!< ")
#define HEADER_COMMENT_WIDTH              (HEADER_INLINE_COMMENT_LEN - 3-2)  //80  // width for comments in newline
#define HEADER_INCOMMENT_WIDTH             16     // width for chars that begin/end a comment box
#define HEADER_INLINE_COMMENT_DISTANCE     50     // distants where inline comment starts
#define HEADER_STRUCTUNION_OFFSET          30     // distance of ": 0" Bit position
#define HEADER_PERI_OFFSET                 38     // distance of second expression in peri mem map and peri decl
#define HEADER_PERI_BASEOFFSET             64     // distance of third expression in peri decl
#define HEADER_IRQLIST_OFFSET              HEADER_STRUCTUNION_OFFSET
#define HEADER_POSMAK_VAL_OFFSET           44
#define HEADER_POSMAK_COMMENT_OFFSET      100
#define HEADER_POSMAK_COMMENTEND_OFFSET   144
#define HEADER_POSMAK_COMMENT_OFFSET2     (100-30)
#define HEADER_POSMAK_COMMENTEND_OFFSET2  HEADER_INLINE_COMMENT_LEN   //(144-30+20)

const string HeaderGenerator::m_definePosStr = "_Pos";
const string HeaderGenerator::m_defineMaskStr = "_Msk";
const string HeaderGenerator::m_headerCommentBegin = "/* ";
const string HeaderGenerator::m_headerCommentEnd = " */";
const char HeaderGenerator::m_headerCommentPartChar = '=';
const char HeaderGenerator::m_headerCommentSubPartChar = '=';

const string HeaderGenerator::m_itemsH[] = {
  "",       // EMPTY
  "",       // DOXY_COMMENT       / keep empty for DOXY_COMMENT
  "//",     // C_COMMENT
  "union",  // UNION
  "struct", // STRUCT
  "enum",   // ENUM
  "",       // C_ERROR
  "",       // C_WARNING
  "",       // DOXY_COMMENT_POSMSK
  "",       // DOXY_COMMENTSTAR
};

const string HeaderGenerator::m_langAddH[] = {
  "",
  "typedef",
  "",
  "",
  "",
};


HeaderGenerator::HeaderGenerator(FileIo *fileIo):
  m_fileIo(fileIo),
  m_prevOpenedStructUnion(false),
  m_prevClosedStructUnion(false),
  m_misraCompliantStruct(false),
  m_debugHeaderfile(false),
  m_extraSpaces(0),
  m_tabCount(0),
  m_bracketBegin(0),
  m_structUnionCnt(0)
{
}

HeaderGenerator::~HeaderGenerator()
{
}

void HeaderGenerator::GenerateFileDescription(const std::string& fileName)
{
}

void HeaderGenerator::WriteText(const std::string& text)
{
  m_fileIo->WriteText(text);
}

void HeaderGenerator::GenerateHeader(const std::string& text, Special what)
{
  size_t i=0, len;
  char  commentChar=0;
  char fillSpaces=0;

  size_t textLen = text.length();

  if(what == HEADER) {
    commentChar = m_headerCommentPartChar;
  } else if(what == PART){
    commentChar = m_headerCommentSubPartChar;
  } else if(what == SUBPART){
    commentChar = m_headerCommentSubPartChar;
  }

  if(what != SUBPART){
    Generate<DIRECT>("\n");
  }

  if((what == HEADER) | (what == PART)) {
    // Top line
    Generate<DIRECT>(m_headerCommentBegin);

    for(i=0; i<HEADER_COMMENT_WIDTH; i++) {
      Generate<RAW>("%c", commentChar);
    }

    Generate<RAW>("%s\n", m_headerCommentEnd.c_str());
  }
  else if(what == SUBPART) {
    Generate<RAW>("\n");
    fillSpaces = 1;      // fill spaces with comment char
  }

  // Text Line
  Generate<RAW>(m_headerCommentBegin);

  for(i=0; i<HEADER_INCOMMENT_WIDTH; i++) {
    Generate<RAW>("%c", commentChar);
  }

  len = (int32_t)( (((int32_t)HEADER_COMMENT_WIDTH/2)-(textLen/2)) - HEADER_INCOMMENT_WIDTH);
  for(i=0; i<len; i++) {                                                                        // print spaces or commentChar
    Generate<RAW>("%c", (fillSpaces && i<(len-2))? commentChar : ' ');
  }

  Generate<RAW>(text);

  i += textLen + (2*HEADER_INCOMMENT_WIDTH);
  len = i;
  for(; i<HEADER_COMMENT_WIDTH; i++) {                                                         // print spaces or commentChar
    Generate<RAW>("%c", (fillSpaces && !(i<len+2))? commentChar : ' ');
  }

  for(i=0; i<HEADER_INCOMMENT_WIDTH; i++) {
    Generate<RAW>("%c", commentChar);
  }

  Generate<RAW>(m_headerCommentEnd);

  if((what == HEADER) | (what == PART)) {
    // Bottom line
    Generate<RAW>("\n%s", m_headerCommentBegin.c_str());

    for(i=0; i<HEADER_COMMENT_WIDTH; i++) {
      Generate<RAW>("%c", commentChar);
    }

    Generate<RAW>("%s\n", m_headerCommentEnd.c_str());
  }
}

void HeaderGenerator::GenerateH_CRTabOffset(uint32_t howMany)
{
  uint32_t tabs = 0;

  Generate<RAW>("\r");

  tabs += howMany / SPACES_PER_TAB_FIO;
  for(uint32_t i=0; i<tabs; i++) {
    Generate<RAW>("\t");
  }

  uint32_t extra = howMany % SPACES_PER_TAB_FIO;
  for(uint32_t i=0; i<extra; i++) {
    Generate<RAW>(" ");
  }
}

void HeaderGenerator::Generate_NewLine(void)
{
  Generate<RAW>("\n");
  for(uint32_t i=0; i<m_tabCount; i++) {
    for(uint32_t j=0; j<SPACES_PER_TAB_FIO; j++) {
      Generate<RAW>(" ");
    }
  }
}

void HeaderGenerator::MakeFile(const std::string& name)
{
  // Generate File
  m_fileIo->Close();
  if(!m_fileIo->Create(name)) {
    return;
  }

  Generate<DESCR  >("%s", name);
  Generate<RAW    >("\n");
}

void HeaderGenerator::MakeCMSISConfig(const std::string& text, const std::string& MCUName, CmsisCfg cmsisCfg)
{
  const string& cpuName = SvdTypes::GetCpuName(cmsisCfg.cpuType);
  const string& cpuType = SvdTypes::GetCpuType(cmsisCfg.cpuType);
  const CpuFeature& cpuFeatures = SvdTypes::GetCpuFeatures(cmsisCfg.cpuType);
  string cpuTypeDefine = cpuType;
  string cpuTypeInclude = cpuType;
  SvdUtils::ToUpper(cpuTypeDefine);
  SvdUtils::ToLower(cpuTypeInclude);

  Generate_NewLine();
  Generate<DESCR|HEADER         >("Processor and Core Peripheral Section");
  Generate<DESCR|SUBPART        >("Configuration of the %s Processor and Core Peripherals",   cpuName);

  Generate<DIRECT               >("#define __%s_REV                 0x%04xU",                 cpuTypeDefine, cmsisCfg.cpuRevision);
  Generate<MAKE|MK_DOXY_COMMENT >("%s Core Revision",                                         cpuType);

  Generate<MAKE|MK_DEFINE       >("__NVIC_PRIO_BITS", cmsisCfg.nvicPrioBits, 10, 41, "Number of Bits used for Priority Levels");
  Generate<MAKE|MK_DEFINE       >("__Vendor_SysTickConfig", cmsisCfg.vendorSystickConfig, 10, 41, "Set to 1 if different SysTick Config is used");

  if(cpuFeatures.VTOR || cmsisCfg.forceGeneration.bVtorPresent) {
    Generate<MAKE|MK_DEFINE     >("__VTOR_PRESENT", cmsisCfg.vtorPresent, 10, 41, "Set to 1 if CPU supports Vector Table Offset Register");
  }
  if(cpuFeatures.MPU || cmsisCfg.forceGeneration.bMpuPresent) {
    Generate<MAKE|MK_DEFINE     >("__MPU_PRESENT", cmsisCfg.mpuPresent, 10, 41, "MPU present");
  }
  if(cpuFeatures.FPU || cmsisCfg.forceGeneration.bFpuPresent) {
    Generate<MAKE|MK_DEFINE     >("__FPU_PRESENT", cmsisCfg.fpuPresent? 1 : 0, 10, 41, "FPU present");
  }
  if(cpuFeatures.FPUDP || cmsisCfg.forceGeneration.bFpuDP) {
    Generate<MAKE|MK_DEFINE     >("__FPU_DP", cmsisCfg.fpuDP, 10, 41, cmsisCfg.fpuPresent? "Double Precision FPU" : "unused, Device has no FPU");
  }
  if(cpuFeatures.DSP || cmsisCfg.forceGeneration.bDspPresent) {
    Generate<MAKE|MK_DEFINE     >("__DSP_PRESENT", cmsisCfg.dspPresent? 1 : 0, 10, 41, "DSP extension present");
  }
  if(cpuFeatures.ICACHE || cmsisCfg.forceGeneration.bIcachePresent) {
    Generate<MAKE|MK_DEFINE     >("__ICACHE_PRESENT", cmsisCfg.icachePresent, 10, 41, "Instruction Cache present");
  }
  if(cpuFeatures.DCACHE || cmsisCfg.forceGeneration.bDcachePresent) {
    Generate<MAKE|MK_DEFINE     >("__DCACHE_PRESENT", cmsisCfg.dcachePresent, 10, 41, "Data Cache present");
  }
  if(cpuFeatures.ITCM || cmsisCfg.forceGeneration.bItcmPresent) {
    Generate<MAKE|MK_DEFINE     >("__ITCM_PRESENT", cmsisCfg.itcmPresent, 10, 41, "Instruction TCM present");
  }
  if(cpuFeatures.DTCM || cmsisCfg.forceGeneration.bDtcmPresent) {
    Generate<MAKE|MK_DEFINE     >("__DTCM_PRESENT", cmsisCfg.dtcmPresent, 10, 41, "Data TCM present");
  }
  if(cpuFeatures.SAU || cmsisCfg.forceGeneration.bSauPresent) {
    Generate<MAKE|MK_DEFINE     >("__SAUREGION_PRESENT", cmsisCfg.sauPresent? 1 : 0, 10, 41, "SAU region present");
  }
  if(cpuFeatures.PMU || cmsisCfg.forceGeneration.bPmuPresent) {
    Generate<MAKE|MK_DEFINE     >("__PMU_PRESENT", cmsisCfg.pmuPresent? 1 : 0, 10, 41, "PMU present");
    Generate<MAKE|MK_DEFINE     >("__PMU_NUM_EVENTCNT", cmsisCfg.pmuNumEventCnt, 10, 41, "PMU Event Counters");
  }
  if(cpuFeatures.MVE || cmsisCfg.forceGeneration.bMvePresent) {
    Generate<MAKE|MK_DEFINE     >("__MVE_PRESENT", cmsisCfg.mvePresent? 1 : 0, 10, 41, "MVE region present");
    Generate<MAKE|MK_DEFINE     >("__MVE_FP", cmsisCfg.mveFP, 10, 41, cmsisCfg.mveFP? "Floating Point MVE" : "Integer MVE");
  }

  Generate_NewLine();
  Generate<MAKE|MK_DOXYENDGROUP >(text);
  Generate_NewLine();

  Generate<MAKE|MK_INCLUDE_CORE >(cpuTypeInclude, cpuName);
  Generate<MAKE|MK_INCLUDE_SYSTEM>(MCUName, MCUName);
  Generate_NewLine();

  const string comment = "Fallback for older CMSIS versions";
  Generate<MAKE|MK_IFNDEF       >("__IM", comment);
  Generate<MAKE|MK_DEFINE_TEXT  >("__IM", "__I");
  Generate<MAKE|MK_ENDIF        >("");

  Generate<MAKE|MK_IFNDEF       >("__OM", comment);
  Generate<MAKE|MK_DEFINE_TEXT  >("__OM", "__O");
  Generate<MAKE|MK_ENDIF        >("");

  Generate<MAKE|MK_IFNDEF       >("__IOM", comment);
  Generate<MAKE|MK_DEFINE_TEXT  >("__IOM", "__IO");
  Generate<MAKE|MK_ENDIF        >("");
}

void HeaderGenerator::MakeRegisterStruct(const std::string& text, SvdTypes::Access accessType, const std::string& dataType, uint32_t size)
{
  const std::string& ioType = SvdTypes::GetAccessTypeIo(accessType);
  Generate<DIRECT>("%s %s \r\t\t\t\t\t\t\t\t\t\t%s;", ioType, dataType, text.c_str());
}

void HeaderGenerator::MakeFieldStruct(const std::string& text, SvdTypes::Access accessType, const std::string& dataType, uint32_t size, uint32_t bitWidth)
{
  const std::string& ioType = SvdTypes::GetAccessTypeIo(accessType);
  Generate<DIRECT>("%s %s \r\t\t\t\t\t\t\t\t\t\t%s \r\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t: %d;", ioType, dataType, text.c_str(), bitWidth);
}

void HeaderGenerator::MakeFieldUnion(const std::string& text, SvdTypes::Access accessType, const std::string& dataType, uint32_t size)
{
  const std::string& ioType = SvdTypes::GetAccessTypeIo(accessType);
  Generate<DIRECT>("%s %s \r\t\t\t\t\t\t\t\t\t\t%s", ioType, dataType, text.c_str());
  GenerateH_CRTabOffset(HEADER_STRUCTUNION_OFFSET);
  Generate<RAW>(": %2i;", size);
}

void HeaderGenerator::MakeAnnonUnionCompiler(const std::string& text, bool bStart)
{
  if(m_misraCompliantStruct) {
    return;
  }

  Generate<DIRECT>("\n");
  Generate<DESCR|SUBPART>("%s of section using anonymous unions", bStart? "Start" : "End");
  Generate<DIRECT>(text);
}

void HeaderGenerator::MakeInclude(const std::string& text)
{
  Generate<DIRECT>("#include \"%s\"", text.c_str());
}

void  HeaderGenerator::MakeIncludeCore(const std::string& text, const std::string& cpuName)
{
  Generate<MAKE|MK_INCLUDE>("core_%s.h",                          text.c_str());
  Generate<MAKE|MK_DOXY_COMMENT>("%s processor and core peripherals",  cpuName.c_str());
}

void  HeaderGenerator::MakeIncludeSystem(const std::string& text, const std::string& mcuName)
{
  Generate<MAKE|MK_INCLUDE>("system_%s.h",                        text.c_str());
  Generate<MAKE|MK_DOXY_COMMENT>("%s System",                          mcuName.c_str());
}

void HeaderGenerator::MakePeripheralMapping(const std::string& periName, uint32_t baseAddress)
{
  Generate<DESCR|SUBPART>("%s", periName);
  Generate<DIRECT>("#define %s_BASE", periName);

  GenerateH_CRTabOffset(HEADER_PERI_OFFSET);

  Generate<RAW>("(0x%08XUL)", baseAddress);
  Generate<DIRECT>("#define %s", periName);

  GenerateH_CRTabOffset(HEADER_PERI_OFFSET);

  Generate<RAW>("((%s_Type", periName);

  GenerateH_CRTabOffset(HEADER_PERI_BASEOFFSET);

  Generate<RAW>("*) %s_BASE)", periName);
}

void HeaderGenerator::MakePeripheralAddressDefine(const std::string& periName, uint32_t baseAddress, const std::string& prefix)
{
  Generate<DIRECT>("#define %s%s_BASE ", prefix, periName);
  GenerateH_CRTabOffset(HEADER_PERI_OFFSET);
  Generate<RAW>("0x%08XUL", baseAddress);
}

void HeaderGenerator::MakePeripheralAddressMapping(const std::string& periName, uint32_t baseAddress, const std::string& typeName, const std::string& prefix)
{
  Generate<DIRECT>("#define %s%s ", prefix, periName);
  GenerateH_CRTabOffset(HEADER_PERI_OFFSET);
  Generate<RAW>("((%s_Type*) ", typeName);
  GenerateH_CRTabOffset(HEADER_PERI_BASEOFFSET);
  Generate<RAW>(" %s%s_BASE)", prefix, periName);
}

void HeaderGenerator::MakePeripheralArrayAddressMapping(const std::string& periName, uint32_t baseAddress, const std::string& typeName, const std::string& prefix)
{
  Generate<DIRECT>( "#define %s%s ", prefix, periName);
  GenerateH_CRTabOffset(HEADER_PERI_OFFSET);
  Generate<RAW>( "((%s%s_ARRAYType*) ", prefix, typeName);
  GenerateH_CRTabOffset(HEADER_PERI_BASEOFFSET);
  Generate<RAW>(" %s%s_BASE)", prefix, periName);
}

void HeaderGenerator::MakeEnumValue(const std::string& enumName, uint32_t val, const std::string& name, const std::string& descr)
{
  Generate<DIRECT>("%s\r\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t = %d,", enumName, val);
  Generate<DOXY_COMMENT>("%s : %s", name, descr);
}

void HeaderGenerator::MakeDefine(const std::string& name, uint32_t val, uint32_t base, uint32_t tabOffs, const std::string& descr)
{
  Generate<DIRECT>("#define %s", name);
  if(tabOffs == (uint32_t)-1) {
    tabOffs = HEADER_PERI_OFFSET;
  }
  GenerateH_CRTabOffset(tabOffs);

  if(base == 16) {
    Generate<RAW>("0x%08x", val);
  } else {
    Generate<RAW>("%d", val);
  }

  if(!descr.empty()) {
    Generate<DOXY_COMMENT>("%s", descr);
  }
}

void HeaderGenerator::MakeEnumStructUnionEnd(const std::string& textBuf, Index elementIndex, Special specialType, Additional langAdditional)
{
  if(specialType == ANON) {
    Generate<RAW>(";");
  }
  else if(langAdditional == TYPEDEF) {
    if(elementIndex == ENUM) {
      Generate<RAW>(" %s_Enum;", textBuf);
    }
    else {
      Generate<RAW>(" %s_Type;", textBuf);
    }
  }
  else {
    Generate<RAW>(" %s;", textBuf);
  }

  if(m_prevOpenedStructUnion) {
    //Generate_NewLine();
  }
  if(m_bracketBegin) {
    //Generate_NewLine();
  }

  m_prevOpenedStructUnion = false;
  m_prevClosedStructUnion = true;

  if(m_structUnionCnt) {
    m_structUnionCnt--;
  }

  if(m_misraCompliantStruct && specialType == ANON) {
    Generate<C_ERROR>("ANSI C does not allow anonymous struct/union. See generated C Headerfile for details.", -1);
  }
}

void HeaderGenerator::MakeEnumStructUnionEndArray(const std::string& textBuf, Index elementIndex, Special specialType, Additional langAdditional, uint32_t num)
{
  if(specialType == ANON) {
    Generate<RAW>(";");
  }
  else if(langAdditional == TYPEDEF) {
    if(elementIndex == ENUM) {
      Generate<RAW>(" %s_Enum;", textBuf);
    }
    else {
      Generate<RAW>(" %s_Type[%d];", textBuf, num);
    }
  }
  else {
    Generate<RAW>(" %s;", textBuf);
  }

  if(m_prevOpenedStructUnion) {
    //Generate_NewLine();
  }
  if(m_bracketBegin) {
    //Generate_NewLine();
  }

  m_prevOpenedStructUnion = false;
  m_prevClosedStructUnion = true;

  if(m_structUnionCnt) {
    m_structUnionCnt--;
  }

  if(m_misraCompliantStruct && specialType == ANON) {
    Generate<C_ERROR>("ANSI C does not allow anonymous struct/union. See generated C Headerfile for details.", -1);
  }

  Generate<RAW>(textBuf);
}

void HeaderGenerator::MakeHeaderIfDef(const std::string& name, bool begin)
{
  if(begin) {
    Generate<DIRECT>("#ifndef %s_H", name);
    Generate<DIRECT>("#define %s_H", name);
  }
  else {
    Generate<DIRECT>("#endif /* %s_H */", name);
    Generate<DIRECT>("");
  }
}

void HeaderGenerator::MakeHeaderExternC(const std::string& name, bool begin)
{
  Generate<DIRECT>("");

  if(begin) {
    Generate<DIRECT>("#ifdef __cplusplus");
    Generate<DIRECT>("  extern \"C\" {");
    Generate<DIRECT>("#endif");
  }
  else {
    Generate<DIRECT>("#ifdef __cplusplus");
    Generate<DIRECT>("  }");
    Generate<DIRECT>("#endif");
  }

  Generate<DIRECT>("");
}

void HeaderGenerator::CreateDoxyCommentLineBreak(const std::string& text)
{
  size_t lineLen=0;
  size_t headerInlineCommentDistance = HEADER_INLINE_COMMENT_DISTANCE;
  char cPrev=0;
  string outText;

  Generate<RAW>("/*!< ");
  bool firstLine = true;

  for(auto c : text) {
    if(c == ' ' && c == cPrev) {
      continue;
    }
    if(c == '\n' || c=='\r' /*|| c=='\t'*/) {   // TABs are used by the generator ...
      continue;
    }
    if(!lineLen && c==' ') {
      continue;
    }
    if(cPrev == '\\' && c == 'n') {
      if(outText.length()) {
        outText.pop_back();
      }
    }

    if( (cPrev == '\\' && c == 'n') || \
      (  lineLen >  (HEADER_INLINE_COMMENT_LEN - headerInlineCommentDistance - 20) && c==' ') /* break on SPACE or */ \
      || lineLen >= (HEADER_INLINE_COMMENT_LEN - headerInlineCommentDistance)                 /* hard break Word :-( */ \
    ) {
      m_fileIo->WriteText(outText);

      if(firstLine) {
        headerInlineCommentDistance += strlen("/*!< ");
        headerInlineCommentDistance += m_extraSpaces;
        m_extraSpaces = 0;
        firstLine = false;
      }

      Generate_NewLine();
      GenerateH_CRTabOffset((unsigned)headerInlineCommentDistance);
      outText.clear();
      lineLen = 0;
      continue;
    }

    outText += c;
    lineLen++;
    cPrev = c;
  }

  m_fileIo->WriteText(outText);    // left m_fileIo here because text can contain % (e.g. "uses 80% of x").}
}

void HeaderGenerator::MakeIfNDef(const std::string& text, const std::string& descr)
{
  Generate<DIRECT>("#ifndef %s", text.c_str());

  if(!descr.empty()) {
    Generate<MAKE|MK_DOXY_COMMENT>(descr);
  }

  m_tabCount++;
}

void HeaderGenerator::MakeIfNDefHeader(const std::string& text, const std::string& descr)
{
  Generate<DIRECT>("#ifndef %s_h", text.c_str());

  if(!descr.empty()) {
    Generate<MAKE|MK_DOXY_COMMENT>(descr);
  }
}

void HeaderGenerator::MakeEndIf(const std::string& text)
{
  m_tabCount--;

  if(text.length()) {
    Generate<DIRECT>("#endif /* %s */", text.c_str());
  }
  else {
    Generate<DIRECT>("#endif");
  }
}

void HeaderGenerator::MakeDefineText(const std::string& text, const std::string& repl)
{
  Generate_NewLine();
  Generate<RAW>("#define %s\r\t\t\t\t\t\t\t\t\t %s", text.c_str(), repl.c_str());
}

void HeaderGenerator::MakeCPlusPlus(const std::string& text, bool bStart)
{

  if(bStart) {
    Generate<DIRECT>("");
    Generate<DIRECT>("#ifdef __cplusplus\nextern \"C\" {\n#endif");
  }
  else {
    Generate<DIRECT>("\n");
    Generate<DIRECT>("#ifdef __cplusplus\n}\n#endif");
  }

  if(!text.empty()) {
    Generate<MAKE|MK_DOXY_COMMENT>(text);
  }

  Generate<DIRECT>("");
}

void HeaderGenerator::CreateDoxyComment(const std::string& text)
{
  Generate<RAW>("/*!< %s", text.c_str());
}

void HeaderGenerator::CreateDoxyCommentStar(const std::string& text)
{
  Generate<RAW>("/** %s", text.c_str());
}

void HeaderGenerator::CreateCComment(const std::string& text)
{
  Generate_NewLine();
  Generate<RAW>("// %s", text.c_str());
}

void HeaderGenerator::CreateCCommentBegin (const std::string& text)
{
  if(text.empty()) {
    Generate_NewLine();
    Generate<RAW>("/*");
  }
  else {
    Generate<RAW>(" /* %s", text.c_str());
  }
}

void HeaderGenerator::CreateCCommentEnd (const std::string& text)
{
  if(text.empty()) {
    Generate_NewLine();
    Generate<RAW>("*/");
  }
  else {
    Generate<RAW>("%s */", text.c_str());
  }
}

void HeaderGenerator::CreateCError (const std::string& text, uint32_t lineNo)
{
  Generate<DIRECT>("#error \"%s\"", text.c_str());
  LogMsg("M219", MSG(text), lineNo);
}

void HeaderGenerator::CreateCWarning (const std::string& text, uint32_t lineNo)
{
  Generate<DIRECT>("#warning \"%s\"", text.c_str());
  LogMsg("M220", MSG(text), lineNo);
}

void HeaderGenerator::MakeDoxyCommentAddress(const std::string& text, uint32_t address)
{
  if(m_debugHeaderfile) {
    string t = "(@ ";
    t += SvdUtils::CreateHexNum(address, 8);
    t += ") ";
    m_extraSpaces = (uint32_t)strlen("(@ ) ") + t.length();
    t+= text;
    MakeDoxyComment(t);
  }
  else {
    MakeDoxyComment(text);
  }
}

void HeaderGenerator::MakeDoxyCommentNumber(const std::string& text, uint32_t number)
{
  if(m_debugHeaderfile) {
    string t = "(@ ";
    t += SvdUtils::CreateHexNum(number, 8);
    t += ") ";
    m_extraSpaces = (uint32_t)strlen("/*!< ") + t.length();
    t += text;
    MakeDoxyComment(t);
  }
  else {
    MakeDoxyComment(text);
  }
}

void HeaderGenerator::MakeDoxyComment(const std::string& text)
{
  GenerateH_CRTabOffset(HEADER_INLINE_COMMENT_DISTANCE);
  Generate<DOXY_COMMENT|BEGIN>("%s", text.c_str());
  GenerateH_CRTabOffset(HEADER_INLINE_COMMENT_LEN);
  Generate<DOXY_COMMENT|END>("");
}

void HeaderGenerator::MakeDoxyCommentBitField(const std::string& text, uint32_t firstBit, uint32_t lastBit)
{
  uint32_t extraSpaces = m_extraSpaces;

  m_extraSpaces = 0;
  GenerateH_CRTabOffset(HEADER_INLINE_COMMENT_DISTANCE);

  if(m_debugHeaderfile) {
    Generate<DOXY_COMMENT|BEGIN>("[%i..%i] %s", lastBit, firstBit, text.c_str());
  }
  else {
    Generate<DOXY_COMMENT|BEGIN>("%s", text.c_str());
  }
  GenerateH_CRTabOffset(HEADER_INLINE_COMMENT_LEN);
  Generate<DOXY_COMMENT|END>("");

  m_extraSpaces = extraSpaces;
}

void HeaderGenerator::MakeDoxyCommentBitPos(const std::string& text, uint32_t pos)
{
  uint32_t extraSpaces = m_extraSpaces;
  m_extraSpaces = 0;

  GenerateH_CRTabOffset(HEADER_INLINE_COMMENT_DISTANCE);

  if(m_debugHeaderfile) {
    Generate<DOXY_COMMENT|BEGIN>("[%i] %s", pos, text.c_str());
  }
  else {
    Generate<DOXY_COMMENT|BEGIN>("%s", text.c_str());
  }

  GenerateH_CRTabOffset(HEADER_INLINE_COMMENT_LEN);
  Generate<DOXY_COMMENT|END>("");

  m_extraSpaces = extraSpaces;
}

void HeaderGenerator::MakeInterruptStruct(const std::string& text, int32_t number, bool lastEnum)
{
  Generate<DIRECT>("%s", text.c_str());
  GenerateH_CRTabOffset(HEADER_IRQLIST_OFFSET);
  Generate<RAW>("= %3i%s", number, lastEnum? "" : ",");
}

void HeaderGenerator::MakeFieldPosMask(const std::string& field, const std::string& peri, const std::string& reg, uint32_t pos, uint32_t mask)
{
  Generate<DIRECT        >("#define %s_%s_%s%s ", peri, reg, field, m_definePosStr);
  GenerateH_CRTabOffset(HEADER_POSMAK_VAL_OFFSET);
  Generate<RAW           >("%i", pos);

  // Doxy Comment
  GenerateH_CRTabOffset(HEADER_POSMAK_COMMENT_OFFSET);
  Generate<DOXY_COMMENT|BEGIN>("%s %s: %s Position", peri, reg, field);
  GenerateH_CRTabOffset(HEADER_POSMAK_COMMENTEND_OFFSET);
  Generate<DOXY_COMMENT|END>("");

  if(mask < 0xff)  {
    Generate<DIRECT>("#define %s_%s_%s%s ", peri, reg, field, m_defineMaskStr);
    GenerateH_CRTabOffset(HEADER_POSMAK_VAL_OFFSET);
    Generate<RAW>("(0x%02xUL << %s_%s_%s%s)", mask, peri, reg, field, m_definePosStr);
  }
  else if(mask < 0xffff)  {
    Generate<DIRECT>("#define %s_%s_%s%s ", peri, reg, field, m_defineMaskStr);
    GenerateH_CRTabOffset(HEADER_POSMAK_VAL_OFFSET);
    Generate<RAW>("(0x%04xUL << %s_%s_%s%s)", mask, peri, reg, field, m_definePosStr);
  }
  else {
    Generate<DIRECT>("#define %s_%s_%s%s ", peri, reg, field, m_defineMaskStr);
    GenerateH_CRTabOffset(HEADER_POSMAK_VAL_OFFSET);
    Generate<RAW>("(0x%08xUL << %s_%s_%s%s)", mask, peri, reg, field, m_definePosStr);
  }

  // Doxy Comment
  GenerateH_CRTabOffset(HEADER_POSMAK_COMMENT_OFFSET);
  Generate<DOXY_COMMENT|BEGIN>("%s %s: %s Mask", peri, reg, field);
  GenerateH_CRTabOffset(HEADER_POSMAK_COMMENTEND_OFFSET);
  Generate<DOXY_COMMENT|END>("");

  if(!(mask % 2)) {
    Generate<DOXY_COMMENT>("Even number for MASK detected!");
    LogMsg("M220", MSG("Even number for MASK detected!"));
  }
}

void HeaderGenerator::MakeFieldPosMask2(const std::string& field, const std::string& peri, const std::string& reg, uint32_t pos, uint32_t mask)
{
  uint32_t calcMask;

  Generate<DIRECT>("#define %s_%s_%s%s ", peri, reg, field, m_definePosStr);
  GenerateH_CRTabOffset(HEADER_POSMAK_VAL_OFFSET);
  Generate<RAW>("(%iUL) ", pos);

  // Doxy Comment
  GenerateH_CRTabOffset(HEADER_POSMAK_COMMENT_OFFSET2);
  Generate<DOXY_COMMENT_POSMSK|BEGIN>("%s %s: %s (Bit %i)", peri, reg, field, pos);
  GenerateH_CRTabOffset(HEADER_POSMAK_COMMENTEND_OFFSET2);
  Generate<DOXY_COMMENT|END>("");

  calcMask = mask << pos;
  Generate<DIRECT>("#define %s_%s_%s%s ", peri, reg, field, m_defineMaskStr);
  GenerateH_CRTabOffset(HEADER_POSMAK_VAL_OFFSET);
  Generate<RAW>("(0x%xUL) ", calcMask);

  // Doxy Comment
  GenerateH_CRTabOffset(HEADER_POSMAK_COMMENT_OFFSET2);
  Generate<DOXY_COMMENT_POSMSK|BEGIN>("%s %s: %s (Bitfield-Mask: 0x%02x)", peri, reg, field, mask);
  GenerateH_CRTabOffset(HEADER_POSMAK_COMMENTEND_OFFSET2);
  Generate<DOXY_COMMENT|END>("");

  if(!(mask % 2)) {
    Generate<DOXY_COMMENT_POSMSK>("Even number for MASK detected!");
    LogMsg("M220", MSG("Even number for MASK detected!"));
  }
}

void HeaderGenerator::MakeFieldPosMask3(const std::string& pName, const std::string& field, uint32_t pos, uint32_t mask)
{
  uint32_t calcMask;

  Generate<DIRECT>("#define %s%s ", pName, m_definePosStr);
  GenerateH_CRTabOffset(HEADER_POSMAK_VAL_OFFSET);
  Generate<RAW>("(%iUL) ", pos);

  // Doxy Comment
  GenerateH_CRTabOffset(HEADER_POSMAK_COMMENT_OFFSET2);
  Generate<DOXY_COMMENT_POSMSK|BEGIN>("%s (Bit %i)", field, pos);
  GenerateH_CRTabOffset(HEADER_POSMAK_COMMENTEND_OFFSET2);
  Generate<DOXY_COMMENT|END>("");

  calcMask = mask << pos;
  Generate<DIRECT>("#define %s%s ", pName, m_defineMaskStr);
  GenerateH_CRTabOffset(HEADER_POSMAK_VAL_OFFSET);
  Generate<RAW>("(0x%xUL) ", calcMask);

  // Doxy Comment
  GenerateH_CRTabOffset(HEADER_POSMAK_COMMENT_OFFSET2);
  Generate<DOXY_COMMENT_POSMSK|BEGIN>("%s (Bitfield-Mask: 0x%02x)", field, mask);
  GenerateH_CRTabOffset(HEADER_POSMAK_COMMENTEND_OFFSET2);
  Generate<DOXY_COMMENT|END>("");

  if(!(mask % 2)) {
    Generate<DOXY_COMMENT_POSMSK>("Even number for MASK detected!");
    LogMsg("M220", MSG("Even number for MASK detected!"));
  }
}

void HeaderGenerator::MakeTypedefToArray(const std::string& name, uint32_t num)
{
  /* typedef GPIO_PRT_Type  GPIO_PRT_ARRAYType[2];
     creates 2 * sizeof(GPIO_PRT_Type)
     therefore we must use [1] to declare the type as array, but create only 1 * sizeof(GPIO_PRT_Type)
  */
  Generate<DIRECT>("typedef %s_Type  %s_ARRAYType[%i];", name, name, 1/*num*/);
  Generate<DOXY_COMMENT>("max. %i instances available", num);
}

void HeaderGenerator::MakeDoxygenAddGroup(const std::string& text)
{
  Generate_NewLine();
  Generate<DIRECT>("/** %caddtogroup %s\n  * %s\n  */", '@', text.c_str(), "@{");
  Generate_NewLine();
}

void HeaderGenerator::MakeDoxygenEndGroup(const std::string& text)
{
  Generate_NewLine();
  Generate<DIRECT>("/** %s */ /* End of group %s */", "@}", text.c_str());
}

void HeaderGenerator::MakeDoxygenAddPeripheral(const std::string& text)
{
  Generate_NewLine();
  Generate<DIRECT>("/**\n  * @brief %s\n  */", text.c_str());
}
