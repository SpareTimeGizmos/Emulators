//++
// RTC.hpp -> generic non-volatile RAM and real time clock class
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
//   CRTC objects are used to implement specific real time clock chips, such
// as the DS12887 and the CDP1879.  Note that CRTC objects are NOT a CDevice
// implementation and they're intended to be encapsulated in (i.e. a "has a"
// relationship) the emulation of an actual RTC/NVR chip.
//
// REVISION HISTORY:
// 20-JUL-22  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
using std::string;              // ...


class CRTC
{
  //++
  // Generic real time clock and non-volatile RAM ...
  //--

  // Specific RTC types ...
public:
  //   These are returned by GetRTCtype() for code that needs to identify
  // the exact type of a CRTC derived object.  It's a cheapo RTTI ...
  enum _RTC_TYPES {
    RTC_UNKNOWN   = 0,        // undefined
    RTC_DS12887   = 1287,     // Dallas DS1287/DS12887/DS12887A
    RTC_CDP1879   = 1879,     // RCA CDP1879
  };
  typedef enum _RTC_TYPES RTC_TYPE;

  // Constructor and destructor ...
public:
  CRTC (uint16_t cbNVR=0);
  virtual ~CRTC();
private:
  // Disallow copy and assignments!
  CRTC (const CRTC &) = delete;
  CRTC& operator= (CRTC const &) = delete;

  // Public properties ...
public:
  // Return the specific UART subtype ...
  virtual RTC_TYPE GetType() const {return RTC_UNKNOWN;}
  // Return the size of the NVR (may be zero!)...
  uint16_t NVRsize() const {return m_cbNVR;}

  // RTC specific public methods ...
public:
  // Get the current time from the operating system ...
  static void GetNow (uint8_t &bSeconds, uint8_t &bMinutes, uint8_t &bHours,
                      uint8_t &bDay, uint8_t &bMonth, uint8_t &bYear, uint8_t &bWeekday,
                      bool *pfIsPM=NULL, bool fUseBCD=true, bool fUse24HR=true);

  // NVR specific public methods ...
public:
  // Save NVR to a file or load NVR from a file ...
  size_t SaveNVR (string sFileName) const;
  size_t LoadNVR (string sFileName);
  // Clear the memory portion of the NVR ...
  void ClearNVR (uint16_t nFirst, uint16_t nLast);
  void ClearNVR (uint16_t nFirst=0) {ClearNVR(nFirst, m_cbNVR-1);}
  // Read or write NVR bytes ...
  uint8_t ReadNVR (uint16_t a) const {assert(a<m_cbNVR);  return m_pbNVR[a];}
  void WriteNVR (uint8_t a, uint8_t d) {assert(a<m_cbNVR);  m_pbNVR[a] = d;}

  // Other public methods ...
public:
  // Convert a binary value to BCD ...
  static uint8_t BinaryToBCD(uint8_t bData);
  // Convert a BCD value back to binary ...
  static uint8_t BCDtoBinary(uint8_t bData);
  // Format dates and times for printing ...
  static string FormatWeekday (uint8_t bDay);
  static string FormatMonth (uint8_t bMonth);
  static string FormatTime (uint8_t bSeconds, uint8_t bMinutes, uint8_t bHours, bool fBCD, bool f24Hr);
  static string FormatDate (uint8_t bDay, uint8_t bMonth, uint8_t bYear, bool fBCD);
  // Dump the NVR for debugging ...
  void DumpNVR (ostringstream &ofs) const;

  // Private methods ...
private:
  // Log file read or write errors ...
  static int FileError (string sFileName, const char *pszMsg, int nError);

  // Private member data...
protected:
  uint16_t  m_cbNVR;          // size of NVR in bytes
  uint8_t  *m_pbNVR;          // non-volatile RAM storage
};
