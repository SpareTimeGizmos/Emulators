//++
// Printer.cpp -> SBC1802 specific parallel port printer emulation
//
//   COPYRIGHT (C) 2015-2025 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
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
//    The SBC1802 can connect to a traditional Centronics interface style
// parallel port printer using the CDP1851 programmable peripheral interface.
// In fact, the DB25 connector used for the CDP1851 interface is wired up to
// make this "just work" albeit not in the most optimal way.  The SBC1802
// firmware contains some routines to handshake with the parallel printer and
// print basic text.
// 
//    This class wraps around the CDP1851 PPI class and simulates the SBC1802
// style printer interface.  The CDP1851 bits and pins are wired up to the
// actual printer as follows -
// 
//	PPI PIN	PPI DIR	CENTRONICS SIGNAL   ACTIVE    NOTES
//	------	------- ------------------- ------    -------------------------
//	PA0..7	output	data 0..7           HIGH
//	ARDY	output	STROBE              LOW       inverted on SBC1802!
//	ASTB	input	ACK                 LOW       inverted on SBC1802!
//	BRDY	output	AUTO LF             LOW
//	BSTB	input	BUSY                HIGH
//	PB0	output	INIT                LOW
//	PB1	input	SELECT IN           HIGH
//	PB2	output	SELECT OUT          LOW
//	PB3	input	ERROR               LOW
//	PB4	input	PAPER OUT           HIGH
//	PB5..7	input	unused
//
//   In addition to simulating the printer handshake, the CPrinter object can
// be attached to a file.  All output sent to the printer will then be captured
// in that file, verbatim!
//  
// REVISION HISTORY:
// 26-MAR-25  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <ctype.h>              // toupper() ...
#include <string>               // C++ std::string class, et al ...
#include <cstring>              // needed for memset()
#include <iostream>             // C++ style output for LOGS() ...
#include <sstream>              // C++ std::stringstream, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "CPU.hpp"              // CPU definitions
#include "SBC1802.hpp"          // global declarations for this project
#include "COSMAC.hpp"           // CDP1802 COSMAC specific defintions
#include "Device.hpp"           // generic device definitions
#include "PPI.hpp"              // generic parallel interface definitions
#include "CDP1851.hpp"          // RCA CDP1851 specific PPI emulation
#include "Printer.hpp"          // declarations for this module

CPrinter::CPrinter (const char *pszName, address_t nPort, CEventQueue *pEvents)
  : CCDP1851(pszName, nPort, pEvents, PPI_ARDY_EF, PPI_BRDY_EF, PPI_IRQ_EF, PPI_IRQ_EF)
{
  //++
  //--
  m_llBusyDelay = CPSTONS(DEFAULT_SPEED);
  m_lLineWidth = DEFAULT_WIDTH;
  ClearDevice();
}

void CPrinter::ClearDevice()
{
  //++
  //--
  CCDP1851::ClearDevice();
  m_bCurrentStatus = m_bLastControl = 0;
  m_lCurrentColumn = 0;  m_bDataBuffer = 0;
  CancelEvent(EVENT_BUSY_DELAY);
  CancelEvent(EVENT_ACK_DELAY);
  if (IsAttached()) NewLine();
  UpdateStatus();
}

void CPrinter::NewLine (bool fLF)
{
  //++
  //   Output a carriage return to the printer and reset the current column
  // to zero.  If fLF is true, then output a line feed too.
  //--
  assert(IsAttached());
  if (m_lCurrentColumn > 0) {
    m_PrinterFile.Write(CR);  m_lCurrentColumn = 0;
  }
  if (fLF) m_PrinterFile.Write(LF);
}

void CPrinter::Print (uint8_t ch)
{
  //++
  //   Write a character to the printer file, handling line wrap around at the
  // right margin.  If the character is either a <CR> or an <LF> then start
  // a new line.
  //--
  if ((ch == CR) || (ch == LF)) {
    NewLine(/*IsAutoLF()*/);
  } else {
    ++m_lCurrentColumn;
    if ((m_lLineWidth > 0) && (m_lCurrentColumn > m_lLineWidth)) NewLine();
    m_PrinterFile.Write(ch);
  }
}

uint8_t CPrinter::UpdateStatus()
{
  //++
  //   Update the current printer status in m_bCurrentStatus, and also update
  // ACK and BUSY signals too (which are wired to the PPI handshaking pins).
  //--
  m_bCurrentStatus &= ~STS_MASK_READ;
  if (IsSelectOut()) SETBIT(m_bCurrentStatus, STS_SELECT_IN);
  if (!IsAttached()) SETBIT(m_bCurrentStatus, STS_PAPER_OUT);
  SETBIT(m_bCurrentStatus, STS_ERROR);
  UpdateStrobeA(IsAcknowledge());
  UpdateStrobeB(IsBusy());
  return m_bCurrentStatus & STS_MASK_READ;
}

