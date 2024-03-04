//++
// emulib.cpp -> general purpose emulator library routines
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
//   This file contains a random collection of small routines that have no better
// place to go.  They're all things that I wanted to share between emulator
// projects, but none are complicated enough to be worth making a class around
// them.  Prototypes for these functions are in EMULIB.hpp.
//
// Bob Armstrong <bob@jfcl.com>   [10-DEC-2015]
//
// REVISION HISTORY:
// 10-DEC-15  RLA   New file.
// 28-FEB-17  RLA   Make 64 bit clean.
//  1-JUN-17  RLA   Linux port.
// 26-AUG-22  RLA   Clean up Linux/WIN32 conditionals.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <limits.h>             // needed for PATH_MAX on Linux ...
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <time.h>               // time() ...
#include <string.h>             // strcpy(), dirname(), basename(), etc ...
#include <stdarg.h>		// va_list(), va_start(), va_end(), ...
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <windows.h>            // can't live without this anymore ...
#include <process.h>            // needed for GetProcessAffinityMask(), et al ...
#include <mmsystem.h>           // time API functions (e.g. timeBeginPeriod())
#include <winsock2.h>           // socket interface declarations ...
#pragma comment(lib, "ws2_32.lib")  // force the winsock2 to be loaded
#pragma comment(lib, "Winmm.lib")   // force the multimedia library to be loaded
#elif defined(__linux__)
#include <unistd.h>             // usleep(), access(), R_OK, etc ...
#include <netinet/in.h>         // Internet in_addr definitions
#include <libgen.h>
#endif
#include "EMULIB.hpp"           // emulator library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "LogFile.hpp"          // emulator library message logging facility

// Local member variables ...
#if defined(_WIN32)
static unsigned int m_uTimerResolution = 0; // high resolution timer setting
#endif

void _sleep_ms(uint32_t nDelay)
{
  //++
  // Sleep (delay) for the specified number of milliseconds...
  //--
#if defined(_WIN32)
  Sleep(nDelay);
#elif defined(__linux__)
  usleep(nDelay*1000);
#endif
}

void CheckAffinity(void)
{
  //++
  //   This little routine will check if the current process affinity setting
  // prevents us from running on all available processors.  If it does, we'll
  // issue a warning (we could just change the affinity setting too, but for
  // now we'll assume the operator knows what he is doing).
  //--
#if defined(_WIN32)
  DWORD_PTR dwProcessAffinity, dwSystemAffinity;
  GetProcessAffinityMask(GetCurrentProcess(), &dwProcessAffinity, &dwSystemAffinity);
  if (dwProcessAffinity != dwSystemAffinity)
    LOGS(WARNING, "CURRENT AFFINITY SETTING BLOCKS USE OF ALL PROCESSORS!");
#elif defined(__linux__)
  // Currently there is no Linux version!!
  assert(false);
#endif
}

void SetTimerResolution (unsigned int uResolution)
{
  //++
  //   This routine will attempt to change the resolution of the Windows system
  // timer.  This usually defaults to 15.6ms per tick but that tradition dates
  // back to the original PC.  Most modern systems will allow the resolution to
  // be set as low as 1ms.
  //
  //   Of course this is not without its problems - increasing the frequence of
  // timer ticks increases system overhead and of course that increases power
  // consumption.  That could cause dynamic frequency scaling to reduce the CPU
  // clock frequency to keep power dissipation within safe limits, and that
  // could actually make things SLOWER!  Use this at your own risk...
  //
  //   The main program should call RestoreTimerResolution() before exiting to
  // restore the original timer setting, but in practice this isn't absolutely
  // necessary.  Windows seems to clean up even if this process terminates
  // abormally.
  //
  //   One last comment - the timeXYZ() calls are actually part of the Windows
  // Multimedia subsystem, so you'll need to link with WinMM.lib.
  //--
#if defined(_WIN32)
  TIMECAPS tc;
  m_uTimerResolution = 0;
  if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) goto failed;
  m_uTimerResolution = MIN(MAX(tc.wPeriodMin, uResolution), tc.wPeriodMax);
  if (timeBeginPeriod(m_uTimerResolution) != TIMERR_NOERROR) goto failed;
  LOGF(DEBUG, "Windows system timer resolution set to %dms", m_uTimerResolution);
  return;

