//++
// RTC.cpp -> generic non-volatile RAM and real time clock emulation
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
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
//   This module is a generic implementation of a real time clock and non-
// volatile RAM chip, such as the DS12887 or the CDP1879.  Exactly how the
// RTC should be implemented is up for discussion.  Should it get the real
// time of day from the host and always return the actual wall clock time?
// Or should it keep track of simulated time and tick off simulated seconds
// without regard to the real time?  It's more useful, at least for me, to
// have it return the real, wall clock, time.  That way time stamps for the
// simulated operating system actually make sense in the real world.
//
//   Along those lines, this module implements methods to get the current
// date and time from the operating system and then store the results in
// the hours, minutes, seconds, day, month, year and weekday members.
// These values can be stored in either BCD or pure binary and the hours
// can be in either 24 or 12 hour format.
//
//   Some chips also implement several bytes of general purpose SRAM that
// are also backed up by the RTC battery.  This module can also emulate
// that, and the number of bytes of NVR is specified in the call to the
// constructor.  This size MAY BE zero if no such general purpose RAM
// exists, as is the case for the CDP1879. The SaveNVR() and LoadNVR()
// methods are provided for saving or loading the NVR to or from a file,
// but these are not called automatically.  If you want the NVR to really
// be non-volatile, then it's up to you to call them at the right times.
//    
// REVISION HISTORY:
// 20-JUN-22  RLA   Generalized from the DS12887 version.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <cstring>              // needed for memset()
#include <time.h>               // _localtime64_s(), localtime_r(), ...
#include <sys/timeb.h>          // struct __timeb, ftime(), etc ...
#include "EMULIB.hpp"           // emulator library definitions
#include "SafeCRT.h"            // replacements for some Microsoft functions
#include "LogFile.hpp"          // emulator library message logging facility
#include "RTC.hpp"              // declarations for this module


CRTC::CRTC (uint16_t cbNVR)
{
  //++
  // Initialize all locals and allocate the RAM space, if required...
  //--
  m_cbNVR = cbNVR;  m_pbNVR = NULL;
  if (m_cbNVR > 0) {
    m_pbNVR = DBGNEW uint8_t[m_cbNVR];
    ClearNVR();
  }
}

CRTC::~CRTC()
{
  //++
  // The destructor needs to release the NVR RAM, if any ...
  //--
  if (m_cbNVR) {
    assert(m_pbNVR != NULL);
    delete []m_pbNVR;
  }
}

uint8_t CRTC::BinaryToBCD (uint8_t bData)
{
  //++
  // Convert a binary value, 0..59, into two BCD digits...
  //--
  uint8_t d1 = bData / 10;
  uint8_t d2 = bData % 10;
  return MKBYTE(d1, d2);
}

uint8_t CRTC::BCDtoBinary (uint8_t bData)
{
  //++
  // Convert a BCD value to pure binary ...
  //--
  return HINIBBLE(bData)*10 + LONIBBLE(bData);
}

void CRTC::GetNow (uint8_t &bSeconds, uint8_t &bMinutes, uint8_t &bHours,
                   uint8_t &bDay, uint8_t &bMonth, uint8_t &bYear,
                   uint8_t &bWeekday, bool *pfIsPM, bool fUseBCD, bool fUse24HR)
{
  //++
  //   Get and return the current, real (not simulated!) time and date from
  // the operating system.  The result can be returned in either pure binary
  // or BCD, and in 12 or 24 hour format.
  //--
#if defined(_WIN32)
  __time64_t llTime;  struct tm tmNow;
  _time64(&llTime);  _localtime64_s(&tmNow, &llTime);
#elif defined(__linux__)
  time_t lTime;  struct tm tmNow;
  time(&lTime);  localtime_r(&lTime, &tmNow);
#endif

  // These are all easy ...
  bSeconds = fUseBCD ? BinaryToBCD(tmNow.tm_sec) : tmNow.tm_sec;
  bMinutes = fUseBCD ? BinaryToBCD(tmNow.tm_min) : tmNow.tm_min;
  bDay = fUseBCD ? BinaryToBCD(tmNow.tm_mday) : tmNow.tm_mday;
  bMonth = fUseBCD ? BinaryToBCD(tmNow.tm_mon + 1) : tmNow.tm_mon + 1;
  bYear = fUseBCD ? BinaryToBCD(tmNow.tm_year % 100) : tmNow.tm_year % 100;
  bWeekday = tmNow.tm_wday + 1;

  // The hours take a little more work to account for AM/PM mode ...
  if (pfIsPM != NULL) *pfIsPM = (tmNow.tm_hour >= 12);
  if (fUse24HR) {
    bHours = fUseBCD ? BinaryToBCD(tmNow.tm_hour) : tmNow.tm_hour;
  } else {
    // Careful!  In 12 hour mode, midnight and noon are both 12!!
    if ((tmNow.tm_hour==0) || (tmNow.tm_hour==12))
      bHours = fUseBCD ? BinaryToBCD(12) : 12;
    else if (tmNow.tm_hour < 12)
      bHours = fUseBCD ? BinaryToBCD(tmNow.tm_hour) : tmNow.tm_hour;
    else
      bHours = fUseBCD ? BinaryToBCD(tmNow.tm_hour-12) : tmNow.tm_hour-12;
  }
}

