/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ErrLog.h"
#include "Cbuild.h"

#define USAGE_STRING \
"Usage:\n\n" \
/*-------------------------------------------------------------------------------| */ \
"  %EXE% [-V] [--version] [-h] [--help]\n" \
"            <ProjectFile>.cprj <command> [<args>]\n" \
"\n" \
"    <ProjectFile>.cprj      : CMSIS Project Description file\n" \
"\n" \
"    <command> : packlist    : write the URLs of missing packs into\n" \
"                              <ProjectFile>.cpinstall\n" \
"              : cmake       : generate CMakeLists.txt\n" \
"\n" \
"       <args> : [--toolchain=<Toolchain> --update=<CprjFile>\n" \
"                 --intdir=<IntDir> --outdir=<OutDir> --quiet]\n" \
"\n" \
"                <Toolchain> : select the toolchain\n" \
"                <CprjFile>  : generate <CprjFile> with fixed versions\n" \
"                              for reproducing the current build\n" \
"                <IntDir>    : set intermediate directory\n" \
"                <OutDir>    : set output directory\n" \
"\n" \
"    <command> : extract     : export <Layer1> <Layer2> ... from\n" \
"                              <ProjectFile>.cprj into <OutDir> folder\n" \
"\n" \
"       <args> : [--layer=<Layer1> ... --layer=<LayerN>] --outdir=<OutDir>\n" \
"                <LayerN>    : optional layer ID\n" \
"                <OutDir>    : set output directory for extracted layers\n" \
"\n" \
"    <command> : remove      : delete <Layer1> <Layer2> ... from\n" \
"                              <ProjectFile>.cprj\n" \
"\n" \
"       <args> : --layer=<Layer1> ... --layer=<LayerN>\n" \
"                <LayerN>    : layer ID\n" \
"\n" \
"    <command> : compose     : generate a new <ProjectFile>.cprj from\n" \
"                              <1.clayer> <2.clayer> ...\n" \
"\n" \
"       <args> : <1.clayer> <2.clayer> ... <N.clayer>\n" \
"                [--name=<ProjectName> --description=<ProjectDescription>]\n" \
"\n" \
"                <N.clayer>           : layer description file\n" \
"                <ProjectName>        : project name string\n" \
"                <ProjectDescription> : project description string\n" \
"\n" \
"    <command> : add         : insert <1.clayer> <2.clayer> ... into\n" \
"                              <ProjectFile>.cprj\n" \
"\n" \
"       <args> : <1.clayer> <2.clayer> ... <N.clayer>\n" \
"                <N.clayer>  : layer description file\n" \
"\n" \
"Auxiliary Commands Usage:\n\n" \
"  %EXE% <command> <path1> ... <pathN> [<args>]\n" \
"\n" \
"    <command> : mkdir       : create directories including parents\n" \
"\n" \
"              : touch       : set access and modification time to current time,\n" \
"                              create file if it does not exist.\n" \
"\n" \
"              : rmdir       : remove directories and their contents recursively\n" \
"\n" \
"       <args> : [--except=<ChildPath>]\n" \
"                <ChildPath> : this file or child directory is exceptionally\n" \
"                              not deleted by rmdir command\n" \
"\n"


/*******
  Message levels
  --------------

  Message gets printed if level is greater or equal to set message-level (default: -w0, warning)

  // --debug
    MsgLevel::LEVEL_DEBUG     = 0,     // program debug information
  // --verbose2
    MsgLevel::LEVEL_INFO2     = 1,     // all processing infos
  // --verbose
    MsgLevel::LEVEL_PROGRESS  = 2,     // Progress Messages or Progress Info (i.e. "."), then shifted to MsgLevel::LEVEL_TEXT
    MsgLevel::LEVEL_INFO      = 3,     // Information
  // -w3
    MsgLevel::LEVEL_WARNING3  = 4,     // Warnings -w3
  // -w2
    MsgLevel::LEVEL_WARNING2  = 5,     // Warnings -w2
  // -w1
    MsgLevel::LEVEL_WARNING   = 6,     // Warnings -w1
  // -w0
    MsgLevel::LEVEL_ERROR     = 7,     // Errors
    MsgLevel::LEVEL_CRITICAL  = 8,     // Critical Errors
    MsgLevel::LEVEL_TEXT      = 9,     // Always printed if not --quiet
  // --quiet

 *******/

void InitMessageTableStrict();