failed:
  LOGS(WARNING, "UNABLE TO CHANGE SYSTEM TIME RESOLUTION!");
#elif defined(__linux__)
  // Currently there is no Linux version!
  assert(false);
#endif
}

void RestoreTimerResolution (void)
{
  //++
  // Restore the origina Windows timer resolution from SetTimerResolution() ...
  //--
#if defined(_WIN32)
  if (m_uTimerResolution != 0) timeEndPeriod(m_uTimerResolution);
  m_uTimerResolution = 0;
#elif defined(__linux__)
  // Currently there is no Linux version!
  assert(false);
#endif
}

bool SplitPath (const char *pszPath, string &sDrive, string &sDirectory, string &sFileName, string &sExtension)
{
  //++
  //   Split pszPath into its individual components - drive, directory, file
  // name and extension.  This is just a wrapper for the standard _splitpath()
  // function, but with C++ std::string arguments.
  //
  //   On Linux the dirname() and basename() functions give us the path and
  // filename components, however we have to parse the filename ourselves to
  // extract the extension (Linux is a bit ambivalent about that concept).
  // The drive name is always empty on Linux.
  //--
  assert(pszPath != NULL);
#if defined(_WIN32)
  // The Windows version ...
  char szDrive[_MAX_DRIVE+1], szDirectory[_MAX_DIR+1];
  char szFileName[_MAX_FNAME+1], szExtension[_MAX_EXT+1];
  errno_t err = _splitpath_s(pszPath, szDrive, sizeof(szDrive),
    szDirectory, sizeof(szDirectory), szFileName, sizeof(szFileName),
    szExtension, sizeof(szExtension));
  if (err != 0) return false;
  sDrive = szDrive;  sDirectory = szDirectory;
  sFileName = szFileName;  sExtension = szExtension;
#elif defined(__linux__)
  //   And now the Linux version.  Note that the dirname() and basename()
  // library functions are a bit, well, funky.  dirname() modifies the caller's
  // buffer by replacing the last "/" in the path with a null and then it
  // returns a pointer to start of the string.  basename() doesn't modify the
  // buffer it's passed and just returns a pointer to the first character after
  // the last "/" in the name.  Calling dirname() before basename() doesn't
  // work because the latter will be unable to find the actual file name after
  // dirname terminates the string just in front of it.  Calling them in the
  // reverse order, however, works fine.
  char szPath[PATH_MAX];
  strcpy_s(szPath, sizeof(szPath), pszPath);
  char *pszFileName = basename(szPath);
  char *pszDirectory = dirname(szPath);
  sDrive.clear();  sExtension.clear();  sDirectory = pszDirectory;
  //   Now we play the same game ourselves to separate the extension (if any)
  // from the file name.  Note that we extract the extension string BEFORE
  // replacing the "." with an EOS because the dot is part of the extension!
  char *pszExtension = strrchr(pszFileName, '.');
  if (pszExtension != NULL) {
    sExtension = pszExtension;  *pszExtension = '\0';  sFileName = pszFileName;
  } else {
    sFileName = pszFileName;  sExtension.clear();
  }
#endif
  return true;
}

string MakePath (const char *pszDrive, const char *pszDirectory, const char *pszFileName, const char *pszExtension)
{
  //++
  //   Combine the components of a path - drive, directory, file name and
  // extension - into a single path.  This is just a wrapper for the standard
  // _makepath() function, but with a std::string return value.
  //
  //   Linux has no direct equivalent to _makepath(), but we hardly need one.
  // It's trivial to do with string concatenation; we just have to be careful
  // to get all the slashes and dots in the right places.  Note that the drive
  // name is ignored on Linux!
  //--
#if defined(_WIN32)
  char szPath[_MAX_PATH+1];
  errno_t err = _makepath_s(szPath, sizeof(szPath),
    pszDrive, pszDirectory, pszFileName, pszExtension);
  if (err != 0) return string("");
  return string(szPath);
#elif defined(__linux__)
  string sPath = string(pszDirectory) + "/"
               + string(pszFileName)
               + string(pszExtension);
  return sPath;
#endif
}

