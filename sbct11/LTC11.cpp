//++
// LTC11.cpp -> SBCT11 Line Time Clock implementation
//
// LICENSE:
//    This file is part of the emulator library project.  SBCT11 is free
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
//   The SBCT11 implements a fairly standard PDP11 line time clock.  It is a
// superset of the DEC BDV11 and MXV11 implementations (which implement only
// the LTE bit, and that as write only) and a variation of the KPV11.
//
//   The LTCCSR register contains two bits - ENABLE (bit 6) is the line time
// clock enable, and FLAG (bit 7) is the current status of the clock flip-flop.
// The enable bit may be set or cleared by writing a 1 or 0 to bit 6 of address
// 177546. The enable flag may be read back, along with the current state of
// the LTC flip flop, by reading address 177546.  The flag is read only and
// cannot be written.  The other bits in this register are undefined and should
// be ignored.
// 
//   Note that reading this register WILL NOT CLEAR the LTC flag.  Also note
// that the flag bit WILL NOT TOGGLE unless the enable bit is also set.  If you
// simply want to watch the bit toggle in software you can always avoid LTC
// interrupts by raising the processor priority to level 7.  The ENABLE bit is
// cleared at power up and by BCLR.
// 
// IMPORTANT!
//    Presently this clock operates exclusively on SIMULATED time.  It has no
// connection to wall clock time in the real world, and naturally your RT-11
// system won't keep accurate time of day.  Some day we might fix that, but
// not today.
//
// NOTE
//   An observant reader will notice that the FLAG bit doesn't actually exist
// anywhere in this module.  That's because we use the interrupt request bit in
// the CSimpleInterrupt object as our flag.  We do that because this bit will be
// automatically cleared when the DCT11 acknowledges the LTC interrupt, which is
// exactly what we want to happen.
// 
// REVISION HISTORY:
// 10-JUL-22  RLA   New file.
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
#include "LTC11.hpp"            // declarations for this module


CLTC11::CLTC11 (address_t nBase, CEventQueue *pEvents)
  : CDevice("LTC", "LTC11", "Line Time Clock", INOUT, nBase, 2, pEvents)
{
  //++
  //--
  m_fEnable = false;
}

void CLTC11::ClearDevice()
{
  //++
  //   BCLR clears both the enable and the flag/interrupt request bits, and
  // schedules a tick event for the next clock tick.
  //--
  m_fEnable = false;
  RequestInterrupt(false);
  CDevice::ClearDevice();
  ScheduleEvent(EVENT_TICK, HZTONS(HERTZ));
}

void CLTC11::AttachInterrupt (CSimpleInterrupt* pInterrupt)
{
  //++
  // Make sure that any interrupt we attach is edge triggered!
  //--
  assert(pInterrupt != NULL);
  assert(pInterrupt->GetMode() == CSimpleInterrupt::EDGE_TRIGGERED);
  CDevice::AttachInterrupt(pInterrupt);
}

uint8_t CLTC11::DevRead (address_t nPort)
{
  //++
  //   Read from the LTCCSR.  There's really only one byte (and only two actual
  // bits at that) which can be read.  Note that the even CSR address is the
  // one with the bits - the odd address isn't implemented on the SBCT11 and
  // will return junk if read.
  //--
  assert(nPort >= GetBasePort());
  assert((nPort - GetBasePort()) < 2);
  if (ISEVEN(nPort))
    return   (m_fEnable ? LTC_ENABLE : 0)
           | (IsInterruptRequested() ? LTC_FLAG : 0);
  else
    return 0xFF;
}

void CLTC11::DevWrite (address_t nPort, uint8_t bData)
{
  //++
  //   Write to the LTCCSR.  This is even easier still, since there's only
  // one bit which can actually be written.  Everything else is ignored.
  // Note that writing a zero to the ENABLE will also clear the flag.
  //--
  assert(nPort >= GetBasePort());
  assert((nPort - GetBasePort()) < 2);
  if (ISEVEN(nPort)) {
    bool fWasEnabled = m_fEnable;
    m_fEnable = ISSET(bData, LTC_ENABLE);
    if (!m_fEnable) RequestInterrupt(false);
    if (m_fEnable != fWasEnabled)
      LOGF(TRACE, "line time clock %s", m_fEnable ? "ENABLED" : "DISABLED");
  }
}

void CLTC11::EventCallback (intptr_t lParam)
{
  //++
  //   The LTC tick always sets the flag, if the LTC is enabled, and will
  // also request an interrupt.  Note that it sets the flag - it doesn't
  // toggle - because the flag will be cleared by an interrupt acknowledge.
  //--
  if (m_fEnable) RequestInterrupt(true);
  ScheduleEvent(EVENT_TICK, HZTONS(HERTZ));
}
