//++
// DS12887.hpp -> Non-volatile RAM and RTC emulation
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
//   This class emulates the Dallas DS1287, DS12887 and DS12887A non-volatile
// RAM and real time (time of day) clock chip.  This is the chip that's used
// on the ELF2K Disk/UART/RTC card, and it's also the same chip that was used
// in the IBM PC..
//
// REVISION HISTORY:
// 22-JAN-20  RLA   New file.
// 21-JUN-22  RLA   Rewrite to use the new generic RTC class
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Device.hpp"           // generic device definitions
#include "RTC.hpp"              // generic real time clock defintions
using std::string;              // ...


class C12887 : public CDevice, public CRTC {
  //++
  // DS12887 real time clock emulation ...
  //--
 
  // Registers and bits ...
public:
  enum {
    // Reserved RAM locations ...
    REG_SECONDS       = 0x00, // current time of day - seconds
    REG_SECONDS_ALARM = 0x01, // alarm time - seconds
    REG_MINUTES       = 0x02, // current time of day - minutes
    REG_MINUTES_ALARM = 0x03, // alarm time - minutes
    REG_HOURS         = 0x04, // current time of day - hours
    REG_HOURS_ALARM   = 0x05, // alarm time - hours
    REG_WEEKDAY       = 0x06, // current day of the week (Sunday=1!)
    REG_DAY           = 0x07, // current day of the month (1..31)
    REG_MONTH         = 0x08, // current month (1..12)
    REG_YEAR          = 0x09, // current year
    REG_A             = 0x0A, // control register A
    REG_B             = 0x0B, // ... B
    REG_C             = 0x0C, // ... C
    REG_D             = 0x0D, // ... and D
    NVRSIZE           = 128,  // total number of bytes in the NVR
    // Register A bits ..
    REGA_UIP          = 0x80, // update in progress
    REGA_DV           = 0x70, // oscillator control bits
    REGA_DV1          = 0x20, //   ... clock running
    REGA_RATE         = 0x0F, // square wave rate select
    // Register B bits ...
    REGB_SET          = 0x80, // clock setting mode
    REGB_PIE          = 0x40, // periodic interrupt enable
    REGB_AIE          = 0x20, // alarm      "    "    "
    REGB_UIE          = 0x10, // update     "    "    "
    REGB_SQWE         = 0x08, // square wave enable
    REGB_BINARY       = 0x04, // select binary data mode
    REGB_24HR         = 0x02, // select 24 hour mode
    REGB_DSE          = 0x01, // daylight savings enable
    // Register C bits ...
    REGC_IRQF         = 0x80, // interrupt request flag
    REGC_PF           = 0x40, // periodic interrupt flag
    REGC_AF           = 0x20, // alarm flag
    REGC_UF           = 0x10, // update flag
    // Register D bits ...
    REGD_VRT          = 0x80, // battery OK (valid RAM and time)
    // RTC events ...
    EVENT_PF          = 1,    // square wave (periodic flag) events
    // The base year used by ElfOS when storing the current date ...
    ELFOS_YEAR        = 1972  // everything is relative to 1972 for Mike!
  };

  // Constructor and destructor ...
public:
  C12887 (const char *pszName, uint16_t nBase, CEventQueue *pEvents, bool fElfOS=true);
  virtual ~C12887() {};
private:
  // Disallow copy and assignments!
  C12887 (const C12887&) = delete;
  C12887& operator= (C12887 const &) = delete;

  // Public properties ...
public:
  // Return the contents of the control registers ...
  uint8_t GetRegA() const {return m_bREGA;}
  uint8_t GetRegB() const {return m_bREGB;}
  uint8_t GetRegC() const {return m_bREGC;}

  // Device methods from CDevice ...
public:
  virtual RTC_TYPE GetType() const override {return RTC_DS12887;}
  virtual void ClearDevice() override;
  virtual void EventCallback (intptr_t lParam) override;
  virtual uint8_t DevRead (uint16_t nRegister) override;
  virtual void DevWrite (uint16_t nRegister, uint8_t bData) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Other public DS12887A methods ...
public:

  // Private methods ...
protected:
  // TRUE if binary mode is selected ...
  bool IsBinary() const {return ISSET(m_bREGB, REGB_BINARY);}
  // TRUE if 24 hour mode is selected
  bool Is24Hour() const {return ISSET(m_bREGB, REGB_24HR);}
  // Handle reading register A (and updating the time) ...
  uint8_t ReadRegA();
  // Handle writing register A (and generaing square waves) ...
  void WriteRegA (uint8_t bData);
  // Handle read and write register B ..
  uint8_t ReadRegB();
  void WriteRegB (uint8_t bData);
  // Handle square wave events ...
  void PeriodicEvent();
  // Update the current date and time registers ...
  void UpdateTime();

  // Private member data...
protected:
  // Simulated DS12887 registers ...
  uint8_t   m_bREGA;          // control/status register A
  uint8_t   m_bREGB;          //    "       "     "   "  B
  uint8_t   m_bREGC;          //    "       "     "   "  C
  bool      m_fElfOS;         // mangle the year according to ElfOS rules
  uint64_t  m_llPFdelay;      // square wave (periodic flag) delay
  // Table of square wave frequencies ...
  static const uint64_t g_llSQWfrequencies[16];
};
