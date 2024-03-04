//++
// SafeCRT.h -> Microsoft "Safe C Run Time" replacements
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
//   This header file defines a few macros and inline functions that replace
// some the Microsoft "safe" C runtime functions - e.g. all those that end in
// "_s", strcat_s, strcpy_s, etc - with Linux/gcc/glib equivalents.  Some of
// the replacements require more than just a trivial amount of code, though,
// and you'll find them in the SafeCRT.c file.
//
// Bob Armstrong <bob@jfcl.com>   [26-MAY-2017]
//
// REVISION HISTORY:
// 26-MAY-17  RLA   New file.
// 26-AUG-22  RLA   Clean up Linux/WIN32 conditionals.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#if !defined(_WIN32)          // don't need any of this for Microsoft C!
#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//   You'll find replacements for strcpy_s() and strcat_s() in SafeCRT.c.  We
// just define the prototypes here, and the associated Microsoft only "magic"
// constants, _TRUNCATE and STRUNCATE.  Note that the latter is an errno and
// it's not completely clear which value we should use on Linux, so we more or
// less arbitrarily pick ERANGE...
#define _TRUNCATE ((size_t) -1)
#define STRUNCATE ERANGE
extern int strcpy_s (char *pszDst, size_t cbDst, const char *pszSrc);
extern int strcat_s (char *pszDst, size_t cbDst, const char *pszSrc);

//  strncpy_s() and strncat_s() are easy to emulate, assuming we only allow
// the _TRUNCATE behavior (that's all we ever use)...
//inline int strncpy_s(char *pszDst, size_t cbDst, const char *pszSrc, size_t cbSrc)
//  {assert(cbSrc == _TRUNCATE);  return strcpy_s(pszDst, cbDst, pszSrc);}
//inline int strncat_s(char *pszDst, size_t cbDst, const char *pszSrc, size_t cbSrc)
//  {assert(cbSrc == _TRUNCATE);  return strcat_s(pszDst, cbDst, pszSrc);}

//  The _stricmp() and _strnicmp() functions are equivalent to strcasecmp()
// and strncasecmp() - only the names are different...
#define _stricmp(str1,str2) strcasecmp(str1,str2)
#define _strnicmp(str1,str2,n) strncasecmp(str1,str2,n)

//   The safe replacement for sprintf_s() is snprintf().  It'd be nice if we
// could just use snprintf() directly in our code, but unfortunately the MS
// version of snprintf() isn't quite compatible with the "standard" version.
// Our only choice is to call sprintf_s() in the code and replace it with
// snprintf() here...
#define sprintf_s(dst,len,fmt,...) snprintf(dst,len,fmt, ##__VA_ARGS__)
#define vsprintf_s(dst,len,fmt,...) vsnprintf(dst,len,fmt, ##__VA_ARGS__)

//   I'm not sure how sscanf_s() differs from sscanf, but Visual Studio whines
// if we use the latter. Ditto for fscanf_s() ...
#define sscanf_s(buf,fmt,...) sscanf(buf,fmt, ##__VA_ARGS__)
#define fscanf_s(f,fmt,...)   fscanf(f,fmt, ##__VA_ARGS__)

//   It's not really clear to me why fopen_s() is "safer" than fopen(),
// because all it really does is to change around the calling ssequence.
// I will say that the fopen_s() behavior of returning the error code
// rather than a file pointer is a lot more useful!
inline int fopen_s(FILE **pFile, const char *pszName, const char *pszMode)
  {return ((*pFile=fopen(pszName, pszMode)) == NULL) ? errno : 0;}

//   Linux recently added a strerr() function with bounds checking, but
// unfortunately they called it strerror_r() instead of strerror_s().
// It's also a little different - it's allowed to return a pointer to an
// immutable string and not use the caller's buffer!
int strerror_s (char *pszBuffer, size_t cbBuffer, int nError);

//   Unfortunately, Linux/gcc not only lacks _itoa_s(), but it also lacks
// even iota()!  snprintf(), et al, is not really a suitable replacement
// since itoa() allows any radix to be specified.  We just write our own
// replacement here.  BTW, note that the real Windows _itoa_s() returns
// an errno_t, but we don't bother with that here...
int _itoa_s (int nValue, char *pszBuffer, size_t cbBuffer, int nRadix);

#if defined(__cplusplus)
}
#endif

#endif	// #ifndef _WIN32