void CRTC::ClearNVR (uint16_t nFirst, uint16_t nLast)
{
  //++
  // Clear the non-volatile RAM contents ...
  //--
  assert((m_cbNVR > 0)  &&  (m_pbNVR != NULL));
  assert((nLast < m_cbNVR)  &&  (nFirst <= nLast));
  memset(m_pbNVR+nFirst, 0, nLast-nFirst+1);
}

int CRTC::FileError (string sFileName, const char *pszMsg, int nError)
{
  //++
  // Print a file related error message and then always return 0.
  //--
  char sz[80];
  if (nError > 0) {
    LOGS(ERROR, "error (" << nError << ") " << pszMsg << " " << sFileName);
    strerror_s(sz, sizeof(sz), nError);
    LOGS(ERROR, sz);
  } else {
    LOGS(ERROR, pszMsg << " - " << sFileName);
  }
  return 0;
}

size_t CRTC::SaveNVR (string sFileName) const
{
  //++
  //   Save the NVR contents to a binary file.  This pretty simple, and there
  // are no options.  The entire NVR is saved, in binary, to a file.  This
  // method returns the number of bytes written (always the size of the NVR),
  // or zero if any error occurs.
  //--
  assert((m_cbNVR > 0)  &&  (m_pbNVR != NULL));
  FILE *pFile;  int err = fopen_s(&pFile, sFileName.c_str(), "wb+");
  if (err != 0) return FileError(sFileName, "opening", err);

  // Now write the file ...
  size_t cbFile = fwrite(m_pbNVR, 1, m_cbNVR, pFile);
  if (cbFile != m_cbNVR) return FileError(sFileName, "writing", errno);

  // Close the file and we're done ...
  fclose(pFile);
  return cbFile;
}

size_t CRTC::LoadNVR (string sFileName)
{
  //++
  //   This method will load a disk file into the NVR.  There are no options -
  // the entire file, which must be exactly the size of the NVR, is loaded.
  // The number of bytes read is returned (which will always be the same as
  // the NVR size), or zero if any error occurs.
  //--
  assert((m_cbNVR > 0)  &&  (m_pbNVR != NULL));
  FILE *pFile;  int err = fopen_s(&pFile, sFileName.c_str(), "rb");
  if (err != 0) return FileError(sFileName, "opening", err);

  // Now read the file ...
  size_t cbFile = fread(m_pbNVR, 1, m_cbNVR, pFile);
  if (cbFile != m_cbNVR) return FileError(sFileName, "reading", errno);

  // Close the file and we're done ...
  fclose(pFile);
  return cbFile;
}

string CRTC::FormatWeekday (uint8_t bDay)
{
  //++
  // Return the name of the weekday corresponding to bDay ...
  //--
  static const string asDays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  return ((bDay > 0) && (bDay <= 7)) ? asDays[bDay-1] : "none";
}


string CRTC::FormatMonth (uint8_t bMonth)
{
  //++
  // Return the name of the month corresponding to  bMonth ...
  //--
  static const string asMonths[12] = {
    "January", "February", "March",     "April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"};
  return ((bMonth > 0) && (bMonth <= 12)) ? asMonths[bMonth-1] : "none";
}

string CRTC::FormatTime (uint8_t bSeconds, uint8_t bMinutes, uint8_t bHours, bool fBCD, bool f24Hr)
{
  //++
  // Format a time into a pretty ASCII string ...
  //--
  bool fPM = ISSET(bHours, 0x80);  CLRBIT(bHours, 0x80);
  if (fBCD) {
    bSeconds = BCDtoBinary(bSeconds);
    bMinutes = BCDtoBinary(bMinutes);
    bHours   = BCDtoBinary(bHours);    
  }
  if (f24Hr)
    return FormatString("%02d:%02d:%02d", bHours, bMinutes, bSeconds);
  else
    return FormatString("%02d:%02d:%02d %s", bHours, bMinutes, bSeconds, fPM ? "PM" : "AM");
}

string CRTC::FormatDate (uint8_t bDay, uint8_t bMonth, uint8_t bYear, bool fBCD)
{
  //++
  // Format a date into a pretty ASCII string ...
  //--
  if (fBCD) {
    bDay     = BCDtoBinary(bDay);
    bMonth   = BCDtoBinary(bMonth);
    bYear    = BCDtoBinary(bYear);
  }
  return FormatString("%02d/%02d/%d", bMonth, bDay, bYear);
}

void CRTC::DumpNVR (ostringstream &ofs) const
{
  //++
  // Dump NVR contents in hex or octal for debugging ...
  //--
  ofs << "--------------------------- NON-VOLATILE RAM ---------------------------";
  for (uint16_t nStart = 0; nStart < m_cbNVR; nStart += 16) {
    ofs << std::endl << FormatString("%03X/ ", nStart);
    for (uint16_t i = 0; i < 16; ++i)
      ofs << FormatString("%02X ", m_pbNVR[nStart+i]);
    ofs << "\t";
    for (uint16_t i = 0; i < 16; ++i) {
      uint8_t b = m_pbNVR[nStart+i] & 0x7F;
      ofs << FormatString("%c", isprint(b) ? b : '.');
    }
  }
}
