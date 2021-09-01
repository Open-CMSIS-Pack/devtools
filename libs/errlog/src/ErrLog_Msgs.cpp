/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ErrLog.h"


/*
  Examples
  --------
LogMsg("M008");     //
LogMsg("M001", VAL("TEXT", text));     //
LogMsg("M002", VAL("TEXT", text), VAL("TEXT2", text));     //
LogMsg("M003", VAL("TEXT", text), VAL("TEXT2", text), VAL("TEXT3", text3));     //
LogMsg("M004", VAL("TEXT", text), VAL("TEXT2", text), VAL("TEXT3", text3), VAL("TEXT4", text4));     //


  Message levels
  --------------
  Message gets printed if level is greater or equal to set message-level (default: -w0, warning)

    LEVEL_DEBUG     = 0,     // programm debug information
    LEVEL_INFO2     = 1,     // all processing infos
    LEVEL_PROGRESS  = 2,     // Progress Messages or Progress Info (i.e. "."), then shifted to LEVEL_TEXT
    LEVEL_INFO      = 3,     // Information
    LEVEL_WARNING3  = 4,     // Warnings -w3
    LEVEL_WARNING2  = 5,     // Warnings -w2
    LEVEL_WARNING   = 6,     // Warnings -w1
    LEVEL_ERROR     = 7,     // Errors
    LEVEL_CRITICAL  = 8,     // Critical Errors
    LEVEL_TEXT      = 9,     // Always printed if not --quiet
*/


const MsgTable ErrLog::msgTable = {
  { "M000",   { MsgLevel::LEVEL_CRITICAL, CRLF_B,   "Log Message not found in Messages Table"                                     }  },
  { "M001",   { MsgLevel::LEVEL_TEXT ,    CRLF_B,   "%TEXT%"                                                                      }  },
  { "M002",   { MsgLevel::LEVEL_TEXT ,    CRLF_B,   "%TEXT%%TEXT2%"                                                               }  },
  { "M003",   { MsgLevel::LEVEL_TEXT ,    CRLF_B,   "%TEXT%%TEXT2%%TEXT3%"                                                        }  },
  { "M004",   { MsgLevel::LEVEL_TEXT ,    CRLF_B,   "%TEXT%%TEXT2%%TEXT3%%TEXT4%"                                                 }  },
  { "M005",   { MsgLevel::LEVEL_TEXT,     CRLF_B,   "%TEXT%%TEXT2%%TEXT3%%TEXT4%%TEXT5%"                                          }  },
  { "M006",   { MsgLevel::LEVEL_INFO ,    CRLF_NO,  "%TEXT%"                                                                      }  },
  { "M007",   { MsgLevel::LEVEL_TEXT ,    CRLF_E,   "SUCCEDED."                                                                   }  },
  { "M008",   { MsgLevel::LEVEL_TEXT ,    CRLF_E,   "FAILED!"                                                                     }  },
  { "M009",   { MsgLevel::LEVEL_INFO ,    CRLF_NO,  "FAILED!"                                                                     }  },
  { "M010",   { MsgLevel::LEVEL_INFO ,    CRLF_NO,  " OK"                                                                         }  },
  { "M011",   { MsgLevel::LEVEL_INFO ,    CRLF_E,   " DONE"                                                                       }  },
  { "M012",   { MsgLevel::LEVEL_PROGRESS, CRLF_E,   " DONE"                                                                       }  },
  { "M013",   { MsgLevel::LEVEL_INFO ,    CRLF_NO,  "."                                                                           }  },
  { "M014",   { MsgLevel::LEVEL_TEXT ,    CRLF_NO,  "."                                                                           }  },
  { "M015",   { MsgLevel::LEVEL_INFO ,    CRLF_NO,  "\n"                                                                          }  },
  { "M016",   { MsgLevel::LEVEL_TEXT ,    CRLF_NO,  "\n"                                                                          }  },
  { "M017",   { MsgLevel::LEVEL_ERROR,    CRLF_B,   "An Error Message (%MSG%) cannot be suppressed."                              }  },

  { "ERROR",  { MsgLevel::LEVEL_ERROR,    CRLF_B,   "ERROR"                                                                       }  },
  { "WARNING",{ MsgLevel::LEVEL_WARNING,  CRLF_B,   "WARNING"                                                                     }  },
  { "INFO",   { MsgLevel::LEVEL_INFO,     CRLF_B,   "INFO"                                                                        }  },

  { "M1000",  { MsgLevel::LEVEL_ERROR ,  CRLF_B,   "TestMessage M1000"                                                            }  },
  { "M1001",  { MsgLevel::LEVEL_ERROR ,  CRLF_B,   "TestMessage M1001"                                                            }  },
  { "M1002",  { MsgLevel::LEVEL_ERROR ,  CRLF_B,   "TestMessage M1002"                                                            }  },
  { "M1003",  { MsgLevel::LEVEL_WARNING, CRLF_B,   "TestMessage M1003"                                                            }  },
  { "M1004",  { MsgLevel::LEVEL_ERROR ,  CRLF_B,   "TestMessage M1004"                                                            }  },
};

const MsgTableStrict ErrLog::msgStrictTable = {
  { "XML112",   MsgLevel::LEVEL_ERROR },
  { "XML113",   MsgLevel::LEVEL_ERROR },
  { "XML114",   MsgLevel::LEVEL_ERROR },
};
