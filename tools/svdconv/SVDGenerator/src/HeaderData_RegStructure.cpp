/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdarg.h>
#include "HeaderData.h"
#include "HeaderGenerator.h"
#include "SvdItem.h"
#include "ErrLog.h"
#include "SvdDimension.h"
#include "SvdTypes.h"
#include "SvdUtils.h"
#include "HeaderGenAPI.h"

using namespace std;
using namespace c_header;


#define C_STRUCT         0
#define C_UNION          1
#define C_CLOSE          0
#define C_OPEN           1

#define INVALID_NODE    ((RegTreeNode*)-1)


bool HeaderData::AddNode_Register(SvdItem *reg, RegSorter* pRegSorter)
{
  if(!reg) {
    return false;
  }

  uint32_t address      = (uint32_t)reg->GetAddress();
  pRegSorter->address   = (address & 0xfffffffc);
  pRegSorter->unaligned = 0; //reg->pBlock->baseAddress & 0x03;
  uint32_t pos          = address & 0x03;
  uint32_t width        = reg->GetEffectiveBitWidth();
    
  if(width < 9)    {
    if(pRegSorter->accessBYTE[pos].regItems > MAX_REGS) {
      LogMsg("M105", NAME(reg->GetName()));
      return false;
    }
    pRegSorter->accessBYTE[pos].reg[pRegSorter->accessBYTE[pos].regItems++] = reg;
  }
  else if(width < 17)   {
    if(pRegSorter->accessSHORT[pos? 1 : 0].regItems > MAX_REGS) {
      LogMsg("M105", NAME(reg->GetName()));
      return false;
    }
    pRegSorter->accessSHORT[pos? 1 : 0].reg[pRegSorter->accessSHORT[pos? 1 : 0].regItems++] = reg;
  }
  else if(width < 33)   {
    if(pRegSorter->accessINT.regItems > MAX_REGS) {
      LogMsg("M105", NAME(reg->GetName()));
      return false;
    }
    pRegSorter->accessINT.reg[pRegSorter->accessINT.regItems++] = reg;
    if(pRegSorter->accessINT.regItems >= MAX_REGS-1) {
      LogMsg("M105", NAME(reg->GetName()));
    }
  }

  return true;
}


/*
//Structure:
// the possibility of double-union is because of the generation process, 
// e.g. multiple of the same and different sizes

typedef struct {
1   union {

5       union {
            uint32_t i0;
            uint32_t i1;
5       }

2       struct {

3           union {
6               union {
                    short s0;
                    short s0a;
6               }
7               struct {
8                   union {
                        byte b0;
                        byte b0a
8                   }
9                   union {
                        byte b1;
                        byte b1a;
9                   }
7               }
3           }

13          union {
16              union {
                    short s1;
                    short s1a;
16              }
17              struct {
18                  union {
                        byte b2;
                        byte b2a
18                  }
19                  union {
                        byte b3;
                        byte b3a;
19                  }
17              }
13          }

2       }

1   }
}
*/


void HeaderData::Init_OpenCloseStructUnion()
{
  m_prevWasStruct = 0;
  m_prevWasUnion = 0;
}

void HeaderData::Push_structUnionStack(bool isUnion, uint32_t num)
{
  if(m_structUnionPos < 31) {
    m_structUnionPos++;
  }
  else {
    //Message(MSG_CRITICAL, "Struct/Union Stack full!");
  }

  m_structUnionStack[m_structUnionPos].isUnion  = isUnion;
  m_structUnionStack[m_structUnionPos].num      = num;
}


void HeaderData::Pop_structUnionStack(bool &isUnion, uint32_t &num)
{
  isUnion = m_structUnionStack[m_structUnionPos].isUnion;
  num     = m_structUnionStack[m_structUnionPos].num;

  m_structUnionStack[m_structUnionPos].isUnion  = 0;
  m_structUnionStack[m_structUnionPos].num      = 0;

  if(m_structUnionPos) {
    m_structUnionPos--;
  }
  else {
    //Message(MSG_CRITICAL, "Struct/Union Stack empty!");
  }
}

bool HeaderData::InUnion()
{
  if(!m_structUnionPos) {
    return false;
  }

  return m_structUnionStack[m_structUnionPos].isUnion;
}

bool HeaderData::InStruct()
{
  if(!m_structUnionPos) {
    return false;
  }

  return !m_structUnionStack[m_structUnionPos].isUnion;
}