string FullPath (const char *pszRelativePath)
{
  //++
  //   Return the fully qualified, absolute, path for a relative path
  // specification.  This is just a wrapper for the _fullpath() function,
  // but with a std::string return value.
  //
  //   Note that if you pass _fullpath() a null string, it will return the
  // current working directory.  I guess that makes sense, but it doesn't
  // work for our needs, so there's a special case for that.  In our usage,
  // a null argument returns a null result.
  //
  //   This is suprisingly easy on Linux, as the _realpath() function does
  // pretty much what we want.  The same comments about a null file name
  // returning a null result applies there, too.
  //--
  if (strlen(pszRelativePath) == 0) return string("");
#if defined(_WIN32)
  char szAbsolutePath[_MAX_PATH+1];
  if (_fullpath(szAbsolutePath, pszRelativePath, sizeof(szAbsolutePath)) == NULL) return string("");
#elif defined(__linux__)
  char szAbsolutePath[PATH_MAX];
  if (realpath(pszRelativePath, szAbsolutePath) == NULL) {
    //   realpath() will return an error status if the specified file doesn't
    // exist, but that's not a problem for us.  Fortunately realpath() does
    // return a valid result in that case, so just go ahead and return it.
    // In the event of any other error, we just return the original name.
    if (errno != ENOENT) return string(pszRelativePath);
  }
#endif
  return string(szAbsolutePath);
}

bool FileExists (const char *pszPath)
{
  //++
  // Return TRUE if the file and path exists ...
  //--
#if defined(_WIN32)
  DWORD dwAttrib = GetFileAttributesA(pszPath);
  if (dwAttrib == INVALID_FILE_ATTRIBUTES) return false;
  if (ISSET(dwAttrib, FILE_ATTRIBUTE_DIRECTORY)) return false;
  return true;
#elif defined(__linux__)
  return (access(pszPath, F_OK) == 0);
#endif
}

string FormatString (const char *pszFormat, ...)
{
  //++
  //   This routine will use printf() style formatting to create a C++ string.
  // Seems like there ought to be some std::string method to do this already,
  // but there isn't.  Probably because C++ types don't like printf() very
  // much (it's not type safe, after all), but it's REALLY handy!
  //--
  char sz[256];  va_list args;  va_start(args, pszFormat);
  vsprintf_s(sz, sizeof(sz), pszFormat, args);
  va_end(args);
  return sz;
}

void FormatString (string &sResult, const char *pszFormat, ...)
{
  //++
  //   This is the same as the previous routine, except this time the result
  // is stored in an existing string...
  //--
  char sz[256];  va_list args;  va_start(args, pszFormat);
  vsprintf_s(sz, sizeof(sz), pszFormat, args);
  va_end(args);
  sResult = sz;
}

string FormatIPaddress (uint32_t lIP, uint16_t nPort)
{
  //++
  //   Format an IP address and port in the format "w.x.y.z:pppp" .  It's
  // a bit ugly (OK, it's a lot ugly!) but it's just for error messages...
  //--
  char buf[64];
  if (nPort != 0) {
    sprintf_s(buf, sizeof(buf), "%u.%u.%d.%u:%u",
              (lIP >> 24) & 0xFF, (lIP >> 16) & 0xFF,
              (lIP >>  8) & 0xFF,  lIP        & 0xFF, nPort);
  } else {
    sprintf_s(buf, sizeof(buf), "%u.%u.%u.%u",
              (lIP >> 24) & 0xFF, (lIP >> 16) & 0xFF,
              (lIP >>  8) & 0xFF,  lIP        & 0xFF);
  }
  return string(buf);
}

string FormatIPaddress (const sockaddr_in *p)
{
  //++
  //   Format the remote address and port in the format "w.x.y.z:pppp" .  It's
  // a bit ugly (OK, it's a lot ugly!) but it's just for error messages...
  //--
  if ((p != NULL) && (p->sin_family == AF_INET))
    return FormatIPaddress(ntohl(p->sin_addr.s_addr), ntohs(p->sin_port));
  else
    return string("???.???.???.???:????");
}

