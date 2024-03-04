//++
// CDP1879.hpp -> COSMAC Real Time Clock definitions
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
//   This class emulates the RCA CDP1879 real time clock chip.
//
// REVISION HISTORY:
// 18-JUN-22  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Device.hpp"           // generic device definitions
#include "RTC.hpp"              // generic real time clock emulation
using std::string;              // ...


class CCDP1879 : public CDevice, public CRTC
{
  //++
  // CDP1879 real time clock emulation ...
  //--

public:
  enum {
    // Reserved RAM locations ...
    RTCSIZE           = 8,    // number of RTC registers
    // Register offsets for the real time clock ...
    //   In the SBC1802 the CDP1879 is memory mapped and these offsets are all
    // relative to RTCBASE.  Note that there are no offsets zero or one - that's
    // not a mistake. The RCA guys did that on purpose just in case you want to
    // I/O map your RTC.  Also notice that the CDP1879 doesn't keep track of the
    // year - you're on your own for that!
    RTCSEC            = 2,    // current seconds ...
    RTCMIN            = 3,    // ... minutes
    RTCHRS            = 4,    // ... hours
    RTCDAY            = 5,    // ... day of month
    RTCMON            = 6,    // ... month
    RTCCSR            = 7,    // control and status register
    // Magic bits in the counter registers ...
    RTCPMF            = 0x80, // bit 7 of hours is AM/PM flag (1 = PM)
    RTC12H            = 0x40, // bit 6 of hours is 12/24 hour mode (1 = 12hr)
    RTCLYF            = 0x80, // bit 7 of month is leap year flag (1 = YES)
    // Control register bit assignments for the CDP1879 ...
    RTCOSC            = 0xC0, // oscillator frequency selection
    RTCSTRT           = 0x04, // start the clock running
    RTCWALM           = 0x08, // write/update alarm registers
    RTCSQWF           = 0x0F, // square wave output frequency
    // RTC status register bits(there are only two!) ...
    RTCAIRQ           = 0x80, // alarm generated interrupt request
    RTCCIRQ           = 0x40, // square wave interrupt
    // CDP1879 event callback actions ...
    EVENT_UNFREEZE    = 1,    // timeout and clear the "freeze flag"
    EVENT_TOGGLE      = 2,    // toggle the "clock out" virtual pin
  };

  // Constructor and destructor ...
public:
  CCDP1879 (address_t nBase, CEventQueue *pEvents);
  virtual ~CCDP1879() {};
private:
  // Disallow copy and assignments!
  CCDP1879 (const CCDP1879 &) = delete;
  CCDP1879& operator= (CCDP1879 const &) = delete;

  // Public properties ...
public:

  // CDP1879 device methods from CDevice ...
public:
  virtual RTC_TYPE GetType() const override {return RTC_CDP1879;}
  virtual void ClearDevice() override;
  virtual void EventCallback (intptr_t lParam) override;
  virtual word_t DevRead (address_t nRegister) override;
  virtual void DevWrite (address_t nRegister, word_t bData) override;
  virtual uint1_t GetSense (address_t nSense, uint1_t bDefault=0) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Other public CDP1879 methods ...
public:

  // Private methods ...
protected:
  void FreezeTime();
  void UnFreezeTime();
  void WriteControl (uint8_t bData);
  // Handle clock outpiut events ...
  void ToggleOutput();

  // Private member data...
protected:
  uint8_t   m_bStatus;        // contents of the status register
  uint8_t   m_bControl;       // last value written to the control register
  bool      m_fFreeze;        // infamous "freeze flag" (see comments!)
  bool      m_fClockOut;      // virtual square wave output pin
  bool      m_f12hrMode;      // TRUE if 12hr mode is selected
  bool      m_fLeapYear;      // TRUE if this is a leap year
  uint64_t  m_llClockDelay;   // clock output (square wave) delay
  // Current time and date registers ...
  uint8_t   m_bSeconds;       // current seconds (0..59)
  uint8_t   m_bMinutes;       // minutes (0..59)
  uint8_t   m_bHours;         // hours (1..12 or 0..23)
  uint8_t   m_bDay;           // day of the month (1..31)
  uint8_t   m_bMonth;         // month (1..12)
  // Table of square wave frequencies ...
  static const uint64_t g_llClockPeriod[16];
};