bool HeaderData::InStuctUnion(uint32_t num)
{
  return m_structUnionStack[num].num ? true : false;
}

bool HeaderData::IsUnion()
{
  if(!m_structUnionPos) {
    return false;
  }

  return m_structUnionStack[m_structUnionPos].isUnion;
}

uint32_t HeaderData::GetUnionStructNum()
{
  if(!m_structUnionPos) {
    return (uint32_t)-1;
  }

  return m_structUnionStack[m_structUnionPos].num;
}

void HeaderData::CreateStructUnion(bool isUnion, bool open, uint32_t num)
{
  uint32_t localNum;

  if(open) {
    if(isUnion == InUnion()) {
      return;
    }

    Push_structUnionStack(isUnion, num);

    if(isUnion) {
      m_gen->Generate<UNION|BEGIN>("");
    }
    else {
      m_gen->Generate<STRUCT|BEGIN>("");
    }

    if(m_bDebugStruct) {
      m_gen->Generate<RAW>(" // %i", num);
    }
  }
  else {
    if(num == GetUnionStructNum()) {
      bool localIsUnion = false;
      Pop_structUnionStack(localIsUnion, localNum);

      if(isUnion) {
        m_gen->Generate<UNION|END|ANON>("");
      }
      else {
        m_gen->Generate<STRUCT|END|ANON>("");
      }

      if(m_bDebugStruct) {
        m_gen->Generate<RAW>(" // %i", num);
      }
    }
  }
}

RegTreeNode *HeaderData::GetNextRegNode(bool first /* = 0 */)
{
  static uint32_t cnt = 0;

  if(first) {
    memset(m_regTreeNodes, 0, sizeof(m_regTreeNodes));      // clean array
    cnt = 0;
  }

  return &m_regTreeNodes[cnt++];
}

bool HeaderData::NodeValid(RegTreeNode *node)
{
  if(node && node != INVALID_NODE) {
    return true;
  }

  return false;
}

bool HeaderData::NodeHasRegs(RegTreeNode *node)
{
  if(node->regItems) {
    return true;
  }

  return false;
}

uint32_t HeaderData::NodeHasChilds(RegTreeNode *node, uint32_t pos)
{
  if(!node) {
    return 0;
  }

  uint32_t cnt=0;
  for(uint32_t index = pos; index < 4; index++) {
    const auto pNode = node->pos[index];
    if(NodeValid(pNode)) {
      cnt++;
    }
  }

  return cnt;
}

void HeaderData::AddReservedBytesLater(int32_t reservedBytes)
{
  m_addReservedBytesLater = reservedBytes;
}

void HeaderData::GenerateReserved(int32_t resBytes, uint32_t address, bool bGenerate /*= true */)
{
  int32_t reservedBytes;
  
  reservedBytes = resBytes + m_addReservedBytesLater;
  m_addReservedBytesLater = 0;

  if(!reservedBytes) {
    return;
  }

  uint32_t addr = address;
  switch(addr % 4) {     // natural alignment
    case 1:
      if(reservedBytes) {
        AddReservedPad(1);
        reservedBytes -= 1;
        addr += 1;
      }
      if(reservedBytes) {
        if(reservedBytes == 1) {
          AddReservedPad(1);
          reservedBytes -= 1;
          addr += 1;
        }
        else {
          AddReservedPad(2);
          reservedBytes -= 2;
          addr += 2;
        }
      }
      break;
    case 2:
      if(reservedBytes) {
        if(reservedBytes == 1) {
          AddReservedPad(1);
          reservedBytes -= 1;
          addr += 1;
        }
        else {
          AddReservedPad(2);
          reservedBytes -= 2;
          addr += 2;
        }
      }
      break;
    case 3:
      if(reservedBytes) {
        AddReservedPad(1);
        reservedBytes -= 1;
        addr += 1;
      }
      break;
    default:
      break;
  }

  // Now we are natural aligned...
  int reservedBytesDWORD = (reservedBytes / 4) * 4;
  if(reservedBytesDWORD) {
    AddReservedPad(reservedBytesDWORD);
    addr += 4;
    reservedBytes -= reservedBytesDWORD;
  }

  // We are still natural aligned...
  switch(reservedBytes) {
    case 1:
      AddReservedPad(1);
      break;
    case 2:
      AddReservedPad(2);
      break;
    case 3:
      AddReservedPad(2);
      AddReservedPad(1);
      break;
    default:
      break;
  }

  if(bGenerate) {
    GenerateReserved();
  }
}


