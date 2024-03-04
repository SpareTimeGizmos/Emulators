//++
// EMULIB.hpp -> Global declarations for the emulator library
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the emulator library project.  EMULIB is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    EMULIB is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with EMULIB.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This file contains global constants and universal macros for the emulator
// library.  It's used by all programs.
//
// Bob Armstrong <bob@jfcl.com>   [20-MAY-2015]
//
// REVISION HISTORY:
// 20-MAY-15  RLA   New file.
// 15-JAN-20  RLA   Add inline SplitPath(string sPath, ...
//--
#pragma once
#include <string>             // C++ std::string class, et al ...
using std::string;            // this is used EVERYWHERE!

// Constants and parameters ...
#define EMUVER       152      // library version number ...

//  These macros are used with fuctions that are outside of a class definition.
// "PRIVATE" methods are local to the source file where they live, and "PUBLIC"
// are global...
#define PRIVATE static
#define PUBLIC

// Bit equates (for convenience) ...
#define BIT0    0x0001
#define BIT1    0x0002
#define BIT2    0x0004
#define BIT3    0x0008
#define BIT4    0x0010
#define BIT5    0x0020
#define BIT6    0x0040
#define BIT7    0x0080
#define BIT8    0x0100
#define BIT9    0x0200
#define BIT10   0x0400
#define BIT11   0x0800
#define BIT12   0x1000
#define BIT13   0x2000
#define BIT14   0x4000
#define BIT15   0x8000

// ASCII character codes used by the console window and TELNET classes ...
#define CHNUL        0x00     // null
#define CHBEL        0x07     // bell
#define CHBSP        0x08     // backspace
#define CHTAB        0x09     // horizontal tab
#define CHFFD        0x0C     // form feed
#define CHLFD        0x0A     // line feed
#define CHCRT        0x0D     // carriage return
#define CHEOF        0x1A     // control-Z
#define CHESC        0x1B     // escape
#define CHDEL        0x7F     // delete key for some terminals

//   Sadly, windef.h defines these macros too - fortunately Microsoft's
// definition is compatible with ours, so it's not a fatal problem.
// Unfortunately, Microsoft's header doesn't check to see whether these
// things are defined before redefining them, and that leads to a
// compiler error message.
#if !(defined(_WIN32) && defined(LOBYTE))
#define LOBYTE(x) 	((uint8_t)  ((x) & 0xFF))
#define HIBYTE(x) 	((uint8_t)  (((x) >> 8) & 0xFF))
#define LOWORD(x) 	((uint16_t  ( (x) & 0xFFFF)))
#define HIWORD(x)	((uint16_t) (((x) & 0xFFFF0000L) >> 16) )
#endif

// Assemble and disassemble nibbles, bytes, words and longwords...
#define MASK1(x)        ((x) & 0x01)
#define MASK3(x)        ((x) & 0x07)
#define MASK4(x)        ((x) & 0x0F)
#define MASK8(x)        ((x) & 0xFF)
#define MASK12(x)       ((x) & 0xFFF)
#define MASK13(x)       ((x) & 0x1FFF)
#define MASK15(x)       ((x) & 0x7FFF)
#define MASK16(x)       ((x) & 0xFFFF)
#define MASK32(x)       ((x) & 0xFFFFFFFFUL)
#define LONIBBLE(x)	((x) & 0x0F)
#define HINIBBLE(x)	(((x) >> 4) & 0x0F)
#define MKBYTE(h,l)     ((uint8_t)  ((((h) &  0xF) << 4) | ((l) &  0xF)))
#define MKWORD(h,l)	((uint16_t) ((((h) & 0xFF) << 8) | ((l) & 0xFF)))
#define MKLONG(h,l)	((uint32_t) (( (uint32_t) ((h) & 0xFFFF) << 16) | (uint32_t) ((l) & 0xFFFF)))
#define MKQUAD(h,l)	((uint64_t) (( (uint64_t) ((h) & 0xFFFFFFFF) << 32) | (uint64_t) ((l) & 0xFFFFFFFF)))

//   This macro converts a 64 bit quantity (e.g. a size_t) to a 32 bit integer
// when we're targeting a 64 bit platform.  This is unfortunately necessary
// for some legacy cases (e.g. the second argument to fgets()).
#define MKINT32(x)      ((int) ((x) & 0xFFFFFFFF))

// Bit set, clear and test macros ...
#define SETBIT(x,b)	x |=  (b)
#define CLRBIT(x,b)	x &= ~(b)
#define CPLBIT(x,b)     x ^=  (b)
#define ISSET(x,b)	(((x) & (b)) != 0)

// Useful arithmetic macros ...
#define MAX(a,b)  ((a) > (b) ? (a) : (b))
#define MIN(a,b)  ((a) < (b) ? (a) : (b))
#define ISODD(a)  (((a) & 1) != 0)
#define ISEVEN(a) (((a) & 1) == 0)

// Useful shorthand for string comparisons ...
#define STREQL(a,b)     (strcmp(a,b) == 0)
#define STRNEQL(a,b,n)  (strncmp(a,b,n) == 0)
#define STRIEQL(a,b)    (_stricmp(a,b) == 0)
#define STRNIEQL(a,b,n) (_strnicmp(a,b,n) == 0)

//   Define the C++ new operator to use the debug version. This enables tracing
// of memory leaks via _CrtDumpMemoryLeaks(), et al.  It'd be really nice if
// Visual Studio was smart enough to just do this for us, but it isn't ...
#if defined(_DEBUG) && defined(_MSC_VER)
#define DBGNEW new( _CLIENT_BLOCK, __FILE__, __LINE__)
#else
#define DBGNEW new
#endif

// Prototypes for routines declared in emulib.cpp ...
extern "C" void _sleep_ms (uint32_t nDelay);
extern "C" void CheckAffinity (void);
extern "C" void SetTimerResolution (unsigned int uResolution);
extern "C" void RestoreTimerResolution (void);
// FormatString() ...
extern string FormatString (const char *pszFormat, ...);
extern void FormatString (string &sResult, const char *pszFormat, ...);
// SplitPath() ...
extern bool SplitPath (const char *pszPath, string &sDrive, string &sDirectory, string &sFileName, string &sExtension);
inline bool SplitPath (const string sPath, string &sDrive, string &sDirectory, string &sFileName, string &sExtension)
  {return SplitPath(sPath.c_str(), sDrive, sDirectory, sFileName, sExtension);}
// MakePath() ...
extern string MakePath (const char *pszDrive, const char *pszDirectory, const char *pszFileName, const char *pszExtension);
inline string MakePath (const string sDrive, const string sDirectory, const string sFileName, const string sExtension)
  {return MakePath(sDrive.c_str(), sDirectory.c_str(), sFileName.c_str(), sExtension.c_str());}
// FullPath() ...
extern string FullPath (const char *pszRelativePath);
inline string FullPath (const string sRelativePath) {return FullPath(sRelativePath.c_str());}
// FileExists() ...
extern bool FileExists (const char *pszPath);
inline bool FileExists (const string sPath) {return FileExists(sPath.c_str());}
// ParseIPaddress() and FormatIPaddress() ...
extern bool ParseIPaddress (const char *pszAddr, uint32_t &lIP, uint16_t &nPort);
extern string FormatIPaddress (uint32_t lIP, uint16_t nPort=0);
extern string FormatIPaddress (const struct sockaddr_in *p);
extern string FormatIPaddress (const struct sockaddr *p);
// Dump buffers in HEX and ASCII ...
extern string DumpBuffer (const char *pszTitle, const uint8_t *pabBuffer, size_t cbBuffer);
