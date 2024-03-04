//++
// RTC11.cpp -> SBCT11 DS12887 Real Time Clock implementation
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
// 
// LICENSE:
//    This file is part of the SBCT11 emulator project.  SBCT11 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBCT11 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBCT11.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   The SBCT11 has a single DS12887A real time clock and non-volatile RAM chip.
// This is the same chip that was used in the classic PC/AT to save the CMOS
// settings and keep track of the time.  The DS12887 looks something like a
// memory chip to the CPU, with a total of 128 bytes.  The first 10 bytes keep
// track of the time and date; the next four bytes are control and status
// registers, and the remaining 114 bytes are general purpose memory.  The time
// and status bytes are updated automatically by the DS12887 as time passes, and
// the general purpose RAM bytes can be used for whatever we want.  The SBCT11
// firmware uses some of the to store settings (e.g. baud rates, boot flags,
// etc) and the remainder are available to the user's programs.
//
//   The SBCT11 maps the DS12887 into the DCT11 I/O space as an address port
// and two data ports, one read/write and write only.  The software should
// write the desired RTC/NVR register address (0..127) to REG_ADDRESS, and
// then it can read or write the contents of that RTC register by accessing
// REG_READ or REG_WRITE locations.  Even though the DS12887 is effectively
// just a 128 byte SRAM chip, it's not mapped into the DCT11 address space
// as a block of memory locations.  Turns out it's too hard to do that and
// still meet the DS12887 timing requirements.  Besides, nobody needs fast
// access to the RTC/NVR anyway.
//
// PCB REVISIONS
//   Note that there was a slight error (don't ask!) in the revision B SBCT11
// PC boards, and the DS12887 is actually connected to DAL1-8.  This means that
// all addresses and data need to be left shifted by one bit. In the revision C
// PC boards this was fixed and the DS12887 maps properly the LSB of the DCT11
// data bus.  In the revision C boards you can use byte instructions (e.g.
// MOVB) to access the DS12887 address and data registers, but in revision B
// you cannot.
//
//   The RTC/NVR didn't work at all in the revision A PCBs.
// 
// REVISION HISTORY:
// 11-JUL-22  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
#include <sstream>              // C++ std::stringstream, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "Interrupt.hpp"        // CInterrupt definitions
#include "EventQueue.hpp"       // CEventQueue definitions
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "DS12887.hpp"          // Dallas DS12887 emulation
#include "RTC11.hpp"            // declarations for this module


CRTC11::CRTC11 (address_t nBase, CEventQueue *pEvents, bool fOldPCB)
  : CDevice("RTC", "12887", "Time of Day Clock", INOUT, nBase, REG_COUNT, pEvents)
{
  //++
  //   Create the DS12887 RTC object.  Note that the C12887 is actually
  // another CDevice derived object and it expects to be memory mapped.
  // That's not how the SBCT11 works, however, so we give the C12887 a
  // base address of zero and then we can just supply it with the DS12887
  // NVR address directly.
  // 
  //   BTW, note that the third parameter to the DS12887 constructor, false,
  // says not to use the ElfOS year hack...
  //--
  assert(pEvents != NULL);
  m_p12887 = DBGNEW C12887("RTC11", 0, pEvents, false);
  m_bAddress = 0;  m_fOldPCB = fOldPCB;  m_wCache = 0;
}

CRTC11::~CRTC11()
{
  //++
  // Delete the DS12887 object ...
  //--
  assert(m_p12887 != NULL);
  delete m_p12887;  m_p12887 = NULL;
}

void CRTC11::ClearDevice()
{
  //++
  //   Pass along any RESET inputs to the DS12887.  This stops the square wave
  // generator and disables interrupts, if any.
  //--
  m_p12887->ClearDevice();
}

uint8_t CRTC11::ReadByte (bool fOdd)
{
  //++
  //   Read the byte selected by m_bAddress from the RTC and return it.
  // This would be trivial except for the bug in the revision B PC boards -
  // since the data is shifted left one bit, the MSB of the byte we read
  // has to be returned in the high order (odd) byte.  This isn't a big
  // deal on the SBCT11 since it has a 16 bit bus, but we have an eight
  // bit bus here and the even and odd bytes are transferred in separate
  // read operations.  
  // 
  //   This simulation always reads the low byte first, so we handle that
  // normally but cache the result in m_wCache.  Then when the odd byte is
  // read, we just return it from the cache.  You might ask "why not just
  // read the NVR twice"?  Well, some of the registers have side effects when
  // read, and some of the registers can change over time.  Remember, this
  // chip is a clock, after all!
  //--
  if (m_fOldPCB) {
    if (!fOdd) {
      m_wCache = m_p12887->DevRead(m_bAddress) << 1;
      return LOBYTE(m_wCache);
    } else
      return HIBYTE (m_wCache);
  } else
    return m_p12887->DevRead(m_bAddress);
}

void CRTC11::WriteByte (uint8_t bData, bool fOdd)
{
  //++
  //   And this routine handles writing a byte to the NVR/RTC location
  // selected by m_bAddress.  The problem with the revision B PCBs here is
  // similar to ReadByte(), but in reverse.  The DCT11 will always write the
  // low (even addressed) byte first, so we have to cache that.  Then, only
  // when the high byte is written, can we actually write to the NVR.  This
  // system isn't foolproof, but it's good enough to fool the SBCT11 firmware.
  //--
  if (m_fOldPCB) {
    if (!fOdd)
      m_wCache = MKWORD(0, bData);
    else {
      m_wCache = MKWORD(bData, LOBYTE(m_wCache));
      m_p12887->DevWrite(m_bAddress, LOBYTE(m_wCache>>1));
    }
  } else
    m_p12887->DevWrite(m_bAddress, bData);
}

uint8_t CRTC11::DevRead (address_t nPort)
{
  //++
  //   Read a byte from the RTC and deal with the issues caused by the old
  // PCB layout bug.  Note that only the "READ" register is readable - any
  // reads from any others always return 0.
  //--
  assert(nPort >= GetBasePort());
  switch (nPort-GetBasePort()) {
    case REG_READ:    return ReadByte(false);
    case REG_READ+1:  return ReadByte(true);
    default:          return 0;
  }
}

void CRTC11::DevWrite (address_t nPort, uint8_t bData)
{
  //++
  //   Write a byte to the RTC or the RTC address register and deal with the
  // PCB bugs.  Note that we don't have to worry about the upper (odd) data
  // byte when writing to the address register, because it has only seven
  // bits.  Even on the old PCB addresses never overflow into the upper byte.
  //--
  assert(nPort >= GetBasePort());
  switch (nPort-GetBasePort()) {
    case REG_ADDRESS:  m_bAddress = (m_fOldPCB ? bData>>1 : bData) & 0x7F;  break;
    case REG_WRITE:    WriteByte(bData, false);  break;
    case REG_WRITE+1:  WriteByte(bData, true);   break;
    default: break;
  }
}

void CRTC11::ShowDevice (ostringstream &ofs) const
{
  //++
  //--
  m_p12887->ShowDevice(ofs);
}