void HeaderData::AddReservedPad(int32_t bytes)
{
  uint32_t width = 0;
  if     (!((bytes) % 4)) { width = 4; }
  else if(!((bytes) % 2)) { width = 2; }
  else                    { width = 1; }

  int32_t nMany = bytes / (int32_t)width;

  if(width != 1 && width != 2 && width != 4) {
    m_gen->Generate<C_ERROR>("Padding error: width = %d, many = %d", width, nMany);
    return;
  }

  uint32_t maxWidth = GetMaxBitWidth();
  maxWidth /= 8;
  if(!maxWidth) {
    maxWidth = 1;
  }

  if(width > maxWidth) {
    nMany = nMany * (width / maxWidth);
    width = maxWidth;
  }

  ReservedPad reservedPad;
  reservedPad.nMany = nMany;
  reservedPad.width = width;
  m_reservedPad.push_back(reservedPad);
}

void HeaderData::GenerateReserved()
{
  if(!m_reservedPad.size()) {
    return;
  }

  int32_t nMany = 0;
  uint32_t width = 0;

  for(const auto& pad : m_reservedPad) {
    if(nMany && width != pad.width) {
      GenerateReservedWidth(nMany, width);
      nMany = 0;
    }

    nMany += pad.nMany;
    width = pad.width;
  }

  if(width) {
    GenerateReservedWidth(nMany, width);
  }
  else {
    m_gen->Generate<C_ERROR>("Padding error: width = %d, many = %d", width, nMany);
  }

  m_reservedPad.clear();
}

void HeaderData::GenerateReservedWidth(int32_t nMany, uint32_t width)
{
  if(!nMany || !width) {
    return;
  }

  const auto& dataType = SvdUtils::GetDataTypeString(width);
  string resNum;
  if(m_reservedCnt) {
    resNum = to_string(m_reservedCnt);
  }

  if(nMany == 1) {
    m_gen->Generate<MAKE|MK_REGISTER_STRUCT  >("RESERVED%s", SvdTypes::Access::READONLY, dataType.c_str(), width, resNum);
  }
  else {
    m_gen->Generate<MAKE|MK_REGISTER_STRUCT  >("RESERVED%s[%i]", SvdTypes::Access::READONLY, dataType.c_str(), width, resNum, nMany);
  }

  m_reservedCnt++;

  if(nMany < 0) {
    m_gen->Generate<C_ERROR >("Reserved bytes calculation negative: %i bytes!", -1, nMany * width);
  }
}

void HeaderData::GenerateReservedField(int32_t resBits, uint32_t regSize)
{
  if(!resBits) {
    return;
  }

  const auto& dataType = SvdUtils::GetDataTypeString(regSize);

  m_gen->Generate<MAKE|MK_FIELD_STRUCT    >("", SvdTypes::Access::UNDEF, dataType.c_str(), regSize, resBits);          // experimental: type and size only

  if(resBits < 0) {
    m_gen->Generate<C_ERROR>("Reserved bits calculation negative: %i", -1, resBits);
  }

  m_reservedFieldCnt++;

  return;
}

uint32_t HeaderData::GenerateRegItems(RegTreeNode *node, uint32_t address, uint32_t level)
{
  if(!NodeValid(node) || !NodeHasRegs(node)) {
    return 0;
  }

  uint32_t regAddress=0;
  uint32_t alignAddress=0;

  const auto item = node->reg[0];
  if(item) {
    if(item->GetParent()->GetSvdLevel() == L_Cluster) {
      regAddress   = (uint32_t)item->GetAddress()                       /* + localOffset*/;
      alignAddress = (uint32_t)item->GetParent()->GetAddress()          /* + localOffset*/;   // cluster elements can be relative to "any" address
    } else {
      //regAddress = (uint32_t)item->GetAbsoluteOffset()                /* + localOffset*/;
      regAddress   = (uint32_t)item->GetAddress()                       /* + localOffset*/;
      alignAddress = (uint32_t)item->GetParent()->GetAbsoluteAddress()  /* + localOffset*/;
    }
  }

  uint32_t genRes = regAddress - address;
  if(genRes && (InUnion() || InStruct())) {
    CreateStructUnion(C_STRUCT, C_OPEN, level + 0);
  }

  GenerateReserved(genRes, alignAddress + address);
  
  if(node->regItems > 1) {
    CreateStructUnion(C_UNION, C_OPEN, level + 1);
  }

  uint32_t size = 0;
  for(uint32_t i=0; i<node->regItems; i++) {
    uint32_t tmpSize = CreateSvdItem(node->reg[i], address + genRes);
    if(size < tmpSize) {
      size = tmpSize;
    }
  }

  size += genRes;
  CreateStructUnion(C_UNION, C_CLOSE, level + 1);
  CreateStructUnion(C_STRUCT, C_CLOSE, level + 0);

  return size;
}

