//++
// DC319.cpp -> DEC DC319 KL11 compatible UART emulation
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
//   The CDC319 class implements a DEC DC319 UART connected to a simple
// terminal, the latter being emulated by a CConsoleWindow object. The DC319
// is a very DEC specific device, being software compatible with the standard
// PDP11 KL11 console terminal interface.
//
// REVISION HISTORY:
//  4-JUL-22  RLA   New file.
// 10-JUL-22  RLA   Implement BREAK in loopback mode only ...
// 19-JUL-22  RLA   Add BREAK support with the new CVirtualConsole ...
// 21-JUL-22  RLA   Only update the interrupt request if the state of the IE
//                    bit actually changes in the RxCSR or TxCSR.
// 16-SEP-25  RLA   Add PBRI and baud rate support.
// 23-SEP-25  RLA   Add split baud rates (for TU58 emulation).
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
#include "Interrupt.hpp"        // CInterrupt definitions
#include "EventQueue.hpp"       // CEventQueue definitions
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "UART.hpp"             // generic UART base class
#include "DC319.hpp"            // declarations for this module


// Table of standard DC319 baud rates ...
//   Unlike the regular UARTs, the internal baud rate generator in the 
// DC319 can only generate a few (eight to be exact) very specific baud
// rates, and these are those.  Note that the order of the baud rates in
// this table MUST correspond to the DC319 XMIT CSR baud rate codes!
uint32_t CDC319::m_alStandardBauds[CDC319::STANDARD_BAUD_RATES] = {
  300, 600, 1200, 2400, 4800, 9600, 19200, 38400
};


CDC319::CDC319 (const char *pszName, address_t nBase, CEventQueue *pEvents, CVirtualConsole *pConsole, CCPU *pCPU)
  : CUART (pszName, "DC319", "DL11 Compatible UART", nBase, REG_COUNT, pEvents, pConsole, pCPU)
{
  //++
  //    The constructor initializes all the DC319 fields and sets the default
  // baud rate to be software enabled (i.e. PBRI is NOT asserted) and 38,400 
  // baud!  You can always change these later ...
  //--
  m_wRxCSR = m_wRxBuf = m_wTxCSR = m_wTxBuf = 0;  m_fEnabled = true;
  m_fPBRI = false;  m_lTxBaud = m_lRxBaud = 38400;
  SETBIT(m_wTxCSR, XMIT_PBRE|XMIT_PBR_38400);
  UpdatePBR(GetPBR(m_wTxCSR));
}

void CDC319::ClearDevice()
{
  //++
  //   This method is called by a hardware reset or master clear function.
  // It initializes all the UART registers to their correct values, and it
  // schedules the first event for receiver polling.  The latter is critical,
  // because if we don't schedule polling now then we never will!
  //
  //   According to the DC319  datasheet, BCLR (INIT) only clears the RCV_IE,
  // XMIT_IE, XMIT_MAINT, and XMIT_BREAK bits.  Apparently it doesn't change
  // anything else.  Specifically it doesn't clear either the RCV_DONE nor the
  // XMIT_READY bits, however since it clears the IE bits it will remove any
  // interrupt request.  It also does not affect the programmed baud rate,
  // if enabled...
  //--
  CLRBIT(m_wRxCSR, RCV_IE);  CLRBIT(m_wTxCSR, XMIT_IE|XMIT_MAINT|XMIT_BREAK);
  // Since we cleared the IE bits, remove any interrupt requests ...
  RequestRxInterrupt(false);  RequestTxInterrupt(false);
  //   Just for grins,  and even though it's technically wrong according to
  // the data sheet, clear RCV_DONE and set XMIT_READY anyway ...
  SETBIT(m_wTxCSR, XMIT_READY);  CLRBIT(m_wRxCSR, RCV_DONE);
  //   Clear the PBRE and PBR bits in the CSR and revert to the hardware baud
  // rate selected.  This is also technically wrong, but we do it anyway ...
  CLRBIT(m_wTxCSR, XMIT_PBR|XMIT_PBRE);  UpdateBaud(m_lTxBaud, m_lRxBaud);
  //   Finallly, clear the underlying UART device too...  This will take care
  // of scheduling an event for polling...
  CUART::ClearDevice();
}

