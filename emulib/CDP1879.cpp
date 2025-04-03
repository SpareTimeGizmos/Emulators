//++
// CDP1879.cpp -> COSMAC Real Time Clock emulation
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
//   This module emulates the CDP1879 real time clock chip.  Unlike the DS12887
// used in the Elf2K, the CDP1879 has no general purpose RAM and implements only
// seven registers, all of which keep track of the current date and time.  Reading
// the emulated clock always returns the actual current time and date as obtained
// from the operating system.  It's possible to set the clock in so far as you
// can write to the time registers, however the actual time does not change and
// the next read of the clock will continue to return the current time from the
// host OS.
// 
//   The CDP1879 also implements a square wave generator which can toggle an
// output pin at any one of 15 programmable frequencies.  In addition the rising
// edge of this square wave output can generate a programmed interrupt if enabled.
// This code does not emulate the output pin of course, but the programmable
// interrupt function at any of 15 different intervals IS implemented.
// 
//   The CDP1879 implements an alarm clock function which can generate an interrupt
// at any programmed time in the future.  This function IS NOT implemented here.
// You can write to the alarm registers in the emulated chip, but nothing will
// ever happen.
//
//   Other interesting notes about the CDP1879 -
//
//      * Only BCD mode exists.  There is no binary mode.
//      * It does NOT keep the year - only the day and month.
//      * The host must set or clear the MSB of the month to tell the
//         CDP1879 whether February should have 28 or 29 days.
//      * Likewise, it does NOT track the day of the week.
//
//   Lastly, the CDP1879 contains what RCA calls a "freeze circuit" which is
// intended to prevent the date and time registers from being clocked while
// they are being read or written by the software.  The RCA manuals are a
// little ambiguous about how this works, but my interpretation is that the
// freeze is enabled by ANY access to ANY register EXCEPT the control or
// status registers.  The freeze holds the counters static until either 250
// milliseconds elapse, OR the software explicitly resets the freeze by any
// write to address 1.  Note that address 1 is not otherwise used!
// 
//   To emulate this, we keep a "freeze" flag which is set by any read of any
// of the time registers (remember that we ignore writing the time anyway)
// and is reset by either a timeout event or a write to address 1.  Setting
// the freeze flag causes the emulated time to be updated from the OS, and
// that then stays static until the freeze flag is cleared and set again.
//    
// REVISION HISTORY:
// 18-JUN-22  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "ConsoleWindow.hpp"    // console window functions
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Device.hpp"           // generic device definitions
#include "CPU.hpp"              // CPU definitions
#include "RTC.hpp"              // generic real time clock emulation
#include "CDP1879.hpp"          // declarations for this module

//   This table gives the period, in NANOSECONDS (!!) of the clock/square
// wave output as controlled by the lower 4 bits of the control register.
// We don't care about the output pin of course, but this also generates
// an interrupt request that we DO care about emulating!
#define _HZ2NS(x)         (1000000000ULL/(x))
#define _SEC2NS(x)        (1000000000ULL*(x))
const uint64_t CCDP1879::g_llClockPeriod[16] = {
               0ULL,  //  0 - disabled
    _HZ2NS(2048ULL),  //  1 - 2048Hz (  488.2us)
    _HZ2NS(1024ULL),  //  2 - 1024Hz (  976.5us)
    _HZ2NS( 512ULL),  //  3 -  512Hz ( 1953.1us)
    _HZ2NS( 256ULL),  //  4 -  256Hz ( 3906.2us)
    _HZ2NS( 128ULL),  //  5 -  128Hz ( 7812.5us)
    _HZ2NS(  64ULL),  //  6 -   64Hz ( 15.625ms)
    _HZ2NS(  32ULL),  //  7 -   32Hz ( 31.250ms)
    _HZ2NS(  16ULL),  //  8 -   16Hz ( 62.500ms)
    _HZ2NS(   8ULL),  //  9 -    8Hz (125.000ms)
    _HZ2NS(   4ULL),  // 10 -    4Hz (250.000ms)
    _HZ2NS(   2ULL),  // 11 -    2Hz (500.000ms)
  _SEC2NS(    1ULL),  // 12 - once per second
  _SEC2NS(   60ULL),  // 13 - once per minute
  _SEC2NS( 3600ULL),  // 14 - once per hour
  _SEC2NS(86400ULL),  // 15 - once per day
 };

