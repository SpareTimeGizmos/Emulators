//++
// CDP1854.cpp -> emulate a UART connected to a dumb terminal
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
//   The CCDP1854 class implements an RCA CDP1854 UART connected to a simple
// terminal, the latter being emulated by a CConsoleWindow object.  
//    
// REVISION HISTORY:
// 12-AUG-19  RLA   New file.
// 21-JAN-20  RLA   Finally get the basic, non-interrupt, version working!
// 21-JUN-22  RLA   Add GetSense() for the IRQ and DA bits
//                  Hack to make TSRE lag behind THRE
//                  Use RequestInterrupt() to (gasp!) request an interrupt
// 23-JUN-22  RLA   If the IE bit in the control register is changed, then
//                    update our interrupt request status too!
// 28-FEB-24  RLA   On the 1854, BREAK inhibits the transmitter!
// 10-MAR-24  RLA   Add received break support
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
#include "ConsoleWindow.hpp"    // console window functions
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "MemoryTypes.h"        // address_t and word_t data types
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "UART.hpp"             // generic UART base class
#include "CDP1854.hpp"          // declarations for this module


CCDP1854::CCDP1854 (const char *pszName, address_t nBase, CEventQueue *pEvents, CVirtualConsole *pConsole, CCPU *pCPU, address_t nSenseIRQ, address_t nSenseBRK)
  : CUART (pszName, "CDP1854", "UART Emulation", nBase, REG_COUNT, pEvents, pConsole, pCPU)
{
  //++
  //--
  m_bRBR = m_bTHR = m_bCTL = m_bSTS = 0;
  m_fIRQ = m_fTHRE_IRQ = false;
  m_nSenseIRQ = nSenseIRQ;  m_nSenseBRK = nSenseBRK;
}

void CCDP1854::ClearDevice()
{
  //++
  //   This method is called by a hardware reset or master clear function.
  // It initializes all the UART registers to their correct values, and it
  // schedules the first event for receiver polling.  The latter is critical,
  // because if we don't schedule polling now then we never will!
  //--
  m_bRBR = m_bTHR = m_bCTL = 0;
  m_bSTS = STS_THRE|STS_TSRE;
  m_fIRQ = m_fTHRE_IRQ = false;
  RequestInterrupt(false);
  CUART::ClearDevice();
}

void CCDP1854::UpdateStatus (uint8_t bNew)
{
  //++
  //   This method will update the status register and handle any side effects,
  // such as requesting an interrupt.  Note that it's not possible for the CPU
  // to write the status register, so this routine is only used internally to
  // update the status when we receive a keyboard key or the transmit timer
  // expires.
  //
  //   Note that an interrupt is requested when 1) the IE bit is set in the
  // control register, and either 2a) DA is set, or 2b) THRE makes a 0 to 1
  // transition.  The DA interrupt is level sensitive and IRQ will be asserted
  // as long as this bit is set, however when the firmware reads the receiver
  // buffer register DA will be cleared and this interrupt reset.
  //
  //   The THRE interrupt is a little bit more complicated.  This interrupt
  // is requested when the THRE bit is set, and the interrupt is cleared by
  // reading the status register.  The THRE bit is NOT cleared, however, until
  // a new byte is loaded into the transmitter register.  We need an extra
  // flag, m_fTHRE_IRQ, to keep track of this.
  //
  //   Note that in theory the transmitter shift register can also generate an
  // interrupt request, but we don't actually emulate that.  In our emulation
  // the TSRE bit always matches the THRE bit, so we don't have to worry about
  // this interrupt source.
  //--
  bool fDA_IRQ = ISSET(bNew, STS_DA);
  if (ISSET(bNew, STS_THRE) && !ISSET(m_bSTS, STS_THRE)) m_fTHRE_IRQ = true;
  m_fIRQ = ISSET(m_bCTL, CTL_IE) ? (fDA_IRQ|m_fTHRE_IRQ) : false;
  RequestInterrupt(m_fIRQ);
  m_bSTS = bNew;
}

