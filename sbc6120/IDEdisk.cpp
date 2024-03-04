//++
// IDEdisk.cpp -> SBC6120 IDE disk interface emulation
//
//   Copyright (C) 1999-2022 by Spare Time Gizmos.  All rights reserved.
//
// LICENSE:
//    This file is part of the SBC6120 emulator project. SBC6120 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBC6120 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBC6120.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   In the SBC6120 the IDE interface is implemented by a standard 8255 PPI,
// which gives us 24 bits of general purpose parallel I/O.  PPI port A is
// connected the high byte (DD8..DD15) of the IDE data bus and port B is
// connected to the low byte (DD0..DD7).  Port C supplies IDE control signals
// according to the following table.
//
//      PPI       IDE Signal
//      --------  ----------------------------------------------------------
//      C0..C2    DA0..2 (IDE register select)
//      C3        DIOR (disk I/O read)
//      C4        DIOW (disk I/O write)
//      C5        RESET
//      C6        CS1Fx (Chip select for the 1Fx register space)
//      C7        CS3Fx (Chip select for the 3Fx register space)
//
// One nice feature of the 8255 is that it allows bits in port C to be set or
// reset individually simply by writing the correct command word to the control
// register - it's not necessary to read the port, do an AND or OR, and write
// it back. We can use this feature to easily toggle the DIOR and DIOW lines
// with a single PWCR IOT.
//
//   The HD6120 can access the 8255 PPI by standard IOTs:
//
//      IOT         Function
//      ---------   -------------------------------------------------
//      PRPA 6470   Read PPI Port A
//      PRPB 6471   Read PPI Port B
//      PRPC 6472   Read PPI Port C
//      PWPA 6474   Write PPI Port A and clear the AC
//      PWPB 6475   Write PPI Port B and clear the AC
//      PWPC 6476   Write PPI Port C and clear the AC
//      PWCR 6477   Write the PPI control register and clear the AC
//
// IMPLEMENTATION
//   We do have a perfectly good 8255 emulation module (look for the i8255.hpp
// and i8255.cpp files) which we could use here, but the SBC6120 firmware only
// ever uses ports A and B in simple input or simple output mode, and port C
// always in output mode.  It's really overkill to emulate all of the 8255
// functions, and we just fake the modes used by the SBC6120 firmware.  If we
// ever find any software that wants to use the SBC6120 IDE port as a general
// purpose 12 bit parallel I/O port, which is theoretically possible, then
// we'll have to revisit this decision.
// 
//   There's another kludge worth mentioning - in the real SBC6120 the IDE disk
// is used in 16 bit mode.  The 8255 ports A and B together make a 16 bit I/O
// port that the SBC6120 firmware reads and writes one byte at a time.  But,
// the CIDE class we have here was really written for 8 bit microprocessors,
// it doesn't support a 16 bit data register.  So we actually force the IDE
// emulator into 8 bit mode, even though the SBC6120 firmware never selects that.
// Then for every read or write of the data register, we actually transfer two
// bytes to the IDE emulator, one byte at a time.  The SBC6120 firmware never
// knows the difference.
// 
// REVISION HISTORY:
// 22-AUG-22  RLA   New file.
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
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "HD6120opcodes.hpp"    // PDP8/SBC6120 opcode mnemonics
#include "Memory.hpp"           // basic memory emulation declarations ...
#include "Device.hpp"           // basic I/O device emulation declarations ...
#include "CPU.hpp"              // CCPU base class definitions
#include "HD6120.hpp"           // HD6120 CPU specific implementation
#include "IDE.hpp"              // IDE/ATA disk emulation
#include "IDEdisk.hpp"          // declarations for this module


CIDEdisk::CIDEdisk (word_t nIOT, CEventQueue *pEvents)
  : CIDE("IDE", "IDE", "8255 IDE Interface", nIOT, 1, pEvents)
{
  //++
  //--
  CIDEdisk::ClearDevice();
}

void CIDEdisk::ClearDevice()
{
  //++
  // Reset both our virtual 8255 and also the attached IDE drive ...
  //--
  m_fInputMode = m_fDIOR = m_fDIOW = m_fRESET = false;
  m_bPortA = m_bPortB = m_bPortC = 0;
  CIDE::ClearDevice();
  CIDE::Set8BitMode();
}

address_t CIDEdisk::GetRegister() const
{
  //++
  // Decode the bits in port C and return the IDE register selected ...
  //--
  address_t wReg = m_bPortC & 7;
  if (ISSET(m_bPortC, IDE_CS3FX)) {
    wReg |= 8;
  } else if (ISSET(m_bPortC, IDE_CS1FX)) {
    // Do nothing ...
  } else {
    LOGS(WARNING, "IDE read/write without CS1FX or CS3FX");
  }
  return wReg;
}

void CIDEdisk::WriteRegister()
{
  //++
  //   Write a byte (or in the case of the data register, a 16 bit word) to
  // the selected IDE drive register.  The DA bits, CS1FX and CS3FX bits, in
  // the PPI port C select the IDE register, and ports A and B contain the
  // data to be written.  Port B is the least significant byte and port A is
  // the most.
  //--
  if (m_fRESET || m_fDIOR || m_fInputMode) {
    LOGS(WARNING, "IDE conflicting signals for DIOW");  return;
  }
  if (!IsAttached()) return;
  address_t wRegister = GetRegister();
  LOGF(TRACE, "IDE write register=0x%02X, data=0x%02x%02X", wRegister, m_bPortA, m_bPortB);
  CIDE::DevWrite(wRegister, m_bPortB);
  if (wRegister == 0) CIDE::DevWrite(0, m_bPortA);
}

