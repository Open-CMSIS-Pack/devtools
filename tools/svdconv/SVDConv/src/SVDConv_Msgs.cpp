/*
 * Copyright (c) 2010-2022 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "SVDConv.h"
#include "ErrLog.h"


const MsgTable SvdConv::msgTable = {
// 020 Constant Text (help, ...)
  { "M020", { MsgLevel::LEVEL_TEXT,     CRLF_B,   "" } },
  { "M021", { MsgLevel::LEVEL_TEXT,     CRLF_NO,  "" } },
  { "M022", { MsgLevel::LEVEL_TEXT ,    CRLF_B,   "Found %ERR% Error(s) and %WARN% Warning(s)."                                 } },
  { "M023", { MsgLevel::LEVEL_TEXT ,    CRLF_B,   "\nPhase%CHECK%"                                                              } },
  { "M024", { MsgLevel::LEVEL_TEXT ,    CRLF_B,   "Arguments: %OPTS%"                                                           } },


// 40... Info Messages (INFO = verbose)
  { "M040", { MsgLevel::LEVEL_INFO ,    CRLF_B,   "%NAME%: %TIME%ms. Passed"                                                    } },
  { "M041", { MsgLevel::LEVEL_INFO,     CRLF_B,   "Overall time: %TIME%ms."                                                     } },
  { "M042", { MsgLevel::LEVEL_INFO,     CRLF_B,   ""                                                                            } },
  { "M043", { MsgLevel::LEVEL_INFO,     CRLF_B,   ""                                                                            } },
  { "M044", { MsgLevel::LEVEL_INFO,     CRLF_B,   ""                                                                            } },
  { "M045", { MsgLevel::LEVEL_INFO,     CRLF_B,   ""                                                                            } },
  { "M046", { MsgLevel::LEVEL_INFO,     CRLF_B,   ""                                                                            } },
  { "M047", { MsgLevel::LEVEL_INFO,     CRLF_B,   ""                                                                            } },
  { "M048", { MsgLevel::LEVEL_INFO,     CRLF_B,   ""                                                                            } },
  { "M049", { MsgLevel::LEVEL_INFO,     CRLF_B,   ""                                                                            } },
  { "M050", { MsgLevel::LEVEL_INFO ,    CRLF_B,   "Current Working Directory: '%PATH%'"                                         } },
  { "M051", { MsgLevel::LEVEL_INFO ,    CRLF_B,   "Reading SVD File: '%PATH%'"                                                  } },

  { "M061", { MsgLevel::LEVEL_INFO,     CRLF_B,   "Checking SVD Description"                                                    } },


// 100... Internal and Invocation Errors
  { "M100", { MsgLevel::LEVEL_ERROR,    CRLF_B,   ""                                                                            } },
  { "M101", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Unknown error!"                                                              } },
  { "M102", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "MFC initialization failed"                                                   } },
  { "M103", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Internal Error: %REF%"                                                       } },
  { "M104", { MsgLevel::LEVEL_CRITICAL, CRLF_B,   "%MSG%"                                                                       } },
  { "M105", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Cannot add Register to group sorter: '%NAME%'"                               } },
  { "M106", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Command '%NAME%' failed: %NUM%: %MSG%"                                       } },
  { "M107", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Lost xml file stream."                                                       } },
  { "M108", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "SfrDis not supported."                                                       } },
  { "M109", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Cannot find '%NAME%'"                                                        } },
  { "M110", { MsgLevel::LEVEL_TEXT,     CRLF_B,   ""                                                                            } },
  { "M111", { MsgLevel::LEVEL_PROGRESS, CRLF_B,   "%NAME% failed!"                                                              } },

  { "M120", { MsgLevel::LEVEL_ERROR,    CRLF_BE,  "Invalid arguments!"                                                          } },
  { "M121", { MsgLevel::LEVEL_ERROR,    CRLF_B,   ""                                                                            } },
  { "M122", { MsgLevel::LEVEL_ERROR,    CRLF_BE,  "Name of command file should follow '@'"                                      } },
  { "M123", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "File/Path not found: '%PATH%'!"                                              } },
  { "M124", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Cannot execute SfrCC2: '%PATH%'!"                                            } },
  { "M125", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "SfrCC2 report:\n%MSG%\nSfrCC2 report end.\n"                                 } },
  { "M126", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "SfrDis: %MSG%"                                                               } },
  { "M127", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "SfrCC2 reports errors!"                                                      } },
  { "M128", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "SfrCC2 reports warnings!"                                                    } },
  { "M129", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Option unknown: %OPT%'"                                                      } },
  { "M130", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Cannot create file '%NAME%'"                                                 } },
  { "M131", { MsgLevel::LEVEL_ERROR,    CRLF_B,   ""                                                                            } },
  { "M132", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "SfrCC2 report:\n%MSG%\nSfrCC2 report end.\n"                                 } },


// 200... Validation Errors
  { "M200", { MsgLevel::LEVEL_ERROR,    CRLF_B,   ""                                                                            } },
  { "M201", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Tag <%TAG%> unknown or not allowed on this level."                           } },
  { "M202", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Parse error: <%TAG%> = '%VALUE%'"                                            } },
  { "M203", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Value already set: <%TAG%> = '%VALUE%'"                                      } },
  { "M204", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Parse Error: '%VALUE%'"                                                      } },
  { "M205", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Tag <%TAG%> empty"                                                           } },
  { "M206", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "DerivedFrom not found: '%NAME%'"                                             } },
  { "M207", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Expression marker found but no <dim> specified: '%NAME%'"                    } },
  { "M208", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Ignoring <dimIndex> because specified <name> requires Array generation."     } },
  { "M209", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "CPU section not set. This is required for CMSIS Headerfile generation and debug support."} },
  { "M210", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Use new Format CMSIS-SVD >= V1.1 and add <CPU> Section."                     } },
  { "M211", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "Ignoring %LEVEL% %NAME% (see previous message)"                              } },    // i.e.: Ignoring Peripheral 'Foo'
  { "M212", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Address Block <usage> parse error: '%NAME%'"                                 } },
  { "M213", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Expression for '%NAME%' incomplete, <%TAG%> missing."                        } },    // <dim>, <dimIncrement>
  { "M214", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Peripheral '%NAME%' <dim> single-instantiation is not supported (use Array instead)."} },   //  ???  Reg%s vs Reg[%s]
  { "M215", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Size of <dim> is only one element for '%NAME%', is this intended?"           } },
  { "M216", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Unsupported character found in '%NAME%' : %HEX%."                            } },
  { "M217", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Forbidden Trigraph '??%CHAR%' found in '%NAME%'."                            } },
  { "M218", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Unsupported ESC sequence found in '%NAME%' : %CHAR%."                        } },
  { "M219", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "C Code generation error: %MSG%"                                              } },
  { "M220", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "C Code generation warning: %MSG%"                                            } },
  { "M221", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Input filename must end with .svd: '%NAME%"                                  } },
  { "M222", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Input filename has no extension: '%NAME%"                                    } },
  { "M223", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Input File Name '%INFILE%' does not match the tag <name> in the <device> section: '%NAME%'"} },   // MCU name
  { "M224", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Deprecated: '%NAME%' Use '%NAME2%' instead"                                  } },
  { "M225", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Upper/lower case mismatch: '%NAME%', should be '%NAME2%'"                    } },
  { "M226", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "SFD Code generation error: %MSG%"                                            } },
  { "M227", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "SFD Code generation warning: %MSG%"                                          } },
  { "M228", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Enumerated Value Container: Only one Item allowed on this Level!"            } },
  { "M229", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Register '%NAME%' is not an array, <dimArrayIndex> is not applicable"        } },
  { "M230", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Value '%NAME%:%NUM%' out of Range for %LEVEL% '%NAME2%[%NUM2%]'."            } },
  { "M231", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Value <isDefault> not allowed for %LEVEL%."                                  } },
  { "M232", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Tag <%TAG%> name '%NAME%' must not have specifier '%CHAR%'. Ignoring entry." } },
  { "M233", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Parse error: <%TAG%> = '%VALUE%'"                                            } },  // M202 as warning
  { "M234", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "No valid items found for %LEVEL% '%NAME%'"                                   } },
  { "M235", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "%LEVEL% '%NAME%' cannot be an array."                                        } },
  { "M236", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Expression for <%TAG%> '%NAME%' not allowed."                                } },
  { "M237", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Nameless %LEVEL% must have <%TAG%>."                                         } },
  { "M238", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "%LEVEL% must not have <%TAG%>."                                              } },
  { "M239", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Dim-ed %LEVEL% '%NAME%' must have an expression."                            } },
  { "M240", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Tag <%TAG%> unknown or not allowed on %LEVEL2%:%LEVEL%."                     } },
  { "M241", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Parse Error: '%VALUE%' invalid for Array generation"                         } },
  { "M242", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "%LEVEL% '%NAME%' <dimArrayIndex> found, but no <dim>"                        } },
  { "M243", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "%LEVEL% '%NAME%' <dimArrayIndex> found, but <dim> does not describe an array"} },
  { "M244", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "C Expression: Referenced item '%NAME%' not found from: '%MSG%'"              } },
  { "M245", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "C Expression: Level '%LEVEL%' not supported, change expression: '%NAME%'"    } },
  { "M246", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "C Expression: Only one Item allowed!"                                        } },
  { "M247", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "C Expression: Error occurred during generation!"                             } },
  { "M248", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "C Expression: Referenced Object must be Register or Field!"                  } },


  // 300... Data Check Errors
  { "M300", { MsgLevel::LEVEL_ERROR,    CRLF_B,   ""                                                                            } },
  { "M301", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Interrupt number '%NUM% : %NAME%' already defined: %NAME2% %LINE%"           } },
  { "M302", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Size of Register '%NAME%:%NUM%' must be 8, 16 or 32 Bits"                    } },
  { "M303", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "Register name '%NAME%' is prefixed with Peripheral name '%NAME2%'"           } },    // RegName = USART_CR ==> USART->USART_CR
  { "M304", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "Interrupt number overwrite: '%NUM% : %NAME%' %LINE%"                         } },
  { "M305", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Name not C compliant: '%NAME%' : %HEX%, replaced by '_'"                     } },
  { "M306", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Schema Version not set for <device>."                                        } },
  { "M307", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "Name is equal to Value: '%NAME%'"                                            } },
  { "M308", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Number of <dimIndex> Elements '%NUM%' is different to number of <dim> instances '%NUM2%'"} }, // <dimIndex>A,B,C != <dim>4
  { "M309", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Field '%NAME%': Offset error: %NUM%"                                         } },
  { "M310", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Field '%NAME%': BitWidth error: %NUM%"                                       } },
  { "M311", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Field '%NAME%': Calculation: MSB or LSB == -1"                               } },
  { "M312", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Address Block missing for Peripheral '%NAME%'"                               } },
  { "M313", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Field '%NAME%': LSB > MSB: BitWith calculates to %NUM%"                      } },
  { "M314", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Address Block: <offset> or <size> not set."                                  } },
  { "M315", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Address Block: <size> is zero."                                              } },
  { "M316", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "%LEVEL% <name> not set."                                                     } },    // LEVEL, = Peripheral, Cluster, Register, ...
  { "M317", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "%LEVEL% <description> not set."                                              } },
  { "M318", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "%LEVEL% '%NAME%' <%TAG%> is equal to <name>"                                 } },
  { "M319", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "%LEVEL% <%TAG%> '%NAME%' ends with newline, is this intended?"               } },
  { "M320", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "%LEVEL% <description> '%NAME%' is not very descriptive"                      } },  // name= "Foo", descr = "Foo Register"
  { "M321", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "%LEVEL% <%ITEM%> '%NAME%' starts with '_', is this intended?"                } },  // Peripheral <description> '_Foo'
  { "M322", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "%LEVEL% %ITEM% '%NAME%' is meaningless text. Deleted."                       } },  // description
  { "M323", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "%LEVEL% <%ITEM%> '%NAME%' contains text '%TEXT%'"                            } },  // Foo_Irq_IRQn
  { "M324", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Field '%NAME%' %BITRANGE% does not fit into Register '%NAME2%:%NUM%' %LINE%" } },
  { "M325", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "CPU Revision is not set"                                                     } },   // err or warn?
  { "M326", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Endianess is not set, using default (little)"                                } },
  { "M327", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "NVIC Prio Bits not set or wrong value, must be 2..8. Using default (4)"      } },   // err or warn?
  { "M328", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "%LEVEL% '%NAME%' has no Registers, ignoring %LEVEL%."                        } },    // <cluster> or <peripheral
  { "M329", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "CPU Type is not set, using default (Cortex-M3)"                              } },
  { "M330", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Interrupt '%NAME%' Number not set."                                          } },
  { "M331", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Interrupt '%NAME%' Number '%NUM%' greater or equal '%NAME2%' maximum Interrupts: '%NUM2%'."} },
  { "M332", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "%LEVEL% '%NAME%' has only one Register."                                     } },    // <cluster> or <peripheral
  { "M333", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Duplicate <enumeratedValue> %NUM%: '%NAME%' (%USAGE%), already used by '%NAME2%' (%USAGE2%) %LINE%"} },
  { "M334", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "%LEVEL% <%ITEM%> '%NAME%' is very long, use <description> and a shorter <name>"} },
  { "M335", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Value '%NAME%:%NUM%' does not fit into field '%NAME2%' %BITRANGE%."          } },
  { "M336", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "%LEVEL% '%NAME%' already defined %LINE%"                                     } },
  { "M337", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "%LEVEL% '%NAME%' already defined %LINE%"                                     } },
  { "M338", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Field '%NAME%' %BITRANGE% (%ACCESS%) overlaps '%NAME2%' %BITRANGE2% (%ACCESS2%) %LINE%"} },     // err or warn?
  { "M339", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Register '%NAME%' (%ACCESS%) (@%ADDRSIZE%) has same address or overlaps '%NAME2%' (%ACCESS2%) (@%ADDRSIZE2%) %LINE%"} }, // Register '%NAME%' (@%ADDRSIZE%) has same address or overlaps '%NAME2%' (@%ADDRSIZE2%) %LINE%" } },
  { "M340", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "No Devices found."                                                           } },
  { "M341", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "More than one devices found, only one is allowed per SVD File."              } },
  { "M342", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Dim-extended %LEVEL% '%NAME%' must not have <headerStructName>"              } },    // "name%s" (no array)
  { "M343", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "%LEVEL% '%NAME%' (@%ADDR%) has same address as '%NAME2%' %LINE%"             } },  // err or warn?  // <peripheral>, <cluster>
  { "M344", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Register '%NAME%' (@%ADDRSIZE%) is outside or does not fit any <addressBlock> specified for Peripheral '%NAME2%'\n%TEXT%"} }, // err or warn?   // TEXT = List of AddressBlocks
  { "M345", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Field '%NAME%' %BITRANGE% does not fit into Register '%NAME2%:%NUM%'"        } },
  { "M346", { MsgLevel::LEVEL_WARNING,  CRLF_B,   ""} }, //Register '%NAME%' (@%ADDR%) offset is equal or is greater than it's Peripheral base address '%NAME2%' (@%ADDR2%), is this intended?"} },
  { "M347", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "Field '%NAME%' (width < 6Bit) without any <enumeratedValue> found."          } },
  { "M348", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Alternate %LEVEL% '%NAME%' does not exist at %LEVEL% address (@%ADDR%)"      } },
  { "M349", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Alternate %LEVEL% '%NAME%' is equal to %LEVEL% name '%NAME2%'"               } },    // name=Foo, AltReg=Foo
  { "M350", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Peripheral '%NAME%' (@%ADDR%) is not 4Byte-aligned."                         } },
  { "M351", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "Peripheral %TYPE% '%NAME%' is equal to Peripheral name."                     } },
  { "M352", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "AddressBlock of Peripheral '%NAME%' (@%ADDR%) %TEXT% overlaps '%NAME2%' (@%ADDR2%) %TEXT2% %LINE%"} },
  { "M353", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Peripheral group name '%NAME%' should not end with '_'"                      } },
  { "M354", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Interrupt '%NUM%:%NAME%' specifies a Core Interrupt. Core Interrupts must not be defined, they are set through <cpu><name>."} },
  { "M355", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "No Interrupts found on pos. 0..15. External (Vendor-)Interrupts possibly defined on position 16+. External Interrupts must start on position 0"} },
  { "M356", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "No Interrupt definitions found."                                             } },
  { "M357", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Core Interrupts found. Interrupt Numbers are wrong. Internal Interrupts must not be described, External Interrupts must start at 0."} },
  { "M358", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "AddressBlock of Peripheral '%NAME%' %TEXT% overlaps AddressBlock %TEXT2% in same peripheral %LINE%"} },
  { "M359", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Address Block: <usage> not set."                                             } },
  { "M360", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Address Block: found <%TAG%> (%HEXNUM%) > %HEXNUM2%."                        } },    // offset, size
  { "M361", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "%LEVEL% %ITEM% '%NAME%': 'RESERVED' items must not be defined."              } },    // name, display name
  { "M362", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "%LEVEL% %ITEM% '%NAME%': 'RESERVED' items must not be defined."              } },    // description
  { "M363", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "CPU: <sauNumRegions> not set."                                               } },
  { "M364", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "CPU: <sauNumRegions> value '%NUM%' greater than SAU max num (%NUM2%)"        } },
  { "M365", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Register '%NAME%' (%ACCESS%) (@%ADDRSIZE%) has same address or overlaps '%NAME2%' (%ACCESS2%) (@%ADDRSIZE2%) %LINE%"} }, // M339: Register '%NAME%' (@%ADDRSIZE%) has same address or overlaps '%NAME2%' (@%ADDRSIZE2%) %LINE%" } },
  { "M366", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Register '%NAME%' size (%NUM%Bit) is greater than <dimIncrement> * <addressBitsUnits> (%NUM2%Bit)."} },
  { "M367", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "Access Type: Field '%NAME%' (%ACCESS%) does not match Register '%NAME2%' (%ACCESS2%)"} },
  { "M368", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "%LEVEL% '%NAME%' (@%ADDR%) has same address as '%NAME2%' %LINE%"             } }, // <peripheral>, <cluster>
  { "M369", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Enumerated Value '%NAME%': <value> not set."                                 } },
  { "M370", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "%LEVEL% '%NAME%': <offset> not set."                                         } },
  { "M371", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "%LEVEL% '%NAME%' <headerStructName> is equal to hierarchical name"           } },
  { "M372", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "%LEVEL% <%TAG%> '%NAME%' already defined %LINE%"                             } },
  { "M373", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "%LEVEL% <%TAG%> '%NAME%' already defined %LINE%"                             } },
  { "M374", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "<enumeratedValues> can be:\n"\
                                                           "  - One <enumeratedValues> container for all <enumeratedValue>s, where <usage> can be read, write, or read-write\n"\
                                                           "  - Two <enumeratedValues> container, where one is set to <usage>read and the other is set to <usage>write"} },
  { "M375", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "%LEVEL% '%NAME%' (<enumeratedValues> '%NAME2%'): Too many <enumeratedValues> container specified."} },
  { "M376", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "%LEVEL% '%NAME%' (<enumeratedValues> '%NAME2%'): '%USAGE%' container already defined in %LINE%."} },
  { "M377", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "%LEVEL% '%NAME%' (<enumeratedValues> '%NAME2%'): '%USAGE%' container conflicts with '%NAME3%' %LINE%."} },
  { "M378", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Register Array: Register '%NAME%' size (%NUM%Bit) does not match <dimIncrement> (%NUM2%Bit)."} },
  { "M379", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "XBin Number '%NAME%' too large, skipping evaluation."                        } },
  { "M380", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "AddressBlock of Peripheral '%NAME%' (@%ADDR%) %TEXT% does not fit into 32Bit Address Space."} },
  { "M381", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Interrupt '%NAME%' Number '%NUM%' greater or equal <deviceNumInterrupts>: '%NUM2%'."} },   // 28.03.2017
  { "M382", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "%LEVEL% '%NAME%': %NAME2% '%HEXNUM%' does not fit into %LEVEL% width: %NUM% Bit."             } },  // Register "foo": reset value '0xDEADBEEF' does not fit into register width: 32.
  { "M383", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Number of PMU Event Counters set but PMU present not set"                      } },   // 14.02.2020
  { "M384", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Number of PMU Event Counters (found val: '%NUM%') not set or outside range [2..31]. Ignoring PMU entry."} },   // 14.02.2020
  { "M385", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "PMU not supported for CPU '%NAME%'"                                         } },   // 14.02.2020
  { "M386", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Name not C compliant: '%NAME%' : Brackets [] found"                         } },
  { "M387", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "CPU: <sauNumRegions> set to 0 but regions are configured."               } },
  { "M388", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "MVE Floating-Point support is set but MVE is not set"                       } },
  { "M389", { MsgLevel::LEVEL_ERROR,    CRLF_B,   "Specified <deviceNumInterrupts>: '%NUM%' greater or equal '%NAME%': '%NUM2%'."} },   // 05.05.2020
  { "M390", { MsgLevel::LEVEL_WARNING3, CRLF_B,   "Checking IRQ '%NAME%': CPU unknown (see <cpu>). Assuming a maximum of %NUM% external Interrupts."} },    // 13.05.2020
  { "M391", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "CPU: configured number of SAU regions '%NUM%' greater than <sauNumRegions> value: '%NUM2%'"        } },

  // 500... SfrCC2 related Data modification Errors
  { "M500", { MsgLevel::LEVEL_ERROR,    CRLF_B,   ""                                                                            } },
  { "M517", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "SFD Code generation: Forbidden Trigraph '??%CHAR%' found in '%NAME%'."                            } },
  { "M516", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "SFD Code generation: Unsupported character found in '%NAME%' : %HEX%."                            } },
  { "M518", { MsgLevel::LEVEL_WARNING,  CRLF_B,   "SFD Code generation: Unsupported ESC sequence found in '%NAME%' : %CHAR%."                        } },
};

const MsgTableStrict SvdConv::msgStrictTable = {
  { "M208",  MsgLevel::LEVEL_ERROR    },
  { "M223",  MsgLevel::LEVEL_ERROR    },
  { "M225",  MsgLevel::LEVEL_ERROR    },

  { "M211",  MsgLevel::LEVEL_ERROR    },      // Ignoring ....
  { "M233",  MsgLevel::LEVEL_ERROR    },
  { "M234",  MsgLevel::LEVEL_ERROR    },

  { "M239",  MsgLevel::LEVEL_ERROR    },
  { "M241",  MsgLevel::LEVEL_ERROR    },

  { "M302",  MsgLevel::LEVEL_ERROR    },
  { "M306",  MsgLevel::LEVEL_ERROR    },
  { "M307",  MsgLevel::LEVEL_WARNING  },
  { "M322",  MsgLevel::LEVEL_ERROR    },
  { "M325",  MsgLevel::LEVEL_ERROR    },
  { "M327",  MsgLevel::LEVEL_ERROR    },
  { "M332",  MsgLevel::LEVEL_WARNING  },
  { "M333",  MsgLevel::LEVEL_ERROR    },
  { "M334",  MsgLevel::LEVEL_WARNING  },
  { "M335",  MsgLevel::LEVEL_ERROR    },
  { "M337",  MsgLevel::LEVEL_ERROR    },
  { "M338",  MsgLevel::LEVEL_ERROR    },
  { "M339",  MsgLevel::LEVEL_ERROR    },
  { "M343",  MsgLevel::LEVEL_ERROR    },
  { "M344",  MsgLevel::LEVEL_ERROR    },
  { "M348",  MsgLevel::LEVEL_ERROR    },
  { "M349",  MsgLevel::LEVEL_ERROR    },
  { "M351",  MsgLevel::LEVEL_WARNING  },
  { "M358",  MsgLevel::LEVEL_ERROR    },
  { "M360",  MsgLevel::LEVEL_ERROR    },
  { "M361",  MsgLevel::LEVEL_ERROR    },
  { "M371",  MsgLevel::LEVEL_ERROR    },
  { "M373",  MsgLevel::LEVEL_ERROR    },
  { "M379",  MsgLevel::LEVEL_ERROR    },
  { "M380",  MsgLevel::LEVEL_ERROR    },

  { "M382",  MsgLevel::LEVEL_ERROR    },
};