uint8_t CCDP1854::ReadSTS()
{
  //++
  //   This method is called when the status register is read.  Reading the
  // status register clears the THRE interrupt request, but it DOES NOT
  // clear the THRE bit!  It also clears the PSI bit, however we don't 
  // implement that.
  //--
  m_fTHRE_IRQ = false;
  UpdateStatus(0, 0);
  //   If the UART is currently simulating receiving a break character, then
  // set the framing error bit in the status.  A break condition will cause
  // a continuous stream of framing errors as long as it persists.
  if (IsReceivingBreak()) SETBIT(m_bSTS, STS_FE);
  //   If we find THRE set then also clear TSRE, but return the value of the
  // status byte BEFORE we do that.  This is a hack to make TSRE lag behind
  // THRE a little bit.  See TransmitterDone() for more details...
  uint8_t bOldSTS = m_bSTS;
  if (ISSET(m_bSTS, STS_THRE)) m_bSTS |= STS_TSRE;
  return bOldSTS;
}

void CCDP1854::UpdateRBR (uint8_t bChar)
{
  //++
  //   This method is called whenever the console detects a new keypress, or
  // in loopback mode whenever a character is transmitted.  It will load the
  // received byte into the receiver buffer, set the data availabe (DA) bit in
  // the status register, and request a receiver interrupt if that's enabled.
  // Note that if the DA bit is already set, then the firmware hasn't read the
  // last character we received yet, and so the overrun (OE) bit is also set.
  //--
  m_bRBR = bChar;
  if (ISSET(m_bSTS, STS_DA))
    UpdateStatus(STS_OE, 0);
  else
    UpdateStatus(STS_DA, 0);
}

uint8_t CCDP1854::ReadRBR()
{
  //++
  //   This method will read the receiver buffer register and clear the data
  // available (DA) and overrun error (OE) status bits ...
  //
  //   Note that character reception is scheduled by the event queue just like
  // transmitted characters.  This is necessary to prevent a user from typing
  // on the console window faster than the UART can receive.  In real life
  // that's pretty much impossible, but a simulated CPU with a simulated UART
  // is a lot slower and it's a real problem.  Console keyboard input is
  // therefore buffered until the UART is ready to receive it.
  //--
  UpdateStatus(0, STS_DA|STS_OE);
  return m_bRBR;
}

void CCDP1854::WriteTHR (uint8_t bChar)
{
  //++
  //   Writing the THR clears the THRE and TSRE bits in the status register,
  // and also clears any associated THRE/TSRE interrupt request.  It sends
  // the character to the console and schedules an event for one character
  // time in the future to wake us up and set the THRE/TSRE bits again.
  // 
  //   Note that when the BREAK bit is set in the control register, the entire
  // transmitter is inhibited.  Writing the transmitter buffer has no effect
  // on THRE or TSRE.  It's a bit unusual, but that's the way the 1854 works.
  //--
  if (ISSET(m_bCTL, CTL_BREAK)) return;
  m_bTHR = bChar;  m_fTHRE_IRQ = false;
  UpdateStatus(0, (STS_THRE|STS_TSRE));
  StartTransmitter(bChar);
}

void CCDP1854::WriteCTL (uint8_t bData)
{
  //++
  //   This method is called when the control register is loaded.  This would
  // be trivial except for the action of the TR (transmit request) bit.  This
  // bit asserts the RTS output (which we don't emulate) and also generates a
  // transmitter interrupt IF the THRE bit is currently set.  
  //
  //   Note that the other bits in the control register, including IE, can only
  // be written when TR is zero!
  //--
  if (ISSET(bData, CTL_TR)) {
    if (ISSET(m_bSTS, STS_THRE)) {m_fTHRE_IRQ = true;  UpdateStatus(0,0);}
    SETBIT(m_bCTL, CTL_TR);
  } else {
    uint8_t bOldCTL = m_bCTL;
    m_bCTL = bData;
    //   If the state of the IE bit has changed, then we need to update our
    // interrupt request status too...
    if (ISSET((m_bCTL^bOldCTL), CTL_IE)) UpdateStatus(m_bSTS);
    // If the state of the BREAK bit has changed, then update the console break.
    if (ISSET((m_bCTL^bOldCTL), CTL_BREAK)) m_pConsole->SendSerialBreak(ISSET(m_bCTL, CTL_BREAK));
  }
}

