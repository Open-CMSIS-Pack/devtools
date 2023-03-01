/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CROSSPLATFORM_H
#define CROSSPLATFORM_H

// Platform-specific API,  C-runtime methods and includes
#ifdef _WIN32

#include <windows.h>
#include <tchar.h>

#else

#pragma GCC diagnostic ignored "-Wwrite-strings"      // disable warning: deprecated conversion from string constant to 'char *'

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdarg>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// Substitute POSIX equivalents of Windows macros/functions
#define _strnicmp strncasecmp
#define _stricmp  strcasecmp

#define _popen popen
#define _pclose pclose

/* Windows defines */
#define MAX_PATH  260
#define _T(text)  text

/* Windows basic types */
typedef unsigned long DWORD;
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef char _TCHAR, TCHAR;
typedef char *LPSTR;
typedef uint64_t UINT64;
typedef int64_t INT64;
typedef uint32_t UINT;
typedef char *LPVOID;
typedef DWORD HMODULE;
typedef int errno_t;

typedef signed int INT32, *PINT32;

/* Function prototypes  */
#endif  /* #ifdef _WIN32 */

#endif  /* #ifndef CROSSPLATFORM_H */
