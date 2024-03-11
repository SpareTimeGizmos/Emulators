//++
// UART.cpp -> Generic UART to console terminal emulator class
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
//   This class implements a generic UART emulator that's logically connected
// to the console terminal, the latter being emulated by a CConsoleWindow
// object.  This class is not intended to be used by itself, but as a base
// class for a specific UART emulation (e.g. INS8250, CDP1854, S2651, etc).
// This class essentially handles the interaction with the console window and
// the event scheduling, and the emulation of the specific UART registers is
// left to any derived classes.
//    
// REVISION HISTORY:
//  7-FEB-20  RLA   New file (split from the 8250 implementation)
// 21-JUN-22  RLA   Don't write NULs to the console
// 10-JUL-22  RLA   Allow pConsole to be NULL for a disconnected UART
// 18-JUL-22  RLA   Change CVirtualConsole to RawRead() and RawWrite()
// 20-JUL-22  RLA   On transmitting, DON'T DROP ZERO BYTES!
//                  Don't trim the data to 7 bits either!
// 26-AUG-22  RLA   Constructor should test pConsole for NULL, not m_pConsole!
// 20-NOV-23  RLA   Rewrite the code at ReceiverReady() to always poll for ^E
// 16-DEC-23  RLA   Add text & XMODEM speeds to ShowDevice()
// 10-MAR-24  RLA   Add received break support
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "VirtualConsole.hpp"   // console window functions
#include "SmartConsole.hpp"     // console with file transfer
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "MemoryTypes.h"        // address_t and word_t data types
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "UART.hpp"             // declarations for this module


CUART::CUART (const char *pszName, const char *pszType, const char *pszDescription, address_t nPort, address_t cPorts,
              CEventQueue *pEvents, CVirtualConsole *pConsole, CCPU *pCPU)
     : CDevice (pszName, pszType, pszDescription, INOUT, nPort, cPorts, pEvents)
{
  //++
  // Set the default polling intervals ...
  //--
  assert(pConsole != NULL);
  m_pConsole = pConsole;  m_pCPU = pCPU;
  //   The default UART speed is 2000 characters per second.  That's a little
  // more than 19.2kBps (in simulated time, of course).
  m_llCharacterTime = m_llPollingInterval = HZTONS(DEFAULT_SPEED);
  //   The default time that a serial BREAK condition remains asserted is
  // 100ms.  That's roughly 1 character time at 110 baud...
  m_llBreakTime = MSTONS(100);
  // Clear all the rest of the device state ...
  ClearDevice();
}

void CUART::ClearDevice()
{
  //++
  //   This method is called by a hardware reset or master clear function,
  // and it schedules the first event for receiver polling.  That's critical,
  // because if we don't schedule polling now then we never will!
  //--
  CDevice::ClearDevice();
  m_fReceivingBreak = false;
  if (m_pConsole != NULL) m_pConsole->SendSerialBreak(false);
  ScheduleEvent(EVENT_RXREADY, m_llPollingInterval);
}

void CUART::StartTransmitter (uint8_t bData, bool fLoopback)
{
  //++
  //   This method should be called whenever a new character is written to the
  // transmitter holding register.  It will send the character to the console
  // and schedule a transmitter done event for the near future.  This event
  // will call TransmitterDone(), which should set the transmitter holding
  // register empty bit (or whatever this particular UART has).
  //
  //   If fLoopback is true, then we DON'T send the character to the console,
  // but we still schedule the transmitter done event.  It's assumed that
  // method will then copy the transmitter holding register to the receiver
  // buffer, simulating a looped back tranmission.  This is used by UARTS that
  // implement a loopback mode, like the 8250.
  //
  //   Note that we current trim all characters to 7 bits.  Some old software
  // wants to transmit characters with the 8th bit always set, and that
  // causes wierd characters to appear in the console window.
  //--
  if (!fLoopback && (m_pConsole != NULL))
    m_pConsole->RawWrite((const char *) &bData, 1);
  //   It's possible for a badly behaved program to transmit a second character
  // before the previous character has finished sending.  In that case there'll
  // already be a TXDONE event pending and we have to be careful not to create
  // a second one!  It's not absolutely clear what the hardware will do it this
  // situation, but I'll assume the first byte gets trashed and that the flag
  // sets after an appropriate interval for the second byte...
  CancelEvent(EVENT_TXDONE);
  ScheduleEvent(EVENT_TXDONE, m_llCharacterTime);
}