// This is the "unfreeze" timeout delay, in nanoseconds ...
#define UNFREEZE_TIMEOUT  (250000000ULL)  // 250ms ....

CCDP1879::CCDP1879 (address_t nBase, CEventQueue *pEvents)
  : CDevice("RTC", "CDP1879", "Real Time Clock/Calendar", INOUT, nBase, RTCSIZE, pEvents), CRTC()
{
  //++
  //--
  m_bSeconds = m_bMinutes = m_bHours = 0;
  m_bDay = m_bMonth = 0;  m_fFreeze = false;
  m_fClockOut = m_f12hrMode = m_fLeapYear = false;
  m_bStatus = m_bControl = 0;  m_llClockDelay = 0;
  m_fEnableRTC = true;
}

void CCDP1879::FreezeTime()
{
  //++
  //   This routine implements the "freeze" function of the CDP1879.  It's called
  // before any access to any time register (the control and status registers
  // don't count!) and will examine the Freeze flag.  If we're already frozen then
  // nothing happens, but if the Freeze flag is cleared then it will update the
  // time registers from the OS and set the freeze flag.  
  //--
  uint8_t bDummy;  bool fIsPM;
  if (m_fFreeze) return;
  GetNow(m_bSeconds, m_bMinutes, m_bHours, m_bDay, m_bMonth,
         bDummy, bDummy, &fIsPM, true, !m_f12hrMode);
  //   Note that the CDP1879 continues to toggle the AM/PM bit even when 24 hour
  // mode is selected.  Also, be sure to allow the 12/24 hour mode select bit to
  // be read back - it's bit 6 of the hours!
  if (fIsPM) m_bHours |= RTCPMF;
  if (m_f12hrMode) m_bHours |= RTC12H;
  //   The leap year flag is the MSB of the month register.  We don't set this bit -
  // the CDP1879 doesn't even know the year! - the host has to tell us.  We don't
  // use this bit, but be sure to allow it to be read back...
  if (m_fLeapYear) m_bMonth |= RTCLYF;
  // Freeze the time and schedule a timeout to unfreeze it ...
  CDevice::ScheduleEvent(EVENT_UNFREEZE, UNFREEZE_TIMEOUT);
  LOGF(TRACE, "CDP1879 time frozen");
  m_fFreeze = true;
}

void CCDP1879::UnFreezeTime()
{
  //++
  //   This routine will "unfreeze" the time.  It's called by either the freeze
  // timeout that FreezeTime() schedule, or by a write to imaginary register #1.
  // Register 1 doesn't actually exist, but the real CDP1879 allows a programmed
  // unfreeze by writing any value to this address.
  //--
  CDevice::CancelEvent(EVENT_UNFREEZE);
  LOGF(TRACE, "CDP1879 time unfrozen");
  m_fFreeze = false;
}