void CDC319::UpdatePBR (uint8_t bNew)
{
  //++
  //   Select a new "programmable" baud rate given the DC319 baud rate code,
  // 0..7.  This is used whether the baud rate was selected by software using
  // the PBR/PBRE bits.
  // 
  //   Note that the argument is an index into the m_awStandardBauds[] table,
  // 0..7, and NOT the actual PBR bits from the XMIT CSR.  The only difference
  // is that the latter are shifted left by three bits!
  //--
  assert(bNew < STANDARD_BAUD_RATES);
  UpdateBaud(m_alStandardBauds[bNew], m_alStandardBauds[bNew]);
}

void CDC319::UpdateBaud (uint32_t lTxBaud, uint32_t lRxBaud)
{
  //++
  //   This will change the DC319 baud rate to a specific bits per second
  // value.  This works even if the baud rate doesn't correspond to one of
  // the DC319 pre-defined values.  We allow different baud rates for the
  // UART transmitter and receiver, even though the real DC319 doesn't do
  // that.  It's a handy feature for emulation to be able to set different
  // TX and RX baud rates, so we cheat a little on accuracy.
  // 
  //   Note that the CUART class wants the TxSpeed and RxSpeed in characters
  // per second, NOT bits per second!  Since the DC319 is hardwired for 8 bits
  // per character and we add two more bits for start and stop, that gives us
  // characters per second = baud /  10.
  // 
  //   If either baud rate is zero, then the current setting is not changed.
  //--
  if (lTxBaud > 0) SetTxSpeed(lTxBaud/BITS_PER_CHARACTER);
  if (lRxBaud > 0) SetRxSpeed(lRxBaud/BITS_PER_CHARACTER);
}

bool CDC319::SetBaud (uint32_t lTxBaud, uint32_t lRxBaud)
{
  //++
  //   This routine is called by the user interface to set the simulated baud
  // rate for the DC319 serial port.  It effectively simulates setting the BRS
  // (baud rate select) input pins on the DC319 bit what actually happens
  // depends on the PBRI input (see SetPBRI(), below!).  
  // 
  //   If PBRI is NOT set (i.e. programmable baud rate is enabled) then the
  // baud rate specified MUST match one of the standard DC319 baud rates.
  // We set the UART to that baud rate, AND we set the PBRE and PBR bits in
  // the DC319 TX CSR to match the selected baud.  If the baud rate given
  // does NOT match a standard DC319 rate, then FALSE is returned and nothing
  // else is changed.
  // 
  //   If PBRI is asserted then you can actually set any baud rate you want,
  // even if it is one that the real DC319 couldn't generate.  That's because
  // the real DC319 is limited to a maximum of 38,400 baud, but this kludge
  // allows you to set much faster rates for emulation.  In this case TRUE is
  // always returned regardless of the baud specified.
  //--
  if (m_fPBRI) {
    // Accept any baud rate, and always clear the PBRE and PBR bits ...
    if (lTxBaud > 0) m_lTxBaud = lTxBaud;
    if (lRxBaud > 0) m_lRxBaud = lRxBaud;
    UpdateBaud(lTxBaud, lRxBaud);
    CLRBIT(m_wTxCSR, XMIT_PBRE|XMIT_PBR);
    LOGF(DEBUG, "%s hardware baud rate set to TX=%d/RX=%d", GetName(), lTxBaud, lRxBaud);
    return true;
  } else {
    // This has to be a standard baud rate, and update the PBRE and PBR bits ...
  uint8_t i;
  if (lTxBaud != lRxBaud) return false;
  for (i = 0; i < STANDARD_BAUD_RATES; i++)
    if (m_alStandardBauds[i] == lTxBaud) break;
  if (i >= STANDARD_BAUD_RATES) return false;
  CLRBIT(m_wTxCSR, XMIT_PBR);  SETBIT(m_wTxCSR, XMIT_PBRE|(i<<3));
  UpdatePBR(i);
  LOGF(DEBUG, "%s software baud rate set to %d (PBR=%d)", GetName(), lTxBaud, i);
  return true;
  }
}