uint32_t HeaderData::GenerateChildNodes(RegTreeNode *node, uint32_t address, uint32_t level)
{
  if(!NodeValid(node)) {
    return 0;
  }

  uint32_t size = 0;
  uint32_t sizeMax = 0;
  uint32_t localOffset=0;

  for(uint32_t index=0; index<4; index++) {
    const auto pNode = node->pos[index];
    if(NodeValid(pNode)) {
      size = GenerateNode(pNode, address, level + 4, sizeMax, localOffset);
      if(sizeMax < size) {
        sizeMax = size;
      }

      if(!InUnion()) {
        localOffset += size;
      }
    }
  }

  if(size < sizeMax) {
    size = sizeMax;
  }

  return size;
}

uint32_t HeaderData::GenerateNode(RegTreeNode *node, uint32_t address, uint32_t level, uint32_t &sizeMax, uint32_t localOffset /* = 0 */)
{
  if(!NodeValid(node)) {
    return 0;
  }
  
  SvdItem *item = nullptr;
  if     (node->reg[0]                        ) { item = node->reg[0];         }
  else if(node->pos[0] && node->pos[0]->reg[0]) { item = node->pos[0]->reg[0]; }
  else if(node->pos[1] && node->pos[1]->reg[0]) { item = node->pos[1]->reg[0]; }
  else if(node->pos[2] && node->pos[2]->reg[0]) { item = node->pos[2]->reg[0]; }
  else if(node->pos[3] && node->pos[3]->reg[0]) { item = node->pos[3]->reg[0]; }

  if(!item) {
    return 0;
  }

  // Check if there is some empty block before any struct / union
  uint32_t regAddress = 0;
  uint32_t alignAddress = 0;
  int32_t  genRes = 0;

  if(!InUnion() && !InStruct()) {
    if(item->GetParent()->GetSvdLevel() == L_Cluster) {
      regAddress   = (uint32_t)item->GetAddress()                       /* + localOffset*/;
      alignAddress = (uint32_t)item->GetParent()->GetAddress()          /* + localOffset*/;   // cluster elements can be relative to "any" address
    } else {
      //regAddress = (uint32_t)item->GetAbsoluteOffset()                /* + localOffset*/;
      regAddress   = (uint32_t)item->GetAddress()                       /* + localOffset*/;
      alignAddress = (uint32_t)item->GetParent()->GetAbsoluteAddress()  /* + localOffset*/;
    }

    if(regAddress < address + localOffset) {
      string regName;
      const auto dim = item->GetDimension();
      if(dim) {
        regName = dim->GetExpression()->GetName();
        regName += "<dim>";
      } else {
        regName = item->GetName();
      }

      m_gen->Generate<C_ERROR >("Cannot generate Register or Cluster '%s': Address (0x%08x) is lower than actual address in struct (0x%08x)", item->GetLineNumber(), regName.c_str(), regAddress, address + localOffset);
      
      return 0;
    }

    if(regAddress) {
      genRes   = regAddress - (address + localOffset);
      GenerateReserved(genRes, alignAddress + address + localOffset);
      address += genRes;
    }
  }

  if(NodeHasChilds(node, 0) && NodeHasRegs(node)) {     // union, if any childs
    CreateStructUnion(C_UNION, C_OPEN,  level + 0);
  }

  uint32_t size = GenerateRegItems(node, address + localOffset, level + 2);

  if(InUnion() && NodeHasChilds(node, 1)) {
    CreateStructUnion(C_STRUCT, C_OPEN,  level + 1);
  }

  uint32_t tmpSize = GenerateChildNodes(node, address + localOffset, level);
  if(size < tmpSize) {
    size = tmpSize;
  }

  CreateStructUnion(C_STRUCT, C_CLOSE,  level + 1);
  CreateStructUnion(C_UNION,  C_CLOSE,  level + 0);

  size += genRes;

  if(InUnion()) {
    if(sizeMax < size) {
      sizeMax = size;
    }
  }
  else {
    sizeMax += size;
  }

  return size;
}

