/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef HeaderGenAPI_H
#define HeaderGenAPI_H

#include "SvdTypes.h"


namespace c_header {

// ----------  language (256) [0-7]  ---------------
inline constexpr auto INDEX_MASK = 0x000000ff;

enum Index {
  EMPTY                = 0,
  DOXY_COMMENT         ,
  C_COMMENT            ,
  UNION                ,
  STRUCT               ,
  ENUM                 ,
  C_ERROR              ,
  C_WARNING            ,
  DOXY_COMMENT_POSMSK  ,
  DOXY_COMMENTSTAR     ,
};

// ----------  language additional (256)  [8-15] ---------------
inline constexpr auto LANGADD_MASK  = 0x0000ff00;
inline constexpr auto LANGADD_SHIFT = 8;

enum Additional {
  TYPEDEF                     = ( 1<<LANGADD_SHIFT),
  TYPEDEFARR                  = ( 2<<LANGADD_SHIFT),
}; 

// ----------  Options (256) [24-32] -----------
inline constexpr auto OPTIONS_MASK  = 0xff000000;
inline constexpr auto OPTIONS_SHIFT = 24;

enum Options {
  BEGIN                       = ( 1<<OPTIONS_SHIFT),
  END                         = ( 2<<OPTIONS_SHIFT),
  RAW                         = ( 3<<OPTIONS_SHIFT),
  DIRECT                      = ( 4<<OPTIONS_SHIFT),
  ENDGROUP                    = ( 5<<OPTIONS_SHIFT),
  MAKE                        = ( 6<<OPTIONS_SHIFT),
  DESCR                       = ( 7<<OPTIONS_SHIFT),
};

// ----------  special (256) [16-23] -----------
inline constexpr auto SPECIAL_MASK  = 0x00ff0000;
inline constexpr auto SPECIAL_SHIFT = 16;

enum Special {
  NONE                      = ( 0<<SPECIAL_SHIFT),
  ANON                      = ( 1<<SPECIAL_SHIFT),
  HEADER                    = ( 2<<SPECIAL_SHIFT),
  PART                      = ( 3<<SPECIAL_SHIFT),
  SUBPART                   = ( 4<<SPECIAL_SHIFT),
  MK_INCLUDE                = ( 5<<SPECIAL_SHIFT),
  MK_REGISTER_STRUCT        = ( 6<<SPECIAL_SHIFT),
  MK_PERIMAP                = ( 7<<SPECIAL_SHIFT),
  MK_PERIADDRDEF            = ( 8<<SPECIAL_SHIFT),
  MK_PERIADDRMAP            = ( 9<<SPECIAL_SHIFT),
  MK_DOXY_COMMENT_ADDR      = (10<<SPECIAL_SHIFT),
  MK_INTERRUPT_STRUCT       = (11<<SPECIAL_SHIFT),
  MK_DOXYADDGROUP           = (12<<SPECIAL_SHIFT),
  MK_DOXYADDPERI            = (13<<SPECIAL_SHIFT),
  MK_CMSIS_CONFIG           = (14<<SPECIAL_SHIFT),   // CM Name, CM Rev, MPU present, NVIC Prio Bits, Vendor SysTick Config  (default: 0)
  MK_DOXY_COMMENT           = (15<<SPECIAL_SHIFT),
  MK_DOXYENDGROUP           = (16<<SPECIAL_SHIFT),
  MK_FIELD_UNION            = (17<<SPECIAL_SHIFT),
  MK_FIELD_POSMASK          = (18<<SPECIAL_SHIFT),
  MK_DOXY_COMMENT_BITPOS    = (19<<SPECIAL_SHIFT),
  MK_DOXY_COMMENT_BITFIELD  = (20<<SPECIAL_SHIFT),
  MK_PERIARRADDRMAP         = (21<<SPECIAL_SHIFT),
  MK_FIELD_POSMASK2         = (22<<SPECIAL_SHIFT),
  MK_INCLUDE_CORE           = (23<<SPECIAL_SHIFT),
  MK_INCLUDE_SYSTEM         = (24<<SPECIAL_SHIFT),
  MK_FIELD_STRUCT           = (25<<SPECIAL_SHIFT),
  MK_ENUMVALUE              = (26<<SPECIAL_SHIFT),
  MK_DEFINE                 = (27<<SPECIAL_SHIFT),
  MK_HEADERIFDEF            = (28<<SPECIAL_SHIFT),
  MK_HEADEREXTERNC_BEGIN    = (29<<SPECIAL_SHIFT),
  MK_HEADEREXTERNC_END      = (30<<SPECIAL_SHIFT),
  MK_TYPEDEFTOARRAY         = (31<<SPECIAL_SHIFT),
  MK_FIELD_POSMASK3         = (32<<SPECIAL_SHIFT),
  MK_IFNDEF                 = (33<<SPECIAL_SHIFT),
  MK_ENDIF                  = (34<<SPECIAL_SHIFT),
  MK_DEFINE_TEXT            = (35<<SPECIAL_SHIFT),
  MK_ANONUNION_COMPILER     = (36<<SPECIAL_SHIFT),
  MK_CPLUSPLUS              = (37<<SPECIAL_SHIFT),
  MK_IFNDEF_H               = (38<<SPECIAL_SHIFT),
};
}

#endif // HeaderGenAPI_H