void CDC319::SetPBRI (bool fPBRI)
{
  //++
  //    This routine is called by the UI and simulates the "programmable baud
  // rate inhibit" (aka PBRI) input of the DC319.  When asserted this input
  // disables the software programmable baud rate in the XMIT CSR register.
  // 
  //    Note that asserting the PBRI input on the DC319 actually forces the
  // PBRE and the baud rate bits in the XMIT CSR to zeros.  The WriteTxCSR()
  // routine is responsible for keeping these bits zeroed.
  //--
  if ((m_fPBRI = fPBRI)) {
    //   If we are turning PBRI on, then clear the PBR and PBRE status bits
    // and set the baud rate back to whatever is selected by the DC319 hardware
    // BRS inputs!
    CLRBIT(m_wTxCSR, XMIT_PBR|XMIT_PBRE);
    UpdateBaud(m_lTxBaud, m_lRxBaud);
  } else {
    //   If we're turning PBRI off and the PBRE bit is already set in the TX
    // CSR, then the UART will revert to the baud rate selected by the PBR
    // bits.  If PBRE is not set, then the current baud rate remains unchanged.
    // I'm not clear if that's how the real DC319 works, but that's what we do.
    if (ISSET(m_wTxCSR, XMIT_PBRE)) UpdatePBR(GetPBR(m_wTxCSR));
  }
}

void CDC319::UpdateRBR (uint8_t bData)
{
  //++
  //   This method is called whenever the console detects a new keypress, or
  // in loopback mode whenever a character is transmitted.  It will load the
  // received byte into the receiver buffer, set the receiver done flag, and
  // request a receiver interrupt if that's enabled.
  // 
  //   Note that the receiver active bit, which the DC319 sets during the
  // RS232 START bit and clears during the STOP bit, is not emulated.
  // 
  //   Also note that the only error flags we emulate are OVERRUN and the
  // master error bit.  OVERRUN is set if the receiver done bit is already
  // set when we get here, and the error bit is set any time any error
  // condition exists.  Framing error and break are not currently emulated
  // (and the DC319 doesn't do parity at all!).
  //--
  m_wRxBuf = MKWORD(0, bData);
  if (ISSET(m_wRxCSR, RCV_DONE)) SETBIT(m_wRxBuf, RBUF_OVER|RBUF_ERR);
  SETBIT(m_wRxCSR, RCV_DONE);
  if (ISSET(m_wRxCSR, RCV_IE)) RequestRxInterrupt(true);
}

uint8_t CDC319::ReadRxBuf()
{
  //++
  //   Reading the receiver buffer register returns the last byte received,
  // AND it also clears the RCV_DONE bit and any associated receiver interrupt
  // request.  Note that it doesn't actually alter the RXBUF nor does it clear
  // any of the receiver error flags in RBUG.  The CPU can read the same data
  // over and over until a new character arrives.
  //
  //   Note that character reception is scheduled by the event queue just like
  // transmitted characters.  This is necessary to prevent a user from typing on
  // the console window faster than the UART can receive.  In real life that's
  // pretty much impossible, but a simulated CPU with a simulated UART is a lot
  // slower and it's a real problem.  Console keyboard input is therefore
  // buffered until the UART is ready to receive it.
  //--
  CLRBIT(m_wRxCSR, RCV_DONE);
  //   ALWAYS clear the receiver interrupt request.  If the RCV_IE wasn't set
  // and no interrupt was requested then this does no harm, and if RCV_IE has
  // been cleared in the time since this character was received we don't want
  // to leave the request dangling...
  RequestRxInterrupt(false);
  //  No need to schedule a new event for polling the keyboard here - the CUART
  // base class takes care of that for us.
  return LOBYTE(m_wRxBuf);
}

