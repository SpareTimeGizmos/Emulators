//++
// DS12887.cpp -> Non-volatile RAM and real time clock emulation
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
//   This module emulates the DS1287, DS12887 and DS12887A non-volatile RAM and
// real time clock chips.  THese chips are all basically identical for our
// purposes (the DS1287 has only 64 bytes of RAM where as the others all have
// 128, but we don't bother with that).
//
//   * The non-volatile RAM is implemented and it can be saved to and loaded
//     from a file using the Save() and Load() methods.
//
//   * The time of day clock is implemented and returns the actual time of day
//     from the host OS.  The 12/24 hour, binary/BCD and daylight savings time
//     options work.
//
//   * The square wave generator is implemented and toggles the PF flag in
//     register C.  The square wave time is simulated, so that it appears to
//     have the correct frequency for the simulated program.
//
//   * Alarms are NOT implemented.  You can set and read back the alarm
//     registers, but nothing will ever happen.
//
//   * Interrupts are NOT implemented.  The interrupt enable flags are always
//     zero and cannot be changed.
//
//   * Setting the time of day clock is not implemented.  You can write to it,
//     but it will be overwritten with the correct time of day from the host OS
//     whever it is read.
//
//   * The DVx bits are not implemented.  The oscillator is always on and you
//     can't turn it off.
//
// REAL TIME OF DAY
//   Exactly how the RTC should be implemented is up for discussion.  Should it
// get the real time of day from the host and always return the actual wall
// clock time? Or should it keep track of simulated time and tick off simulated
// seconds without regard to the real time?  It's more useful, at least for me,
// to have it return the real, wall clock, time.  That way time stamps for the
// simulated operating system actually make sense in the real world.
//
//   So, how to implement this?  Thru a combination of hacks and kludges, of
// course.  When we start up we get the real time from the OS and store it in
// the NVR.  That time stays there, unchanging, until the simulated software
// reads register A.  This register contains the "update in progress" (UIP)
// bit, and the first time register A is read the UIP will be returned as 1.
// We then get the current real time from the OS, update the NVR, and clear
// the UIP bit.  Now the simulated OS can read the correct current time from
// the RTC.  The next time register A is read the UIP will be set again, and
// the whole process repeats.  Thus any simulated software that first checks
// the UIP to see if it's safe to read the time, and then reads the time, will
// always get the correct current time.  This won't work for all cases, but it
// works for most of them.
//    
// REVISION HISTORY:
// 22-JAN-20  RLA   New file.
//  3-FEB-20  RLA   Change name to C12887 ...
// 21-JUN-22  RLA   Rewrite to use the new generic RTC class
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <time.h>               // _localtime64_s(), localtime_r(), ...
#include <sys/timeb.h>          // struct __timeb, ftime(), etc ...
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "RTC.hpp"              // generic real time clock defintions
#include "DS12887.hpp"          // declarations for this module

// Table of square wave frequencies ...
const uint64_t C12887::g_llSQWfrequencies[16] = {
        0,  256ULL,  128ULL, 8192ULL, 4096ULL, 2048ULL, 1024ULL,  512ULL,
   256ULL,  128ULL,   64ULL,  32ULL,    16ULL,    8ULL,    4ULL,    2ULL
};


C12887::C12887 (const char *pszName, uint16_t nBase, CEventQueue *pEvents, bool fElfOS)
  : CDevice(pszName, "DS12887", "Real Time Clock/Calendar", INOUT, nBase, NVRSIZE, pEvents),
    CRTC(NVRSIZE)
{
  //++
  //--
  m_fElfOS = fElfOS;
  ClearDevice();
}

void C12887::ClearDevice()
{
  //++
  // Disable the square wave and return the clock to its default settings...
  //--
  m_bREGA = REGA_UIP|REGA_DV1;
  m_bREGB = REGB_BINARY|REGB_24HR|REGB_DSE;
  m_bREGC = 0;  m_llPFdelay = 0;
  UpdateTime();
}

