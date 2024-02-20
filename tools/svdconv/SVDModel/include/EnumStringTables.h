/*
 * Copyright (c) 2010-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef CPUTYPE
  CPUTYPE(UNDEF            , ""         )

  // SVDConv supported types
  CPUTYPE(CM0              , "CM0"      )
  CPUTYPE(CM0PLUS          , "CM0PLUS"  )
  CPUTYPE(CM0P             , "CM0+"     )
  CPUTYPE(CM1              , "CM1"      )
  CPUTYPE(SC000            , "SC000"    )
  CPUTYPE(CM3              , "CM3"      )
  CPUTYPE(SC300            , "SC300"    )
  CPUTYPE(CM4              , "CM4"      )
  CPUTYPE(CM7              , "CM7"      )
  CPUTYPE(CM23             , "CM23"     )
  CPUTYPE(CM33             , "CM33"     )
  CPUTYPE(CM35             , "CM35"     )
  CPUTYPE(CM35P            , "CM35P"    )
  CPUTYPE(V8MML            , "ARMV8MML" )
  CPUTYPE(V8MBL            , "ARMV8MBL" )
  CPUTYPE(V81MML           , "ARMV81MML")
  CPUTYPE(CM55             , "CM55"     )
  CPUTYPE(CM85             , "CM85"     )
  
  // China
  CPUTYPE(SMC1             , "SMC1"     )
  CPUTYPE(CM52             , "CM52"     )

  // Types not supported by SVDConv
  CPUTYPE(CA5              , "CA5"      )
  CPUTYPE(CA7              , "CA7"      )
  CPUTYPE(CA8              , "CA8"      )
  CPUTYPE(CA9              , "CA9"      )
  CPUTYPE(CA15             , "CA15"     )
  CPUTYPE(CA17             , "CA17"     )
  CPUTYPE(CA53             , "CA53"     )
  CPUTYPE(CA57             , "CA57"     )
  CPUTYPE(CA72             , "CA72"     )
  CPUTYPE(OTHER            , "OTHER"    )

  // last entry
  CPUTYPE(END              , ""         )
#endif


#ifdef MODIFWRV
  MODIFWRV(UNDEF           , "undefined"     )
  MODIFWRV(ONETOCLEAR      , "oneToClear"    )
  MODIFWRV(ONETOSET        , "oneToSet"      )
  MODIFWRV(ONETOTOGGLE     , "oneToToggle"   )
  MODIFWRV(ZEROTOCLEAR     , "zeroToClear"   )
  MODIFWRV(ZEROTOSET       , "zeroToSet"     )
  MODIFWRV(ZEROTOTOGGLE    , "zeroToToggle"  )
  MODIFWRV(CLEAR           , "clear"         )
  MODIFWRV(SET             , "set"           )
  MODIFWRV(MODIFY          , "modify"        )

  // last entry
  MODIFWRV(END             , ""         )
#endif