void CCDP1854::TransmitterDone()
{
  //++
  //   Here for a transmitter done event - this means that enough simulated
  // time has elapsed for the last character that was loaded in the THR to
  // have been transmitted.  It's time to clear the THRE bit, and to generate
  // an interrupt for that if so enabled.
  //
  //   FWIW, note that we don't really implement the TSRE bit correctly.  In
  // reality the transmitter is double buffered and the TSRE bit should lag
  // one character time behind THRE.  The easiest thing would be to make the
  // TSRE bit always follow THRE exactly, but the SBC1802 POST actually makes
  // the effort to check that TSRE lags behind THRE.
  // 
  //   So instead we only set THRE here and that will leave TSRE still clear.
  // There's a hack in the ReadSTS() method to set TSRE if it finds THRE set.
  // That means the first read of the status register after the transmitter
  // finishes will find THRE set but TSRE still cleared, and then the next
  // read status after that will find both cleared.  It's a kludge, but it
  // works well enough to fool the firmware.
  // 
  //   And one last issue - the way the CDP1854 handles transmitting BREAK
  // is a bit weird.  As long as the BREAK bit is sent in the control register
  // the transmitter is completely inhibited.  THRE and TSRE never change,
  // and characters written to the transmit buffer aren't sent.
  //--
  if (!ISSET(m_bCTL, CTL_BREAK)) UpdateStatus(STS_THRE, 0);
}

uint1_t CCDP1854::GetSense (address_t nSense, uint1_t bDefault)
{
  //++
  //   On the SBC1802 the CDP1854 interrupt request output is wired to the
  // CPU's EF3 input.  That's easy enough to simulate, however on the RCA
  // MicroBoard CPUs the EF4 input is connected directly to the serial RXD
  // line.  MicroDOS uses this to sense when BREAK is pressed on the terminal
  // and interrupts the current command.
  //--
  if (nSense == m_nSenseIRQ)
    return m_fIRQ ? 1 : 0;
  else if (nSense == m_nSenseBRK)
    //   This returns the raw state of the RXD signal, which some software
    // uses to detect a break condition.  Note that RXD is normally high (1),
    // unless we are in a break condition!
    return IsReceivingBreak() ? 0 : 1;
  else
    return bDefault;
}

word_t CCDP1854::DevRead (address_t nRegister)
{
  //++
  //   The Read() method just returns the contents of the addressed register.
  // Most of them are trivial, but a few of them have side effects (e.g. reading
  // the RBR will clear the DA bit, or reading the status will clear the THRE
  // interrupt).
  //--
  assert(nRegister >= GetBasePort());
  switch (nRegister - GetBasePort()) {
    case REG_RBR:  return ReadRBR();
    case REG_STS:  return ReadSTS();
    default:       assert(false);  return 0xFF;
  }
}

void CCDP1854::DevWrite (address_t nRegister, word_t bData)
{
  //++
  //   And this method will write data to a CDP1854 register.  Like Read(), there
  // are side effects to worry about here too.
  //--
  assert(nRegister >= GetBasePort());
  switch (nRegister - GetBasePort()) {
    case REG_THR:  WriteTHR(bData);  break;
    case REG_CTL:  WriteCTL(bData);  break;
    default:       assert(false);
  }
}

void CCDP1854::ShowDevice (ostringstream &ofs) const
{
  //++
  //   This routine will dump the state of the internal UART registers.
  // This is used by the UI EXAMINE command ...
  //--
  ofs << FormatString("RBR=0x%02X THR=0x%02X STS=0x%02X CTL=0x%02X IRQ=%X\n", m_bRBR, m_bTHR, m_bSTS, m_bCTL, MASK1(m_fIRQ));
  CUART::ShowDevice(ofs);
}
