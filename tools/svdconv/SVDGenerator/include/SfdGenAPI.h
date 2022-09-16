/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef SfdGenAPI_H
#define SfdGenAPI_H

#include "SvdTypes.h"

namespace sfd {
// ----------  language (64k)  ---------------
inline constexpr auto INDEX_MASK = 0x0000ffff;

enum Index {
  EMPTY        = 0,
  MEMBER       ,
  BLOCK        ,
  VIEW         ,
  ITEM         ,
  INFO         ,
  CHECK        ,
  LOC          ,
  EDIT         ,
  TXT          ,
  TABLE        ,
  THEAD        ,
  TCOLUMN      ,
  TROW         ,
  COMBO        ,
  ITREE        ,
  RTREE        ,
  GRP          ,
  NAME         ,
  ACC_R        ,
  ACC_W        ,
  ACC_RW       ,
  QITEM        ,
  IRQTABLE     ,
  NVICPRIOBITS ,
  DISABLECOND  ,
  HEADING      ,
  HEADINGENABLE,
  OPTION       ,
  QOPTION      ,
};

// ----------  Options (256) -----------
inline constexpr auto OPTIONS_MASK  = 0xff000000;
inline constexpr auto OPTIONS_SHIFT = 24;

enum Options {
  BEGIN           = ( 1<<OPTIONS_SHIFT),
  END             = ( 2<<OPTIONS_SHIFT),
  RAW             = ( 3<<OPTIONS_SHIFT),
  ENDIS           = ( 4<<OPTIONS_SHIFT),
  IBIT            = ( 5<<OPTIONS_SHIFT),
  IBIT_ADDR       = ( 6<<OPTIONS_SHIFT),
  IBIT_ADDR_ACC   = ( 7<<OPTIONS_SHIFT),
  IRANGE          = ( 8<<OPTIONS_SHIFT),
  IRANGE_ADDR     = ( 9<<OPTIONS_SHIFT),
  IRANGE_ADDR_ACC = (10<<OPTIONS_SHIFT),
  DIRECT          = (11<<OPTIONS_SHIFT),
  ENDGROUP        = (12<<OPTIONS_SHIFT),
  MAKE            = (13<<OPTIONS_SHIFT),
  DESCR           = (14<<OPTIONS_SHIFT),
  TEXTONLY        = (15<<OPTIONS_SHIFT),
  SINGLE          = (16<<OPTIONS_SHIFT),
  INFO_ADDR       = (17<<OPTIONS_SHIFT),
  DESCR_LINENO    = (18<<OPTIONS_SHIFT),
  APPENDTEXT      = (19<<OPTIONS_SHIFT),
};

// ----------  special (256)  -----------
inline constexpr auto SPECIAL_MASK  = 0x00ff0000;
inline constexpr auto SPECIAL_SHIFT = 16;

enum Special {
  CITEM                = ( 1<<SPECIAL_SHIFT),
  OBIT                 = ( 2<<SPECIAL_SHIFT),
  ORANGE               = ( 3<<SPECIAL_SHIFT),
  HEADER               = ( 4<<SPECIAL_SHIFT),
  PART                 = ( 5<<SPECIAL_SHIFT),
  SUBPART              = ( 6<<SPECIAL_SHIFT),
  MK_EDITLOC           = ( 7<<SPECIAL_SHIFT),
  MK_OBITLOC           = ( 8<<SPECIAL_SHIFT),
  MK_ADDRSTR           = ( 9<<SPECIAL_SHIFT),
  MK_INTERRUPT         = (10<<SPECIAL_SHIFT),
  MK_INTERRUPT_LIST    = (11<<SPECIAL_SHIFT),
  STRLEN               = (12<<SPECIAL_SHIFT),  // <s.10>
  MK_ENABLEDISABLE     = (13<<SPECIAL_SHIFT),
  OBIT_NORANGE         = (14<<SPECIAL_SHIFT),
  MK_OPTSEL            = (15<<SPECIAL_SHIFT),
};
}

#endif // SfdGenAPI_H
