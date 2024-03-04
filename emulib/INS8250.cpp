//++
// INS8250.cpp -> emulate a UART connected to a dumb terminal
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
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
//   The CINS8250 class implements a National 8250 UART connected to a simple
// terminal, the latter being emulated by a CConsoleWindow object.  The 8250
// is a fairly complex UART, complete with an internal baud rate generator
// and full modem control.  The 8250, and it's more famous cousins the 16450
// and 16550, are the standard for IBM PC serial ports.
//
//   Our 8250 emulation is fairly lame.  The baud rate generate is not used
// and the baud rate divisor latch is ignored.  Any divisor, including zero,
// will work here.  The character format (parity, stop bits, word length,
// etc) is also ignored.  Parity errors and framing errors never occur.  The
// modem control and modem status registers are implemented only so far as is
// needed for loopback to work.  Oh, but loopback however, IS implemented.
//
//   Interrupts are NOT currently implemented, however the intention is to do
// so someday and all the necessary hooks should be in here.  The ELF2K system
// doesn't use interrupts, so they're currently unnecessary.
//    
// REVISION HISTORY:
// 12-AUG-19  RLA   New file.
// 21-JAN-20  RLA   Finally get the basic, non-interrupt, version working!
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
#include "VirtualConsole.hpp"   // console window functions
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "UART.hpp"             // generic UART base class
#include "INS8250.hpp"          // declarations for this module


CINS8250::CINS8250 (const char *pszName, uint16_t nBase, CEventQueue *pEvents, CVirtualConsole *pConsole, CCPU *pCPU)
  : CUART (pszName, "INS8250", "UART Console Emulation", nBase, nBase+REG_COUNT-1, pEvents, pConsole, pCPU)
{
  //++
  //--
  m_bRBR = m_bTHR = m_bIER = m_bLCR = m_bLSR = 0;
  m_bMCR = m_bMSR = m_bSCR = m_bIIR = 0;
  m_wDivisor = 0;
}

void CINS8250::ClearDevice()
{
  //++
  //   This method is called by a hardware reset or master clear function.
  // It initializes all the UART registers to their correct values, and it
  // schedules the first event for receiver polling.  The latter is critical,
  // because if we don't schedule polling now then we never will!
  //--
  m_bRBR = m_bTHR = m_bIER = m_bLCR = 0;
  m_bMCR = m_bMSR = m_bSCR = 0;
  m_bIIR = IIR_NOINT;
  m_bLSR = LSR_THRE|LSR_TEMT;
  m_wDivisor = 0;
  CUART::ClearDevice();
}

void CINS8250::UpdateIER (uint8_t bNew)
{
  //++
  //   Update the interrupt enable register and handle any side effects (like
  // setting or clearing interrupt requests!!) ...
  //--
  //NOT YET IMPLEMENTED NYI TODO TBA!!
  m_bIER = bNew;
}

void CINS8250::UpdateMSR (uint8_t bNew)
{
  //++
  //   Update the modem status register (MSR) with the new value given.  By
  // itself that would be trivial, but this also handles the side effects of
  // that update, including setting the delta (e.g. DDCD, DCTS, DDSR and TERI)
  // bits, and also requesting a modem status change interrupt (if enabled).
  //--

  // Strip any delta bits from the new value ...
  CLRBIT(bNew, MSR_DELTA);

  // And now set the delta bits as required ...
  if ((m_bMSR & MSR_CTS) != (bNew & MSR_CTS)) SETBIT(bNew, MSR_DCTS);
  if ((m_bMSR & MSR_DCD) != (bNew & MSR_DCD)) SETBIT(bNew, MSR_DDCD);
  if ((m_bMSR & MSR_DSR) != (bNew & MSR_DSR)) SETBIT(bNew, MSR_DDSR);
  if (ISSET(m_bMSR, MSR_RI) && !ISSET(bNew, MSR_RI)) SETBIT(bNew, MSR_TERI);

  // Update the MSR and, if any of the delta bits are set, request an interrupt.
  m_bMSR = bNew;
//if (ISSET(m_bMSR, MSR_DELTA) request an interrupt...
}

uint8_t CINS8250::ReadMSR()
{
  //++
  //   Read the MSR register and handle all the side effects.  In the case of
  // the MSR, the side effects are clearing the delta bits and any associated
  // interrupt request ...
  //--
//if (ISSET(m_bMSR, MSR_DELTA)) clear any associated interrupt request
  uint8_t bTemp = m_bMSR;
  CLRBIT(m_bMSR, MSR_DELTA);  return bTemp;
}

