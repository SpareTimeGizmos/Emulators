//++
// Switches.hpp -> TIL311 Hexadecimal Switches Emulation
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the ELF2K emulator project.  EMULIB is free
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
//   This class emulates the TIL311 hexadecimal data display used on the ELF.
// This is output device #4 and is used by the Elf2K EPROM to display POST
// codes.
//
// REVISION HISTORY:
// 18-JAN-20  RLA   New file.
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
#include "ELF2K.hpp"            // global declarations for this project
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // basic memory emulation declarations ...
#include "Device.hpp"           // basic I/O device emulation declarations ...
#include "CPU.hpp"              // CCPU base class definitions
#include "Switches.hpp"          // declarations for this module


CSwitches::CSwitches (address_t nPort)
  : CDevice("SWITCHES", "SWITCHES", "Toggle Switches", INPUT, nPort)
{
  //++
  //--
  m_bSwitches = 0;
}

word_t CSwitches::DevRead (address_t nPort)
{
  //++
  //--
  assert(nPort == GetBasePort());
  return m_bSwitches;
}

void CSwitches::ShowDevice (ostringstream &ofs) const
{
  //++
  // Dump the device state for the UI command "EXAMINE DISPLAY" ...
  //--
  ofs << FormatString("SWITCHES=0x%02X", m_bSwitches);
}