void CCDP1879::WriteControl (uint8_t bControl)
{
  //++
  //   This routine handles a write to the CDP1879 control register.  The main
  // thing this does, at least for this emulation, is to enable or disable the
  // periodic clock output divider.  The upper four bits of the control register
  // select the clock output frequency, and we use this to schedule a repeating
  // event at the correct interval.  
  //
  //   We ignore the oscillator frequency select (bits 0 and 1) in the control
  // register.  As long as the firmware sets these to match the actuall crystal
  // or oscillator frequency, these don't affect the time keeping nor the clock
  // output.  Bit 3 in the control register allows the alarm registers to be
  // set, however we don't implement the alarm nor do we implementing setting the
  // time, so we can pretty much ignore that one too.
  // 
  //   As I inderstand the datasheet, bit 2 will disable the entire counter chain
  // when it is cleared.  I suppose we could actually implement that by cancelling
  // the periodic clock event and disabling updating the time from the host OS, but
  // we presently don't bother with any of that.
  // 
  //   Lastly, writing any value to the control register has the important side
  // effect of clearing the status register and cancelling any pending interrupt
  // requests.
  //--
  LOGF(TRACE, "CDP1879 write control 0x%02X", bControl);

  //   Update the clock output periodic event but be careful - if the clock output
  // is currently active AND the new control register value is the same as the last
  // one, then don't disturb anything.
  if ((bControl & 0xF0) != (m_bControl & 0xF0)) {
    // Cancel any pending event ...
    CDevice::CancelEvent(EVENT_TOGGLE);
    //   Figure out the new interval.  Note that the "/2" is present because the
    // table gives the output period, so the output has to toggle at twice that
    // frequency.  Also, remember that zero means the clock output is disabled.
    uint8_t bSQW = (bControl >> 4) & 0x0F;
    m_llClockDelay = g_llClockPeriod[bSQW] / 2ULL;
    if (m_llClockDelay > 0) {
      LOGF(TRACE, "CDP1879 clock out interval %lldns", m_llClockDelay);
      CDevice::ScheduleEvent(EVENT_TOGGLE, m_llClockDelay);
    }
  }

  // Clear the status register and remove any interrupt request ...
  m_bStatus = 0;  CDevice::RequestInterrupt(false);

  // Remember the last control byte written and we're done ...
  m_bControl = bControl;
}

void CCDP1879::ToggleOutput()
{
  //++
  //   Toggle the clock output "pin" and on every falling edge set the clock
  // interrupt bit in the status register.  If interrupts are enabled, this
  // requests an interrupt.  Note that interrupts are requested only on the
  // falling edge of the clock output, so in effect this only interrupts
  // every other event!
  //--
  if (!m_fEnableRTC) return;
  if (!(m_fClockOut = !m_fClockOut)) {
    m_bStatus |= RTCCIRQ;  CDevice::RequestInterrupt(true);
    LOGF(TRACE, "CDP1879 clock out interrupt!");
  }
  if (m_llClockDelay > 0) CDevice::ScheduleEvent(EVENT_TOGGLE, m_llClockDelay);
}

void CCDP1879::ClearDevice()
{
  //++
  // This emulates the RESET input to the CDP1879 ...
  //--
  m_bStatus = m_bControl = 0;
  m_fFreeze = m_fClockOut = false;
  m_llClockDelay = 0;
  CDevice::RequestInterrupt(false);
}

void CCDP1879::EventCallback (intptr_t lParam)
{
  //++
  // Handle event callbacks for this device.
  //--
  switch (lParam) {
    case EVENT_TOGGLE:   ToggleOutput();  break;
    case EVENT_UNFREEZE: UnFreezeTime();  break;
    default: assert(false);
  }
}

word_t CCDP1879::DevRead (address_t nRegister)
{
  //++
  //   This routine implements any read operation for a CDP1879 register.  For
  // all the time registers, this will freeze the time (which in this implementation
  // also obtains the current date and time from the host OS as a side effect!)
  // and then return the contents of the selected counter.  Reading the status
  // register is a special case, and does NOT freeze nor update the current time.
  // 
  //   Registers 0 and 1 are not implemented and the CDP1879 is not actually even
  // selected when those addresses are used.  In the SBC1802 this just lets the
  // bus float, and 0xFF will be read.  The alarm registers are read only, and the
  // datasheet isn't explicit about what happens if you try to read them time with
  // the RTCWALM bit set.  I assume it reads the current time (just as if RTCWALM
  // wasn't set).
  //
  //   Lastly, as far as I can determine there are no side effects to reading the
  // status register.  In particular, reading the status does NOT clear any of the
  // interrupt request bits nor does it deassert the interrupt request output.
  // According to the datasheet, the way to clear an interrrupt request is to write
  // to the control register instead.
  //--
  assert((nRegister >= GetBasePort())  &&  ((nRegister-GetBasePort()) < RTCSIZE));
  if (!m_fEnableRTC) return 0xFF;
  switch (nRegister - GetBasePort()) {
    case 0:                       return 0xFF;
    case 1:                       return 0xFF;
    case RTCSEC:  FreezeTime();   return m_bSeconds;
    case RTCMIN:  FreezeTime();   return m_bMinutes;
    case RTCHRS:  FreezeTime();   return m_bHours;
    case RTCDAY:  FreezeTime();   return m_bDay;
    case RTCMON:  FreezeTime();   return m_bMonth;
    case RTCCSR:                  return m_bStatus;
    default: assert(false);       return 0xFF;
  }
}