void CINS8250::UpdateMCR (uint8_t bNew)
{
  //++
  //   Update the modem control register (MCR) and handle the side effects.
  // Since we don't actually implement any modem control, the only side effect
  // we have to worry about is loopback mode.  If loopback is turned on, then
  // writing the MCR will change the corresponding bits in the MSR (including
  // any side effects that might cause).
  //--
  m_bMCR = bNew;
  if (IsLoopback()) {
    uint8_t bMSR = 0;
    if (ISSET(m_bMCR, MCR_RTS))  SETBIT(bMSR, MSR_CTS);
    if (ISSET(m_bMCR, MCR_DTR))  SETBIT(bMSR, MSR_DSR);
    if (ISSET(m_bMCR, MCR_OUT1)) SETBIT(bMSR, MSR_RI);
    if (ISSET(m_bMCR, MCR_OUT2)) SETBIT(bMSR, MSR_DCD);
    UpdateMSR(bMSR);
  }
}

void CINS8250::UpdateLSR (uint8_t bNew)
{
  //++
  //   This method will update the line status register and handle any side
  // effects, such as requesting interrupts for errors, received data available
  // or transmitter buffer empty.  Note that it's not possible for the CPU to
  // write the LSR register, so this routine is only used internally to update
  // the status when we receive a keyboard key or the transmit timer expires.
  //--
  m_bLSR = bNew;
  // Handle interrupts associated with the LSR!!
}

uint8_t CINS8250::ReadLSR()
{
  //++
  //   Read the line status register and handle all the side effects.  Reading
  // the LSR clears various error bits (PE, FE, OE, etc) and also clears any
  // associated line status interrupt.  Note that it does NOT clear the DR,
  // THRE, TEMT bits or any interrupts associated with those!
  //--
  uint8_t bTemp = m_bLSR;
  CLRBIT(m_bLSR, (LSR_OE|LSR_PE|LSR_FE|LSR_BI));
//Clear any interrupts associated with the LSR!!
  return (bTemp & 0x7F);
}

void CINS8250::UpdateLCR (uint8_t bNew)
{
  //++
  //   This method is called when the line control register (LCR) is updated.
  // In theory this could have some side effects, but the reality is that all
  // the LCR controls is the character format (stop bits, data bits, even/odd
  // parity, etc) and we justs don't care about any of those.  The only thing
  // we do care about is the DLAB bit, and that's handled by IsDLAB() ...
  //--
  m_bLCR = bNew;
}

void CINS8250::UpdateRBR (uint8_t bNew)
{
  //++
  //   This method is called whenever the console detects a new keypress, or
  // in loopback mode whenever a character is transmitted.  It will load the
  // received byte into the receiver buffer, set the data ready (DR) bit, and
  // request a receiver interrupt if that's enabled.
  //--
  m_bRBR = bNew;
  UpdateLSR(LSR_DR, 0);
}

uint8_t CINS8250::ReadRBR()
{
  //++
  //   Reading the receiver buffer register returns the last byte received,
  // AND it also clears the data ready (DR) bit and any associated receiver
  // interrupt request ...  Note that it doesn't actually clear the RBR -
  // the CPU can read the same byte over and over until a new one arrives.
  //
  //   Note that character reception is scheduled by the event queue just like
  // transmitted characters.  This is necessary to prevent a user from typing
  // on the console window faster than the UART can receive.  In real life
  // that's pretty much impossible, but a simulated CPU with a simulated UART
  // is a lot slower and it's a real problem.  Console keyboard input is
  // therefore buffered until the UART is ready to receive it.
  //--
  UpdateLSR(0, LSR_DR);
  // Schedule a new event!!??
  return m_bRBR;
}

void CINS8250::WriteTHR (uint8_t bData)
{
  //++
  //   Writing the THR clears the TEMT and THRE bits in the line status, and
  // also clears any transmitter interrupt that might currently be requested.
  // It then sends the character to the console and schedules an event for one
  // character time in the future to wake us up and set the THRE bits again.
  //
  //   UNLESS the loopback bit is set, in which case we don't send this byte
  // to the console after all.  Instead we simply schedule the event and then
  // that will copy the THR to the RBR and generate a receiver interrupt.
  //
  //   FWIW, note that we don't really implement the TEMT bit correctly.  In
  // reality the transmitter is double buffered and the TEMT bit should lag
  // one character time behind THRE.  We just implement them together, though.
  // Most software doesn't care about TEMT, and it's too much trouble to do
  // it correctly!
  //--
  m_bTHR = bData;
  UpdateLSR(0, (LSR_THRE|LSR_TEMT));
  StartTransmitter(bData, IsLoopback());
}