void CUART::ReceiverReady()
{
  //++
  //   This method is called by the receiver ready event.  It polls the console
  // keyboard for any input and, if it finds something, calls UpdateRBR() to 
  // put the character in the receiver buffer register and sets the data ready
  // bit.  Regardless of whether we find any characters this time, we schedule
  // another receiver ready event for the near future. It's the constant stream
  // of these events that polls the console for keyboard input and passes it
  // into the emulation.  Without them you wouldn't be able to type!
  //--
  uint8_t bData;
  if (m_pConsole != NULL) {
    //   See if a console break (usually ^E) was entered and, if it was, 
    // interrupt this emulation and return to the command parser.
    if (m_pConsole->IsConsoleBreak()) m_pCPU->Break();
    //   If a serial break (not to be confused with the console break
    // above) was entered, we need to simulate receiving a long space
    // break condition on this UART.  Set the m_fReceivingBreak flag to
    // remember that we're in this condition and schedule a future event
    // to clear that flag.  Note that if we're already in a break state,
    // then we just ignore further requests until the current one times
    // out.
    if (m_pConsole->IsReceivingSerialBreak() && !m_fReceivingBreak) {
      m_fReceivingBreak = true;
      ScheduleEvent(EVENT_BRKDONE, m_llBreakTime);
    }
    //   And lastly, if the receiver isn't busy then poll for ordinary input.
    // Notice that we don't poll for input when we're in the receiving break
    // state - a real UART can't receive anything in that condition!
    if (!m_fReceivingBreak && !IsRXbusy()) {
      int32_t nRet = m_pConsole->RawRead(&bData, 1);
      if (nRet > 0) UpdateRBR(MASK8(bData));
    }
  }
  ScheduleEvent(EVENT_RXREADY, m_llPollingInterval);
}

void CUART::ReceivingBreakDone()
{
  //++
  //    This routine will clear the receiving break condition.  It's usually
  // called by the EventCallback() when the break interval expires, but it can
  // be called explicitly to terminate the break state early if needed.  That
  // latter is why we cancel any pending BRKDONE events.
  //--
  CancelEvent(EVENT_BRKDONE);
  m_fReceivingBreak = false;
}

void CUART::EventCallback (intptr_t lParam)
{
  //++
  // Handle event callbacks for this device ...
  //--
  switch (lParam) {
    case EVENT_TXDONE:  TransmitterDone();     break;
    case EVENT_RXREADY: ReceiverReady();       break;
    case EVENT_BRKDONE: ReceivingBreakDone();  break;
    default: assert(false);
  }
}

void CUART::ShowDevice (ostringstream &ofs) const
{
  //++
  // Show the UART settings (for the UI "SHOW DEVICE xxx" command).
  //--
  ofs << FormatString("Transmit speed %ld cps, Receive speed %ld cps",
    NSTOHZ(m_llPollingInterval), NSTOHZ(m_llCharacterTime));
  if (m_pCPU != NULL)
    ofs << FormatString(", BREAK Control-%c", m_pConsole->GetConsoleBreak()+'@');
  ofs << std::endl;

  //   IF our console window is actually a CSmartConsole object, then also
  // show the text and XMODEM download speeds.
  CSmartConsole *p = dynamic_cast<CSmartConsole *> (m_pConsole);
  if (p != NULL) {
    uint64_t qLineDelay, qCharDelay, qXdelay;
    p->GetTextDelays(qCharDelay, qLineDelay);  p->GetXdelay(qXdelay);
    ofs << FormatString("Text speed %ld cps, end of line delay %lld ms, XMODEM speed %ld cps",
      NSTOCPS(qCharDelay), NSTOMS(qLineDelay), NSTOCPS(qXdelay));
    ofs << std::endl;
  }
}