void CCDP1879::DevWrite (address_t nRegister, word_t bData)
{
  //++
  //   This method handles all write operations to CDP1879 registers.  Remember
  // that we don't actually allow the clock to be set (we always return the
  // current time from the host OS instead), so most of these operations do
  // nothing.  Also, we don't implement the alarm nor the alarm registers, so
  // there's no need to worry about the RTCWALM bit here, either.
  // 
  //   There are a couple of important things, however.  Writing any value to
  // register 1 will "unfreeze" the time - this is an intentional side effect
  // that's documented in the datahseet.  Also, writing any of the time registers
  // will freeze the time.  It's not clear that this matters since we're not
  // actually writing the time anyway, but for the sake of accuracy we'll do it
  // anyway.  And when writing to the hours or month registers we need to extract
  // and save the 12/24 hour flag and the leap year flag.  And lastly, writing
  // the control register is of course not a NOP and does do something useful.
  //--
  assert((nRegister >= GetBasePort())  &&  ((nRegister-GetBasePort()) < RTCSIZE));
  if (!m_fEnableRTC) return;
  switch (nRegister - GetBasePort()) {
    case 0:                                                           break;
    case 1:       UnFreezeTime();                                     break;
    case RTCSEC:  FreezeTime();                                       break;
    case RTCMIN:  FreezeTime();                                       break;
    case RTCHRS:  FreezeTime();  m_f12hrMode = ISSET(bData, RTC12H);  break;
    case RTCDAY:  FreezeTime();                                       break;
    case RTCMON:  FreezeTime();  m_fLeapYear = ISSET(bData, RTCLYF);  break;
    case RTCCSR:  WriteControl(bData);                                break;
    default: assert(false);
  }
}

uint1_t CCDP1879::GetSense (address_t nSense, uint1_t bDefault)
{
  //++
  //   On the SBC1802 the CDP1879 interrupt request output is wired to the
  // CPU's EF2 input.  The RTC is a pretty simple minded device and it will
  // request an interrupt any time either the alarm or clock bits are set
  // in the status register.
  //--
  if (!m_fEnableRTC) return 0;
  return ((m_bStatus & RTCCIRQ) != 0) ? 1 : 0;
}

void CCDP1879::ShowDevice(ostringstream &ofs) const
{
  //++
  // Dump the device state for the UI command "EXAMINE DISPLAY" ...
  //--
  if (!m_fEnableRTC) {
    ofs << FormatString("RTC DISABLED") << std::endl;
  } else {
    ofs << FormatString("Last time was %s %02d, %02d:%02d:%02d %s (%sleap year)\n",
      FormatMonth(BCDtoBinary(m_bMonth & 0x7F)).c_str(), BCDtoBinary(m_bDay),
      BCDtoBinary(m_bHours & 0x1F), BCDtoBinary(m_bMinutes), BCDtoBinary(m_bSeconds),
      ISSET(m_bHours, RTCPMF) ? "PM" : "AM",
      m_fLeapYear ? "" : "not a ");

    ofs << FormatString("Status=0x%02X, Control=0x%02X, Freeze=%d, LeapYear=%d\n",
      m_bStatus, m_bControl, m_fFreeze, m_fLeapYear);

    if (m_llClockDelay > _SEC2NS(1))
      ofs << FormatString("Square wave enabled, interval %llds\n", NSTOMS(m_llClockDelay) / 1000ULL);
    else if (m_llClockDelay >= _HZ2NS(8))
      ofs << FormatString("Square wave enabled, interval %lldms, (%ldHz)\n", NSTOMS(m_llClockDelay), NSTOHZ(m_llClockDelay));
    else if (m_llClockDelay > 0)
      ofs << FormatString("Square wave enabled, interval %lldus, (%ldHz)\n", NSTOUS(m_llClockDelay), NSTOHZ(m_llClockDelay));
    else
      ofs << "Square wave output disabled\n"; \
  }
}