void CIDEdisk::ReadRegister()
{
  //++
  //   Same as above, but this time read from an IDE register into the 8255
  // ports A and B.  For 8 bit registers only port B is used, but for the
  // data register the high byte is read into port A and the low into port B.
  //--
  if (m_fRESET || m_fDIOW || !m_fInputMode) {
    LOGS(WARNING, "IDE conflicting signals for DIOR");  return;
  }
  address_t wRegister = GetRegister();
  m_bPortB = LOBYTE(CIDE::DevRead(wRegister));
  if (wRegister == 0) m_bPortA = LOBYTE(CIDE::DevRead(0));
  LOGF(TRACE, "IDE read register=0x%02X, data=0x%02x%02X", wRegister, m_bPortA, m_bPortB);
}

bool CIDEdisk::WriteControl (uint8_t bControl)
{
  //++
  //   This method handles outputs to the 8255 control register.  This both
  // changes the mode and toggles port C bits individually.  The only modes
  // we support are simple, non-strobed I/O with either both A and B as input,
  // or both A and B as output.  The port C signals for RESET, DIOW and DIOR
  // drive those IDE signals directly and the SBC6120 firmware has to toggle
  // them explicitly.  We arrange to take the associated action on the rising
  // edge of each signal, but we make the effort to ensure that the firmware
  // is actually clearing those signals later.
  //--
  LOGF(TRACE, "IDE write control %03o", bControl);
  switch (bControl) {
    // Change the mode of ports A and B (port C is always output) ...
    case IDE_INPUT:  m_fInputMode = true;   break;
    case IDE_OUTPUT: m_fInputMode = false;  break;

    //   Set or clear the IDE drive RESET signal.  The simulated IDE drive is
    // actually reset on the rising edge of this signal, but nothing else can
    // happen (all other operations are ignored!) until RESET is deasserted.
    case IDE_SET_RESET:
      if (!m_fRESET) {
        LOGS(TRACE, "IDE disk RESET");
        CIDE::ClearDevice();  m_fRESET = true;
      }
      break;
    case IDE_CLR_RESET:
      m_fRESET = false;  break;

    // Disk registers are actually written on the rising edge of DIOW.
    case IDE_SET_DIOW:
      if (!m_fDIOW) WriteRegister();
      m_fDIOW = true;  break;
    case IDE_CLR_DIOW:
      m_fDIOW = false;  break;

    // Disk registers are actually read on the rising edge of DIOR.
    case IDE_SET_DIOR:
      if (!m_fDIOR) ReadRegister();
      m_fDIOR = true;  break;
    case IDE_CLR_DIOR:
      m_fDIOR = false;  break;

    // Everything else is unknown ...
    default:
      LOGF(WARNING, "IDE unknown control byte %03o", bControl);
      return false;
  }
  return true;
}

bool CIDEdisk::DevIOT (word_t wIR, word_t &wAC, word_t &wPC)
{
  //++
  //   Handle SBC6120 8255 PPI IOTs.  These are all pretty simple, but notice
  // that all read operations are jam transfers and all write operations clear
  // the AC.  Also note that IOT 6473 is unimplemented.
  // 
  //   Also note that if the IDE disk isn't attached to a file, then all writes
  // are ignored and all reads return all ones.  In the real hardware if no
  // drive is connected then the inputs would float, so returning all ones is
  // reasonable.  This behavior makes the SBC6120 firmware probe for attached
  // drives much faster.
  //--
  switch (wIR & 7) {
    case (OP_PRPA & 7):  wAC = IsAttached() ? m_bPortA : WORD_MAX;  break;
    case (OP_PRPB & 7):  wAC = IsAttached() ? m_bPortB : WORD_MAX;  break;
    case (OP_PRPC & 7):  wAC = IsAttached() ? m_bPortC : WORD_MAX;  break;
    case (OP_PWPA & 7):  m_bPortA = LOBYTE(wAC);  wAC = 0;  break;
    case (OP_PWPB & 7):  m_bPortB = LOBYTE(wAC);  wAC = 0;  break;
    case (OP_PWPC & 7):  m_bPortC = LOBYTE(wAC);  wAC = 0;  break;
    case (OP_PWCR & 7):
      // Write control register ...
      if (!WriteControl(LOBYTE(wAC))) return false;
      wAC = 0;  break;
    default:
      // Everything else is unimplemented ...
      return false;
  }
  return true;
}

void CIDEdisk::ShowDevice (ostringstream &ofs) const
{
  //++
  // Dump the device state for the UI command "SHOW DEVICE" ...
  //--
  ofs << FormatString("%s mode, DIOR %s, DIOW %s, RESET %s\n",
    m_fInputMode ? "INPUT" : "OUTPUT", m_fDIOR ? "TRUE" : "FALSE",
    m_fDIOW ? "TRUE" : "FALSE", m_fRESET ? "TRUE" : "FALSE");
  ofs << FormatString("Port A=%03o, port B=%03o, port C=%3o, selected register=%03o\n",
    m_bPortA, m_bPortB, m_bPortC, GetRegister());
  ofs << std::endl;
  CIDE::ShowDevice(ofs);
}
