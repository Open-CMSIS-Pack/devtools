/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ErrLog.h"
#include "XML_Reader.h"

const MsgTable XML_Reader::msgTable = {
  { "M401", { MsgLevel::LEVEL_INFO ,    CRLF_BE,  "Did you mean '</%TAG%>'?"                                                    }  },
  { "M402", { MsgLevel::LEVEL_INFO ,    CRLF_B,   "Tag:       [%TYPE%] '%TAG%'"                                                 }  },
  { "M403", { MsgLevel::LEVEL_INFO ,    CRLF_B,   "Data:      [%TYPE%] '%DATA%'"                                                }  },
  { "M404", { MsgLevel::LEVEL_INFO ,    CRLF_B,   "Attribute name: '%NAME%'"                                                    }  },
  { "M405", { MsgLevel::LEVEL_INFO ,    CRLF_B,   "Attribute data: '%DATA%'"                                                    }  },
  { "M406", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "XML Stack:\n%MSG%"                                                           }  },
  { "M407", { MsgLevel::LEVEL_INFO ,    CRLF_B,   "Recover from Error"                                                          }  },
  { "M408", { MsgLevel::LEVEL_INFO ,    CRLF_B,   "Recover from Error: giving up after 100 tries..."                            }  },
  { "M409", { MsgLevel::LEVEL_INFO ,    CRLF_B,   "Skipping unknown Tag: '%TAG%'"                                               }  },
  { "M410", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Lost xml file stream."                                                       }  },
  { "M411", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Preamble for '%UTF%' should not be used, specify via '<?xml'"                }  },
  { "M412", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Unsupported format or extra characters found before '<?xml': '%STR%'"        }  },
  { "M413", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "UTF Format not supported: '%UTF%'"                                           }  },
  { "M414", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Cannot decode XML special character: '%SPECIALCHAR%'. %MSG%"                 }  },
  { "M415", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "'<--' found, should this be a comment '<!--' ?"                              }  },
  { "M416", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Begin Tag seems to end with a Single Tag. Is this a typo?"                   }  },
  { "M417", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Inconsistent XML Structure"                                                  }  },
  { "M418", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "XML Stack deeper than 30 Items! Giving up..."                                }  },
  { "M419", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Begin Tag follows Text. Missing End Tag?"                                    }  },
  { "M420", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Missing '\"' in Attributes: '%ATTRLINE%'"                                    }  },
  { "M421", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "XML Hierarchy Error: Missing End Tags."                                      }  },
  { "M422", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Error reading file '%NAME%'"                                                 }  },
};

const MsgTableStrict XML_Reader::msgStrictTable = {
  { "M412",   MsgLevel::LEVEL_ERROR },
  { "M413",   MsgLevel::LEVEL_ERROR },
  { "M414",   MsgLevel::LEVEL_ERROR },
};