string FormatIPaddress (const sockaddr *p)
{
  //++
  //   Verify that the socket address type is AF_INET and then format the
  // address as a dotted IP.  If the type is anything else, punt...
  //--
  if ((p != NULL) && (p->sa_family == AF_INET)) {
    char sz[64];
    sprintf_s(sz, sizeof(sz), "%u.%u.%u.%u",
      LOBYTE(p->sa_data[2]), LOBYTE(p->sa_data[3]),
      LOBYTE(p->sa_data[4]), LOBYTE(p->sa_data[5]));
    return string(sz);
  } else
    return string("???.???.???.???");
}

bool ParseIPaddress (const char *pszAddr, uint32_t &lIP, uint16_t &nPort)
{
  //++
  //   This method will parse a string with the format "a.b.c.d:p" and return
  // IP address and port number.  It accepts several variations on the same
  // basic syntax -
  //
  //   a.b.c.d:p -> set the IP address and port number
  //   a.b.c.d   -> set only the IP and leave nPort unchanged
  //   p         -> set only the port number and leave lIP unchanged
  //   :p        -> ditto
  //
  // Any other syntax returns FALSE and leaves both lIP and nPort unchanged.
  // Note that only dotted IP addresses (e.g. 127.0.0.1) are accepted and host
  // names (e.g. localhost) are not.
  //--
  if ((pszAddr == NULL) || (strlen(pszAddr) == 0)) return false;

  // Check for the "p" and ":p" cases first ...
  char *pszEnd;
  if (*pszAddr == ':') {
    if (!isdigit(*++pszAddr)) return false;
    uint32_t l = strtoul(pszAddr, &pszEnd, 10);
    if (*pszEnd != '\0') return false;
    nPort = LOWORD(l);  return true;
  }
  uint32_t l = strtoul(pszAddr, &pszEnd, 10);
  if (*pszEnd == '\0') {
    nPort = LOWORD(l);  return true;
  }

  // Handle the cases which contain a dotted IP ...
  unsigned int a, b, c, d, p, nLength=0;
  int nCount = sscanf_s(pszAddr, "%d.%d.%d.%d%n:%d%n", &a, &b, &c, &d, &nLength, &p, &nLength);
  if ((nCount < 4) || (nLength != strlen(pszAddr))) return false;
  lIP = ((a & 0xFF) << 24) | ((b & 0xFF) << 16)
    | ((c & 0xFF) <<  8) |  (d & 0xFF);
  if (nCount == 5) nPort = LOWORD(p);
  return true;
}

string DumpBuffer (const char *pszTitle, const uint8_t *pabBuffer, size_t cbBuffer)
{
  //++
  //   Dump a buffer (or really any array of bytes) to the output string in
  // both hex and ASCII.  This is normally used for debug messages to dump the
  // buffer for devices like IDE or TU58 drives, but you can use it for
  // whatever you like.
  //--
  size_t cbTitle = strlen(pszTitle);
  size_t cbDashes = (72-(cbTitle+2)) / 2;
  string sDashes(cbDashes, '-');
  string sDump = FormatString("%s %s %s\n",sDashes.c_str(), pszTitle, sDashes.c_str());
  for (uint16_t nStart = 0; nStart < cbBuffer; nStart += 16) {
    sDump += FormatString("%03X/ ", nStart);
    uint16_t cb = MIN(16, (uint16_t) (cbBuffer-nStart));
    for (uint16_t i = 0; i < cb; ++i)
      sDump += FormatString("%02X ", pabBuffer[nStart+i]);
    for (uint16_t i = cb;  i < 16;  ++i)  sDump += "   ";
    sDump += "\t";
    for (uint16_t i = 0; i < cb; ++i) {
      uint8_t b = pabBuffer[nStart + i] & 0x7F;
      sDump += FormatString("%c", isprint(b) ? b : '.');
    }
    sDump += "\n";
  }
  return sDump;
}