void CINS8250::TransmitterDone()
{
  //++
  //   Here for a transmitter done event - this means that enough simulated
  // time has elapsed for the last character that was loaded in the THR to
  // have been transmitted.  It's time to clear the THRE and TEMT bits, and
  // to generate an interrupt for that if so enabled.
  //
  //   If loopback mode is enabled, then copy the transmitter character to
  // the received and generate a receiver done interrupt too.
  //--
  if (IsLoopback()) UpdateRBR(m_bTHR);
  UpdateLSR((LSR_THRE|LSR_TEMT), 0);
}

uint8_t CINS8250::DevRead (uint16_t nRegister)
{
  //++
  //   The Read() method just returns the contents of the addressed register.
  // Most of them are trivial, but a few of them have side effects (e.g. reading
  // the MSR or LSR will clear the error or delta bits contained there in).
  // Also, remember that registers 0 and 1 address the divisor latch when the
  // DLAB bit is set.
  //--
  assert(nRegister >= GetBasePort());
  switch (nRegister - GetBasePort()) {
    case REG_RBR:  return IsDLAB() ? LOBYTE(m_wDivisor) : ReadRBR();
    case REG_IER:  return IsDLAB() ? HIBYTE(m_wDivisor) : m_bIER;
    case REG_IIR:  return m_bIIR;
    case REG_LCR:  return m_bLCR;
    case REG_MCR:  return m_bMCR;
    case REG_LSR:  return ReadLSR();
    case REG_MSR:  return ReadMSR();
    case REG_SCR:  return m_bSCR;
    default:
      assert(false);
      return 0xFF;  // just to avoid C4715 errors!
  }
}

void CINS8250::DevWrite (uint16_t nRegister, uint8_t bData)
{
  //++
  //   And this method will write data to an 8250 register.  Like Read(), there
  // are side effects to worry about and of course there's the whole DLAB
  // thing.  Also, a few of the registers are read only, and writing them has
  // no effect. 
  //--
  assert(nRegister >= GetBasePort());
  switch (nRegister - GetBasePort()) {
    case REG_THR:
      // transmitter holding register (write only!)
      if (IsDLAB())
        m_wDivisor = MKWORD(HIBYTE(m_wDivisor), bData);
      else
        WriteTHR(bData);
      break;

    case REG_IER:
      // interrupt enable register ...
      if (IsDLAB())
        m_wDivisor = MKWORD(bData, LOBYTE(m_wDivisor));
      else
        UpdateIER(bData);
      break;

    case REG_LCR:
      // line control register ...
      UpdateLCR(bData);  break;
    case REG_MCR:
      // modem control register ...
      UpdateMCR(bData);  break;
    case REG_SCR:
      // scratch register ...
      m_bSCR = bData;  break;

    case REG_IIR:
      //   The interrupt indentification register is read only!  In this case
      // the datasheet explicitly says as much.  Note that on the 16450/550
      // writing to register 2 writes the FCR (FIFO Control Register), but we
      // don't have one of those!
    case REG_LSR:
      //   According to the datasheet, "the LSR is inteded for read operations
      // only" and "writing to this register is not recommended" ...
    case REG_MSR:
      //   Is it actually possible to write the MSR?  The CTS, DCD, DSR and RI
      // bits are wired directly to the associated inputs and can't be changed.
      // Maybe it's possible to write the delta bits and cause an interrupt?
      // Unlike the LSR, the datasheet is silent on this topic.
      break;

    default:
      // Anything else is a coding error!
      assert(false);
  }
}

void CINS8250::ShowDevice (ostringstream &ofs) const
{
  //++
  //   This routine will dump the state of the internal UART registers.
  // This is used by the UI EXAMINE command ...
  //--
  ofs << FormatString("RBR=0x%02X THR=0x%02X IER=0x%02X IIR=0x%02X SCR=0x%02X", m_bRBR, m_bTHR, m_bIER, m_bIIR, m_bSCR);
  ofs << std::endl;
  ofs << FormatString("LCR=0x%02X MCR=0x%02X LSR=0x%02X MSR=0x%02X DIV=%d", m_bLCR, m_bMCR, m_bLSR, m_bMSR, m_wDivisor);
}