void CDC319::WriteTxBuf (uint8_t bData)
{
  //++
  //   Writing to the transmitter buffer register clears the transmitter ready
  // bit in the transmitter CSR, clears any interrupt request associated with
  // transmitter ready, and sends the character to the console window.  It
  // schedules an event for one character time in the future to wake us up and
  // set the transmitter ready bit again.
  //
  //   UNLESS the loopback bit is set, in which case we don't send this byte
  // to the console after all.  Instead we simply schedule the event and then
  // that will copy the transmitter buffer directly to the receiver buffer and
  // generate a receiver done interrupt.
  //
  //   Note that the DC319 registers are all 16 bits wide, and this routine is
  // called only for a write to the least significant (even address) byte.  The
  // upper 8 bits of the transmitter buffer are read only and always zero.
  //--
  m_wTxBuf = MKWORD(0, bData);
  CLRBIT(m_wTxCSR, XMIT_READY);
  RequestTxInterrupt(false);
  StartTransmitter(bData, ISSET(m_wTxCSR, XMIT_MAINT));
}

void CDC319::TransmitterDone()
{
  //++
  //   Here for a transmitter done event - this means that enough simulated
  // time has elapsed for the last character that was loaded in the TBUF to
  // have been transmitted.  Set the transmitter ready bit in the transmitter
  // CSR, and generate an interrupt for that if so enabled.
  //
  //   If loopback mode is enabled, then copy the transmitter character to
  // the received and generate a receiver done interrupt too.  Note that
  // BREAK is not really implemented, UNLESS we transmit a BREAK to ourselves
  // in loopback mode!  That's implemented, because the POST does it.
  //--
  if (ISSET(m_wTxCSR, XMIT_MAINT)) {
    UpdateRBR(LOBYTE(m_wTxBuf));
    if (ISSET(m_wTxCSR, XMIT_BREAK)) SETBIT(m_wRxBuf, RBUF_BREAK);
  }
  SETBIT(m_wTxCSR, XMIT_READY);
  if (ISSET(m_wTxCSR, XMIT_IE)) RequestTxInterrupt(true);
}

uint8_t CDC319::ReadRxCSR()
{
  //++
  //   Reading the receiver status register has no side effects (as far as I
  // know!) and we don't really need this routine.  It's here just in case
  // I'm ever proven wrong about that...
  //
  //   Note that the only active bit in the upper byte of the RXCSR is receiver
  // active, and we don't implement that.  The upper byte of our RXCSR always
  // reads as all zeros.
  //--
  return LOBYTE(m_wRxCSR);
}

void CDC319::WriteRxCSR (uint8_t bData)
{
  //++
  //   This routine will update the receiver control register.  The only
  // writable bit here is the RCV_IE bit, and if the software sets that
  // when RCV_DONE is also set we should request an interrupt.  Likewise
  // if the software clears RCV_IE then we should drop any interrupt
  // request, regardless of RX_DONE.
  // 
  //   Note that we have to be careful to make sure that the software doesn't
  // change any bits OTHER than RCV_IE.  Also, the upper byte of the RXCSR
  // is not writable at all.
  //--
  uint8_t bOldIE = m_wRxCSR & RCV_IE;
  uint8_t bNewIE = bData & RCV_IE;
  m_wRxCSR = (m_wRxCSR & ~RCV_IE) | bNewIE;
  if ((bNewIE^bOldIE) != 0) {
    RequestRxInterrupt(ISSET(m_wRxCSR, RCV_IE) && ISSET(m_wRxCSR, RCV_DONE));
  }
}