void CPrinter::UpdateControl (uint8_t bNew)
{
  //++
  //   This routine is called when the software writes to PPI port B, and it
  // updates the printer control signals driven by that port.
  //--
  if (!IsInitialize() && !ISSET(bNew, CTL_INIT)) {
    // Rising edge on INIT ... do what???
    LOGF(DEBUG, "Printer initialized");
  }
  m_bLastControl = (bNew & CTL_WRITE_MASK) | (m_bLastControl & ~CTL_WRITE_MASK);
  UpdateStatus();
}

void CPrinter::SetStrobe (bool fSet)
{
  //++
  //   This method is called when the firmware updates the PPI READY A output.
  // This drives the printer STROBE signal, and that latches (and prints!) new
  // data on the trailing edige.
  // 
  //    Note that in the real hardware STROBE is active low, however the
  // hardware takes care of that for us.  We're looking for STROBE==1 as the
  // active state!
  //--

  // Look for the trailing edge of the STROBE pulse ...
  if (IsStrobe() && !fSet) {
    if (IsBusy())
      LOGF(WARNING, "printer STROBE while still busy");
    Print(m_bDataBuffer);  SetBusy();  UpdateStatus();
    LOGF(DEBUG, "printer prints 0x%02X", m_bDataBuffer);
  }

  // Remember the last statte of STROBE ...
  if (fSet)
    SETBIT(m_bLastControl, CTL_STROBE);
  else
    CLRBIT(m_bLastControl, CTL_STROBE);

  // Update the BUSY status in the status register ...
  UpdateStatus();
}

void CPrinter::SetBusy (bool fSet)
{
  //++
  //    This routine will mark the printer as "busy" and assert the printer
  // BUSY signal.  On the transition from "not busy" to "busy" we also schedule
  // an event for the printer to finish.
  //--
  if (fSet && !IsBusy())
    ScheduleEvent(EVENT_BUSY_DELAY, m_llBusyDelay);

  // Remember the last state of the BUSY signal ...
  if (fSet)
    SETBIT(m_bCurrentStatus, STS_BUSY);
  else
    CLRBIT(m_bCurrentStatus, STS_BUSY);

  // Update the status register to show the state of BUSY ...
  UpdateStatus();
}

void CPrinter::SetAcknowledge (bool fSet)
{
  //++
  //   And this routine will assert the printer ACKnowledge signal.  This
  // is set just before the end of the printer BUSY cycle ...
  //--
  if (fSet && !IsAcknowledge())
    ScheduleEvent(EVENT_ACK_DELAY, ACK_PULSE_WIDTH);
  if (fSet)
    SETBIT(m_bCurrentStatus, STS_ACK);
  else
    CLRBIT(m_bCurrentStatus, STS_ACK);
  UpdateStatus();
}

void CPrinter::EventCallback (intptr_t lParam)
{
  //++
  //   The event callback for the printer handles the BUSY delay and the ACK
  // DELAY events, and everything else gets passed down the chain to the
  // underlying CDP1851 and PPI objects.
  //--
  switch (lParam) {
    case EVENT_BUSY_DELAY:
      // Issue a short ACK pulse before finishing ...
      SetAcknowledge();  break;
    case EVENT_ACK_DELAY:
      // Clear both ACK and BUSY ...
      SetAcknowledge(false);  SetBusy(false);  break;
    default:
      // This event wasn't for us - pass it down the chain ...
      CCDP1851::EventCallback(lParam);
      break;
  }
}

void CPrinter::ShowDevice (ostringstream &ofs) const
{
  //++
  //  Show the status of the CDP1851 and then this printer specifially ...
  //--
  CCDP1851::ShowDevice(ofs);
  ofs << std::endl;
  ofs << FormatString("PRINTER STATUS\n");
  ofs << FormatString("Strobe=%d, Busy=%d, Ack=%d, Selected=%d, AutoLF=%d\n",
    IsStrobe(), IsBusy(), IsAcknowledge(), IsSelectOut(), IsAutoLF());
  ofs << FormatString("Width=%d, Column=%d, Buffer=0x%02X, Speed=%ld cps\n",
    m_lLineWidth, m_lCurrentColumn, m_bDataBuffer, NSTOCPS(m_llBusyDelay));
}