void HeaderData::GeneratePart(RegSorter *pRegSorter)
{
  uint32_t i;
  RegTreeNode *pNode=nullptr, *pTmpNode=nullptr;
  
  pNode = GetNextRegNode(1);    // INT node always needed
  m_rootNode = pNode;

  if(pRegSorter->accessINT.regItems) {
    for(i=0; i<pRegSorter->accessINT.regItems; i++) {
      pNode->reg[pNode->regItems++] = pRegSorter->accessINT.reg[i];
    }

    pNode->type = REGTYPE::REG_INT;
  }

  if(pRegSorter->accessSHORT[0].regItems) {
    pNode = GetNextRegNode();
    for(i=0; i<pRegSorter->accessSHORT[0].regItems; i++) {
      pNode->reg[pNode->regItems++] = pRegSorter->accessSHORT[0].reg[i];
    }
    pNode->type = REGTYPE::REG_SHORT;
    m_rootNode->pos[0] = pNode;
    m_rootNode->pos[1] = INVALID_NODE;
  }

  if(pRegSorter->accessSHORT[1].regItems) {
    pNode = GetNextRegNode();
    for(i=0; i<pRegSorter->accessSHORT[1].regItems; i++) {
      pNode->reg[pNode->regItems++] = pRegSorter->accessSHORT[1].reg[i];
    }
    pNode->type = REGTYPE::REG_SHORT;
    m_rootNode->pos[2] = pNode;
    m_rootNode->pos[3] = INVALID_NODE;
  }

  if(pRegSorter->accessBYTE[0].regItems) {
    pNode = GetNextRegNode();
    for(i=0; i<pRegSorter->accessBYTE[0].regItems; i++) {
      pNode->reg[pNode->regItems++] = pRegSorter->accessBYTE[0].reg[i];
    }
    pNode->type = REGTYPE::REG_BYTE;

    if(!m_rootNode->pos[0]) {
      m_rootNode->pos[0] = pNode;
    }
    else {
      pTmpNode = m_rootNode->pos[0];
      pTmpNode->pos[0] = pNode;
    }
  }

  if(pRegSorter->accessBYTE[1].regItems) {
    pNode = GetNextRegNode();
    for(i=0; i<pRegSorter->accessBYTE[1].regItems; i++) {
      pNode->reg[pNode->regItems++] = pRegSorter->accessBYTE[1].reg[i];
    }
    pNode->type = REGTYPE::REG_BYTE;

    if(!m_rootNode->pos[1]) {
      m_rootNode->pos[1] = pNode;       // INT node
    }
    else {
      pTmpNode = m_rootNode->pos[0];    // SHORT0 node
      pTmpNode->pos[1] = pNode;
    }
  }

  if(pRegSorter->accessBYTE[2].regItems) {
    pNode = GetNextRegNode();
    for(i=0; i<pRegSorter->accessBYTE[2].regItems; i++) {
      pNode->reg[pNode->regItems++] = pRegSorter->accessBYTE[2].reg[i];
    }
    pNode->type = REGTYPE::REG_BYTE;

    if(!m_rootNode->pos[2]) {           // INT node
      m_rootNode->pos[2] = pNode;
    }
    else {
      pTmpNode = m_rootNode->pos[2];    // SHORT1 node
      pTmpNode->pos[0] = pNode;
    }
  }

  if(pRegSorter->accessBYTE[3].regItems) {
    pNode = GetNextRegNode();
    for(i=0; i<pRegSorter->accessBYTE[3].regItems; i++) {
      pNode->reg[pNode->regItems++] = pRegSorter->accessBYTE[3].reg[i];
    }
    pNode->type = REGTYPE::REG_BYTE;

    if(!m_rootNode->pos[3]) {           // INT node
      m_rootNode->pos[3] = pNode;
    }
    else {
      pTmpNode = m_rootNode->pos[2];    // SHORT1 node
      pTmpNode->pos[1] = pNode;
    }
  }
  
  if(pRegSorter->unaligned) {
    m_gen->Generate<C_WARNING >("Peripheral unaligned address: 0x%08x", -1, pRegSorter->address + pRegSorter->unaligned);
  }

  //uint32_t address = pRegSorter->address + pRegSorter->unaligned;
  uint32_t address  = m_addressCnt;
  uint32_t sizeMax = 0;
  GenerateNode(m_rootNode, address, 1, sizeMax);    // start level must be 1

  m_addressCnt = address + sizeMax;
}