uint8_t CDC319::ReadTxCSR()
{
  //++
  //   AFAIK reading the transmitter CSR has no side effects, so this routine
  // is here only in the event that I turn out to be wrong!
  // 
  //   Note that the upper byte of the TXCSR is always zero.
  //--
  return LOBYTE(m_wTxCSR);
}

void CDC319::WriteTxCSR (uint8_t bData)
{
  //++
  //   The only bits in the transmitter CSR that are writable are the XMIT_IE,
  // XMIT_BREAK and XMIT_MAINT bits.  The XMIT_READY bit always reflects the
  // current state of the transmitter, and we don't implement the programmable
  // baud rate option.
  // 
  //   On the real DC319 chip the baud rate can be set externally via the BRS
  // inputs, or via software by writing to the XMIT CSR (i.e. this register!)
  // with the PBRE (programmable baud rate enable) bit set and the desired
  // baud rate in the PBR bits.  UNLESS, that is, the DC319 PBRI (programmable
  // baud rate inhibit) input is asserted.  if PBRI is asserted then the PBRE
  // abd PBR bits in the XMIT CSR are forced to zero
  // 
  //   Like the receiver, we update the transmitter interrupt request based on
  // any new state of the XMIT_IE and XMIT_READY bits.
  //--
  uint8_t bNew = bData & (XMIT_IE|XMIT_BREAK|XMIT_MAINT);
  uint8_t bOld = LOBYTE(m_wTxCSR);
  CLRBIT(m_wTxCSR, XMIT_IE|XMIT_BREAK|XMIT_MAINT);
  SETBIT(m_wTxCSR, bNew);
  if (ISSET(bNew^bOld, XMIT_BREAK)  &&  (m_pConsole != NULL))
    m_pConsole->SendSerialBreak(ISSET(m_wTxCSR, XMIT_BREAK));
  if (ISSET(bNew^bOld, XMIT_IE)) {
    RequestTxInterrupt(ISSET(m_wTxCSR, XMIT_READY) && ISSET(m_wTxCSR, XMIT_IE));
  }

  // Handle updating the baud rate ...
  if (!m_fPBRI) {
    //   If PBRI is asserted then the PBR bits are forced to zero always, BUT
    // the data sheet doesn't say what happens if PBRI is NOT asserted and the
    // software writes the XMIT CSR when PBRE is NOT set.  Do the PBR bits
    // change anyway, but the baud rate just doesn't change?  Or are changes to
    // the PBR bits ignored any time PBRE is not set?  We assume, rightly or
    // wrongly, that the PBR bits can be written any time, but that the baud
    // rate only changes when PBRE is set.
    m_wTxCSR = (m_wTxCSR & ~(XMIT_PBR|XMIT_PBRE)) | (bData & (XMIT_PBR|XMIT_PBRE));
    if (!ISSET(m_wTxCSR,XMIT_PBRE) && ISSET(bOld,XMIT_PBRE)) {
      //   If PBRE was set before but it's not set now, then revert to the baud
      // rate selected by the hardware BRS inputs ...
      UpdateBaud(m_lTxBaud, m_lRxBaud);
    } else if (ISSET(m_wTxCSR,XMIT_PBRE)) {
      //   If PBRE is set, then we actually change the simulated baud rate to
      // match what was selected.  This is maybe a little bit of overkill, but
      // it does affect the serial port timing and the TU58 emulation.
      UpdatePBR(GetPBR(m_wTxCSR));
    }
  } else {
    //   The DC319 datasheet is explicit - if PBRI is asserted, then you CANNOT
    // write either the PBRE or PBR bits.  The datasheet doesn't say, however,
    // what happens to these bits when PBRI is de-asserted.  Do they revert to
    // their last state, or do they stay zero?  We assume they stay zero until
    // new values are written to the XMIT CSR by the software.
    CLRBIT(m_wTxCSR, XMIT_PBR|XMIT_PBRE);
  }
}