void C12887::UpdateTime()
{
  //++
  //   Get the current, real (not simulated!) time of day from the operating
  // system and store it, in the proper format, in bytes 0..9 of the NVR.
  // Yes, we store the time in the simulated RAM as if it were any other data.
  // There's no reason not to, and it saves us additional storage.
  //--
  uint8_t bSeconds, bMinutes, bHours, bDay, bMonth, bYear, bWeekday;  bool fIsPM;
  GetNow(bSeconds, bMinutes, bHours, bDay, bMonth,
         bYear, bWeekday, &fIsPM, !IsBinary(), Is24Hour());

  // Fixup the AM/PM flag in the hours register, if necessary ...
  if (!Is24Hour()) {
    if (fIsPM) bHours |= 0x80;
  }

  // Careful with the year!!
  //   ElfOS and the ELF2K BIOS want the year to be relative to 1972.  In real
  // life the only way to get a date into the DS12887 is for ElfOS to put it
  // there, so it can use any base year it wants.  The advantage to this is
  // that there's no Y2K discontinuity at year 2000, but I'm not sure everybody
  // does it this way...
  if (m_fElfOS) {
    if (!IsBinary()) bYear = BCDtoBinary(bYear);
    bYear = (bYear + 2000 - ELFOS_YEAR) % 100;
    if (!IsBinary()) bYear = BinaryToBCD(bYear);
  }

  // Store the results in the NVR and we're done ...
  WriteNVR(REG_SECONDS, bSeconds);
  WriteNVR(REG_MINUTES, bMinutes);
  WriteNVR(REG_HOURS,   bHours);
  WriteNVR(REG_DAY,     bDay);
  WriteNVR(REG_MONTH,   bMonth);
  WriteNVR(REG_YEAR,    bYear);;
  WriteNVR(REG_WEEKDAY, bWeekday);
}

uint8_t C12887::ReadRegA()
{
  //++
  //   This method is called whenever register A, which contains the update
  // in progress (aka UIP) bit, is read.  The UIP bit effectively toggles with
  // every read - first it's a 1, then a 0, then a 1, etc.  And every time the
  // UIP bit is set, the NVR clock is updated with the real time of day from
  // the OS.  Most software for the DS12887 will first test the UIP to see if
  // it's safe to read the clock, and then read the clock.  This ensures that
  // it always gets the correct time of day every time.
  //--
  uint8_t bData = m_bREGA;
  if (ISSET(m_bREGA, REGA_UIP)) {
    UpdateTime();  CLRBIT(m_bREGA, REGA_UIP);
  } else {
    SETBIT(m_bREGA, REGA_UIP);
  }
  return bData;
}

void C12887::WriteRegA (uint8_t bData)
{
  //++
  //   Writing to register A activates the square wave timer.  The UIP bit is
  // read only anyway and, although the DV bits are writable, we don't allow
  // them to change.  We don't implement turning off the oscillator!  That
  // leaves the square wave as the only thing we have to worry about.
  //
  //   Unlike the clock, which uses the real wall clock time, the square wave
  // generator runs in simulated time.  We look at the frequency that the
  // square wave is set to and attempt to schedule an event for that time.
  // These events will set the PF bit and then reschedule, until the square
  // wave generator is turned off.
  //--
  m_bREGA = (m_bREGA & ~REGA_RATE) | (bData & REGA_RATE);
  unsigned nRate = m_bREGA & REGA_RATE;
  if (nRate == 0) {
    m_llPFdelay = 0;  CancelEvent(EVENT_PF);
  } else {
    //   Note that the table gives the desired square wave frequency, but the
    // event just toggles the PF bit.  That means we need to have the period
    // to get one complete square wave at the required frequency...
    m_llPFdelay = (1000000000ULL / g_llSQWfrequencies[nRate]) / 2ULL;
    ScheduleEvent(EVENT_PF, m_llPFdelay);
  }
}

uint8_t C12887::ReadRegB()
{
  //++
  // I lied - reading register B doesn't have any side effects!
  //--
  return m_bREGB;
}