void InitMessageTable()
{
  MsgTable table;

// 020 Constant Text (help, ...)
  table["M020"] = MessageEntry(MsgLevel::LEVEL_INFO,     CRLF_B,   USAGE_STRING                                                                  );    // Program Usage
  table["M021"] = MessageEntry(MsgLevel::LEVEL_INFO,     CRLF_NO,  "(%EXE%): %PROD% %VER% %TEXT%"                                                );    // Module Name, Version, Copyright
  table["M022"] = MessageEntry(MsgLevel::LEVEL_INFO,     CRLF_BE,  "Date: %DATE% Time: %TIME%"                                                   );    // Build timestamp
  table["M023"] = MessageEntry(MsgLevel::LEVEL_INFO,     CRLF_E,   "%EXE% %VER% %TEXT%"                                                          );    // EXE Name, Version, Copyright

// 200... Invocation Errors
  table["M200"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Invalid arguments!"                                                          );
  table["M201"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Too many arguments!"                                                         );
  table["M202"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "No CPRJ file was specified"                                                  );
  table["M203"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Error reading file '%PATH%'!"                                                );
  table["M204"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Path not found: '%PATH%'!"                                                   );
  table["M206"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "No command was specified!"                                                   );
  table["M207"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Multiple commands were specified!"                                           );
  table["M208"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Error copying file from '%ORIG%' to %DEST%!"                                 );
  table["M210"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Error writing file '%PATH%'!"                                                );
  table["M211"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Error creating directory '%PATH%'!"                                          );
  table["M212"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Error removing '%PATH%'!"                                                    );
  table["M213"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "'%VAR%' environment variable is not set!"                                    );
  table["M214"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "'extract' command requires --outdir option!"                                 );
  table["M215"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Project does not have layers!"                                               );
  table["M216"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Unable to get executable path %MSG%!"                                        );

  // 600... CMSIS Build Errors, warnings and messages
  table["M600"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Package index was not found in'%PATH%/.Web'!"                                );
  table["M601"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Package '%VENDOR%.%NAME%' was not found in package index!"                   );
  table["M602"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Package '%VENDOR%.%NAME%.%VER%' was not found!"                              );
  table["M603"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "No version of package '%VENDOR%.%NAME%' was found!"                          );
  table["M604"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Unresolved package component: %CMP%"                                         );
  table["M605"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Wrong CPRJ specification!"                                                   );
  table["M606"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Device '%DEV% (%VENDOR%)' was not found!"                                    );
  table["M607"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "RTE Model construction failed!"                                              );
  table["M608"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "No toolchain configuration file for '%NAME%' version '%VER%' was found!"     );
  table["M609"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Missing '%NAME%' element in CPRJ file!"                                      );
  table["M610"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Project supports multiple toolchains. Select one with the option --toolchain");
  table["M611"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Selected toolchain is not supported by the project!"                         );
  table["M613"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "File '%PATH%' has unexpected format!"                                        );
  table["M614"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "Missing access sequence delimiter: '%ACCSEQDELIM%'!"                         );
  table["M615"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "%PROP% '%VAL%' was not found in the loaded packs!"                           );

  table["M630"] = MessageEntry(MsgLevel::LEVEL_WARNING,  CRLF_B,   "Device '%DEV%' is substituted with variant '%VAR%'."                         );
  table["M631"] = MessageEntry(MsgLevel::LEVEL_WARNING,  CRLF_B,   "Project must have exactly one target element!"                               );
  table["M632"] = MessageEntry(MsgLevel::LEVEL_WARNING,  CRLF_B,   "Attribute '%ATTR%' required by access sequence '%ACCSEQ%' was not found!"    );
  table["M633"] = MessageEntry(MsgLevel::LEVEL_WARNING,  CRLF_B,   "Unknown access sequence: '%ACCSEQ%'."                                        );

  table["M650"] = MessageEntry(MsgLevel::LEVEL_INFO,     CRLF_B,   "Command completed successfully."                                             );
  table["M652"] = MessageEntry(MsgLevel::LEVEL_INFO,     CRLF_B,   "Generated file for project build: '%NAME%'"                                  );
  table["M653"] = MessageEntry(MsgLevel::LEVEL_INFO,     CRLF_B,   "Local config file %NAME% was not found. Copying default file from package."  );
  table["M654"] = MessageEntry(MsgLevel::LEVEL_INFO,     CRLF_B,   "Package '%PACK%' was added to the list of missing packages."                 );
  table["M655"] = MessageEntry(MsgLevel::LEVEL_INFO,     CRLF_B,   "'%VAR%' environment variable was not set!"                                   );
  table["M656"] = MessageEntry(MsgLevel::LEVEL_INFO,     CRLF_B,   "Package '%VENDOR%.%NAME%.%VER%' was found in local repository '%PATH%'!"     );
  table["M657"] = MessageEntry(MsgLevel::LEVEL_INFO,     CRLF_B,   "Generated updated project description file: '%NAME%'"                        );

  // 800... Model Errors
  table["M800"] = MessageEntry(MsgLevel::LEVEL_ERROR,    CRLF_B,   "RTE Model reports: %MSG%"                                                    );

  PdscMsg::AddMessages(table);
  InitMessageTableStrict();
}


void InitMessageTableStrict()
{
  MsgTableStrict table;

  PdscMsg::AddMessagesStrict(table);
}
