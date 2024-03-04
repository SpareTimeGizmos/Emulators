//++
// Baud.hpp -> SBC1802 COM8136 Baud Rate Generator emulation
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
//   This class emulates the COM8136 baud rate generator chip used on the SBC1802.
// "Emulates" might be a little much in this case - the CDP1854 UART emulations
// don't care about any baud rate and this object pretty much does nothing.  We
// need something assigned to this port, however, so that the firmware can write
// to it even if nobody cares.
//
// REVISION HISTORY:
// 23-JUN-22  RLA   New file.
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
#include "SBC1802.hpp"          // global declarations for this project
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "Memory.hpp"           // basic memory emulation declarations ...
#include "Device.hpp"           // basic I/O device emulation declarations ...
#include "CPU.hpp"              // CCPU base class definitions
#include "Baud.hpp"             // declarations for this module


CBaud::CBaud (address_t nPort)
  : CDevice("BAUD", "COM8136", "Baud Rate Generator", OUTPUT, nPort)
{
  //++
  //--
  m_bBaud0 = m_bBaud1 = 0;
}

string CBaud::DecodeBaud (uint8_t bBaud)
{
  //++
  // Convert the baud rate to a string for debugging ...
  //--
  switch (bBaud) {
    case BAUD_50:       return("50 bps");
    case BAUD_75:       return("75 bps");
    case BAUD_110:      return("110 bps");
    case BAUD_1345:     return("134.5 bps");
    case BAUD_150:      return("150 bps");
    case BAUD_300:      return("300 bps");
    case BAUD_600:      return("600 bps");
    case BAUD_1200:     return("1,200 bps");
    case BAUD_1800:     return("1,800 bps");
    case BAUD_2000:     return("2,000 bps");
    case BAUD_2400:     return("2,400 bps");
    case BAUD_3600:     return("3,600 bps");
    case BAUD_4800:     return("4,800 bps");
    case BAUD_7200:     return("7,200 bps");
    case BAUD_9600:     return("9,600 bps");
    case BAUD_19200:    return("19.2 kbps");
    default: return "";
  }
}

void CBaud::DevWrite (address_t nPort, word_t bData)
{
  //++
  //--
  assert(nPort == GetBasePort());
  m_bBaud0 =  bData       & 0x0F;
  m_bBaud1 = (bData >> 4) & 0x0F;
  LOGF(TRACE, "Write baud SLU0=%s, SLU1=%s", DecodeBaud(m_bBaud0).c_str(), DecodeBaud(m_bBaud1).c_str());
}

void CBaud::ShowDevice (ostringstream &ofs) const
{
  //++
  // Dump the device state for the "SHOW DEVICE BAUD" command ...
  //--
  ofs << FormatString("SLU0 baud %s, SLU1 %s\n", DecodeBaud(m_bBaud0).c_str(), DecodeBaud(m_bBaud1).c_str());
}

