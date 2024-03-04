//++
// UARTrtc.cpp -> PEV2 UART/RTC expansion board emulation
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the PicoElf emulator project.  EMULIB is free
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
//   This module is an emulation of the Spare Time Gizmos UART/RTC expansion
// board for the PEV2.  This card contains a UART (any one of the 8250, 16450
// or 16550 devices), and a non-volatile RAM and real time clock chip (a DS1287,
// DS12887 or DS12887A). 
// 
//   The UART/RTC card contains a latch, loaded by a write to the port address
// +0.  If bit D7 is a one, then the DS12887 chip is selected and the lower
// seven bits are the RTC/NVR address (the DS12887 has only 128 bytes!).  If
// D7 is a zero, then the lower three bits are the UART register address. If
// the UART is selected, bit D6 will reset the 8250 if it is a one.
//
//   The UART/RTC port address +1 is a data register.  Inputs or outputs from
// or to this port will read or write the device and register last selected
// by the control port.  In short, the operation is pretty simple - do an
// OUT 6 to select the chip and register you want, followed by either an INP 7
// or an OUT 7 to read or write that register.
// 
//   The UART/RTC card also contains an 8 bit output latch that drives 8 LEDs.
// This is selected by an OUT 4 instruction and is functionally the same as
// the TIL311 POST display.  This port is not emulated by this module!
//
// REVISION HISTORY:
// 28-JUL-22  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "ConsoleWindow.hpp"    // needed for CVirtualConsole
#include "PEV2.hpp"             // global declarations for this project
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // basic memory emulation declarations ...
#include "CPU.hpp"              // CCPU base class definitions
#include "Device.hpp"           // basic I/O device emulation declarations ...
#include "INS8250.hpp"          // UART emulation device
#include "DS12887.hpp"          // NVR/RTC emulation
#include "UARTrtc.hpp"          // declarations for this module


CUARTrtc::CUARTrtc (address_t nPort, CINS8250 *pUART, C12887 *pNVR)
  : CDevice("COMBO", "COMBO", "UART/RTC card", INOUT, nPort, 2, NULL)
{
  //++
  //--
  assert((pUART != NULL) && (pNVR != NULL));
  m_pUART = pUART;  m_pNVR = pNVR;
  ClearDevice();
}

void CUARTrtc::ClearDevice()
{
  //++
  // Clear the select register, AND clear all child devices...
  //--
  m_bControl = 0;
  m_pUART->ClearDevice();
  m_pNVR->ClearDevice();
}

void CUARTrtc::DevWrite (address_t nPort, word_t bData)
{
  //++
  //   Remember - writing to the port address +0 writes the control  register,
  // and writing to the port address +1 writes the selected chip.
  //--
  assert(nPort >= GetBasePort());
  switch (nPort - GetBasePort()) {
    case 0:
      //   Write the control register, BUT remember that if D7 is zero
      // (UART selected) and D6 is 1 then we reset the UART too!
      m_bControl = bData;
      //if ((m_bControl & 0xC0) == 0x40) m_pUART->ClearDevice();
      break;

    case 1:
      // Write to the subdevice selected by the control register!
      if (ISSET(m_bControl, 0x80))
        m_pNVR->DevWrite((m_bControl & 0x7F), bData);
      else
        m_pUART->DevWrite((m_bControl & 7), bData);
      break;

    default:
      // Should never get here!!
      break;
  }
}

word_t CUARTrtc::DevRead (address_t nPort)
{
  //++
  //   Read data from the selected chip at base address +1.  The UART/RTC card
  // has no status register of its own, and reading from the base addres +0
  // just returns garbage.
  //--
  assert(nPort >= GetBasePort());
  switch (nPort - GetBasePort()) {
    case 1:
      // Read from the subdevice selected by the control register!
      if (ISSET(m_bControl, 0x80))
        return m_pNVR->DevRead(m_bControl & 0x7F);
      else
        return m_pUART->DevRead(m_bControl & 7);

    case 0:
    default:
      // Otherwise return garbage ...
      return 0xFF;
  }
}


CDevice *CUARTrtc::FindDevice (string sName)
{
  //++
  //  Search the child devices for one with a name matching the one given.
  // Return a pointer to that device, or NULL if we don't find one ...
  //--
  if (m_pUART->GetName() == sName) return m_pUART;
  if (m_pNVR->GetName()  == sName) return m_pNVR;
  return NULL;
}

void CUARTrtc::ShowDevice (ostringstream &ofs) const
{
  //++
  //--
  ofs << FormatString("Last control=0x%02X", m_bControl);
}