void C12887::WriteRegB (uint8_t bData)
{
  //++
  //   Writing register B is pretty easy, except that we don't implement
  // interrupts and don't allow any of the interrupt enable bits to be set.
  // We do allow the SET bit to be set, but it does nothing.  The binary mode,
  // 24 hour mode, and daylight savings time modes all work and can be set or
  // cleared at will.  SQWE doesn't do anything, of course - there's no output
  // here!
  //--
  m_bREGB = bData & (REGB_SET|REGB_SQWE|REGB_BINARY|REGB_24HR|REGB_DSE);
}

void C12887::PeriodicEvent()
{
  //++
  // Toggle the PF bit as long as the square wave generator is turned on.
  //--
  m_bREGC ^= REGC_PF;
  ScheduleEvent(EVENT_PF, m_llPFdelay);
}

void C12887::EventCallback (intptr_t lParam)
{
  //++
  // Handle event callbacks for this device.
  //--
  switch (lParam) {
    case EVENT_PF:  PeriodicEvent();  break;
    default: assert(false);
  }
}

uint8_t C12887::DevRead (uint16_t nRegister)
{
  //++
  //   This handles the read NVR function.  Note that reading the time takes
  // no special effort, since UpdateTime() stores the time data in the m_bNVR
  // array.  We can just return what's there.  Alarms aren't implemented, but
  // reading the alarm registers will read back whatever was last written.
  //
  //   Reading registers A and B have potential side effects, and those are
  // handled by routines.  Register D always returns the "battery OK" bit,
  // which is the only valid bit in that register.  Lastly, register C always
  // returns zeros (because interrupts aren't implemented) EXCEPT for the PF
  // (periodic flag).  That's updated by PeriodicEvent() .. 
  //--
  assert(nRegister >= GetBasePort());
  nRegister -= GetBasePort();
  assert(nRegister < NVRSIZE);
  switch (nRegister) {
    case REG_A:   return ReadRegA();
    case REG_B:   return ReadRegB();
    case REG_C:   return m_bREGC;
    case REG_D:   return REGD_VRT;
    default:      return ReadNVR(nRegister);
  }
}

void C12887::DevWrite (uint16_t nRegister, uint8_t bData)
{
  //++
  //   And this method handles writes to NVR.  Writes are allowed to any of
  // the time registers, although I don't think that's strictly correct - you
  // are supposed to use the REGB_SET bit first.  Alarms are not implemented,
  // and you can write anything you want to those registers.
  //
  //   Registers C and D are officially read only, and writes to those are
  // ignored.  Writing registers A and B have side effects, and those two
  // have special routines to handle them.  That's about it!
  //--
  assert(nRegister >= GetBasePort());
  nRegister -= GetBasePort();
  assert(nRegister < NVRSIZE);
  switch (nRegister) {
    case REG_A:   WriteRegA(bData);                     break;
    case REG_B:   WriteRegB(bData);                     break;
    case REG_C:                                         break;
    case REG_D:                                         break;
    default:      WriteNVR((nRegister & 0x7F), bData);  break;
  }
}

void C12887::ShowDevice (ostringstream &ofs) const
{
  //++
  // Dump the device state for the UI command "EXAMINE DISPLAY" ...
  //--
  ofs << "Last time was " << FormatWeekday(ReadNVR(REG_WEEKDAY)) << " "
      << FormatTime(ReadNVR(REG_SECONDS), ReadNVR(REG_MINUTES), ReadNVR(REG_HOURS), !IsBinary(), Is24Hour()) << " "
      << FormatDate(ReadNVR(REG_DAY), ReadNVR(REG_MONTH), ReadNVR(REG_YEAR), !IsBinary()) << std::endl;
  ofs << FormatString("REGA=0x%02X, REGB=0x%02X, REGC=0x%02X, REGD=0x%02X\n", m_bREGA, m_bREGB, m_bREGC, ReadNVR(REG_D));
  if (m_llPFdelay > 0) ofs << FormatString("Square wave delay=%lldns, frequency=%lldHz\n", m_llPFdelay, NSTOHZ(m_llPFdelay));
  DumpNVR(ofs);
}