uint8_t CDC319::DevRead (uint16_t nRegister)
{
  //++
  //   The Read() method returns the contents of the addressed register.
  // There are two complications here - one is that we have an eight bit bus,
  // but some of the DC319 registers are 16 bits.  Most of them only implement
  // the lower 8 bits and the upper byte is always read as zeros, but there are
  // exceptions, most notably the RXBUF.
  // 
  //   The other complication is that reading some registers has side effects,
  // for example reading RXBUF clears RCV_DONE, and that may have a further
  // effect on any interrupts if enabled.  Those side effects are all handled
  // by the individual routines we call here.
  // 
  //   The m_fEnabled flag controls whether this chip is present, and if it is
  // false then the simulation behaves as if the DC319 is not installed.  That
  // basically means all writes are ignored, and all reads return 0177777.
  //--
  assert(nRegister >= GetBasePort());
  if (m_fEnabled) {
    switch (nRegister-GetBasePort()) {
      case REG_RXCSR:   return ReadRxCSR();
      case REG_RXCSR+1: return 0;
      case REG_RXBUF:   return ReadRxBuf();
      case REG_RXBUF+1: return HIBYTE(m_wRxBuf);
      case REG_TXCSR:   return ReadTxCSR();
      case REG_TXCSR+1: return 0;
      case REG_TXBUF:   return LOBYTE(m_wTxBuf);
      case REG_TXBUF+1: return 0;
      default:
        assert(false);  return 0xFF;  // just to avoid C4715 errors!
    }
  } else
    return 0xFF;
}

void CDC319::DevWrite (uint16_t nRegister, uint8_t bData)
{
  //++
  //   And this method will write data to an the addressed register.  We have
  // the same 16 bit register complications here as we do in Read(), but this
  // time NONE of the writable registers have usable bits in the upper byte.
  // We can simply ignore any writes to an odd address.  But of course, there
  // are still side effects to contend with.
  // 
  //   Also note that the RXBUF is READ ONLY!
  //--
  assert(nRegister >= GetBasePort());
  if (m_fEnabled) {
    switch (nRegister-GetBasePort()) {
      case REG_RXCSR:   WriteRxCSR(bData);  break;
      case REG_RXCSR+1:                     break;
      case REG_RXBUF:                       break;
      case REG_RXBUF+1:                     break;
      case REG_TXCSR:   WriteTxCSR(bData);  break;
      case REG_TXCSR+1:                     break;
      case REG_TXBUF:   WriteTxBuf(bData);  break;
      case REG_TXBUF+1:                     break;
      default:          assert(false);
    }
  }
}

void CDC319::ShowDevice (ostringstream &ofs) const
{
  //++
  //   This routine will dump the state of the internal UART registers.
  // This is used by the UI EXAMINE command ...
  //--
  if (m_fEnabled) {
    ofs << FormatString("RXCSR=%06o RXBUF=%06o RXIRQ=%d\n",
      m_wRxCSR, m_wRxBuf, IsInterruptRequestedB());
    ofs << FormatString("TXCSR=%06o TXBUF=%06o TXIRQ=%d\n",
      m_wTxCSR, m_wTxBuf, IsInterruptRequestedA());
    ofs << FormatString("Hardware Baud TX=%d/RX=%d, ", m_lTxBaud, m_lRxBaud);
    if (m_fPBRI)
      ofs << FormatString("PROGRAMMABLE BAUD RATE INHIBITED\n");
    else
      ofs << FormatString(" Software PBR=%d %s\n",
        m_alStandardBauds[(m_wTxCSR & XMIT_PBR) >> 3],
        ISSET(m_wTxCSR, XMIT_PBRE) ? "ENABLED" : "DISABLED");
  } else
    ofs << FormatString("%s DISABLED", GetName());
  CUART::ShowDevice(ofs);
}

