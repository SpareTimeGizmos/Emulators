//++
// ElfDisk.hpp -> "Standard" Elf IDE disk emulation
//
// LICENSE:
//    This file is part of the SBC1802 emulator project.  SBC1802 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBC1802 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBC1802.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This module emulates the hardware for the "standard" Elf to IDE disk 
// interface.  Two CDP1802 I/O ports are used - one for the IDE status and
// register selection, and the other for 8 bit data transfer.  This same
// interface, more or less, is used by the ELF2K, SBC1802, as well as most
// of Mike's and David's implementations.
// 
//   This standard IDE interface implements two ports, a "select" port and a
// data port.  The ports are assigned two consectutive addresses with the select
// port being first and the data port second.  The select port is a 4 bit, write
// only register.  The lower three bits select the IDE register, and bit 3 
// selects the CS1FX (bit 3 == 0) or CS3FX (bit 3 == 1) IDE address space.  The
// upper 4 bits of this port are not implemented, nor is reading back from the
// select port.
// 
//   The data port is at the device base address plus 1, and is a bidirectional
// eight bit port that transfers data to or from the IDE drive.  Note that the
// normal IDE interface is sixteen bits wide and eight bit mode is a special 
// (and optional!) drive feature.  All CF cards in IDE/ATA mode implement eight
// bit mode because the CFA specification requires it, however may actual spinning
// hard drives do not.  Exactly which drive register is accessed by the data
// port depends on the register selected by the select port.
// 
//   In the SBC1802 IDE disk attention interrupts are also implemented.
//
// REVISION HISTORY:
// 20-JAN-20  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // basic memory emulation declarations ...
#include "CPU.hpp"              // CCPU base class definitions
#include "Device.hpp"           // basic I/O device emulation declarations ...
#include "IDE.hpp"              // IDE/ATA disk emulation
#include "ElfDisk.hpp"          // declarations for this module


word_t CElfDisk::DevRead (address_t nPort)
{
  //++
  //   Reading the IDE data register is easy (well, it is for us - the CIDE
  // class does all the real work) and reading the select register is not
  // implemented.  In the SBC1802 the bus just floats if you try the latter.
  //--
  assert((nPort >= GetBasePort())  &&  ((nPort-GetBasePort()) < IDEPORTS));
  switch (nPort - GetBasePort()) {
    case DATA_PORT:  return CIDE::DevRead(m_bSelect & 0x0F);
    default:         return 0xFF;
  }
}

void CElfDisk::DevWrite (address_t nPort, word_t bData)
{
  //++
  //  Writing to the IDE is no harder ...
  //--
  assert((nPort >= GetBasePort())  &&  ((nPort-GetBasePort()) < IDEPORTS));
  switch (nPort - GetBasePort()) {
    case SELECT_PORT:  m_bSelect = bData;                         break;
    case DATA_PORT:    CIDE::DevWrite((m_bSelect & 0x0F), bData); break;
  }
}

void CElfDisk::ShowDevice (ostringstream &ofs) const
{
  //++
  //   This routine will dump the state of the internal IDE registers.
  // This is used by the UI EXAMINE command ...
  //--
  ofs << FormatString("Last register selected 0x%02X\n", m_bSelect);
  CIDE::ShowDevice(ofs);
}