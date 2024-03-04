//++
// POST.cpp -> SBC6120 three LED POST code display emulation
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
//   This module emulates the SBC6120 three LED POST code display.  This is
// accessed by the 644x IOT, where "x" is displayed on the LEDs.  It's not
// very hard and not very exciting.
//
// REVISION HISTORY:
// 21-AUG-22  RLA   New file.
//--
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
#include "Device.hpp"           // basic I/O device emulation declarations ...
#include "POST.hpp"             // declarations for this module


CPOST::CPOST (word_t nIOT)
  : CDevice("POST", "LEDS", "POST Code Display", OUTPUT, nIOT)
{
  //++
  //--
  m_bPOST = 0;
}

bool CPOST::DevIOT (word_t wIR, word_t &AC, word_t &wPC)
{
  //++
  // Update the POST code display ...
  //--
  m_bPOST = wIR & 7;
  LOGF(DEBUG, "POST=%d", m_bPOST);
  return true;
}

void CPOST::ShowDevice (ostringstream &ofs) const
{
  //++
  // Dump the device state for the UI command "SHOW DEVICE" ...
  //--
  ofs << FormatString("Last POST code=%d", m_bPOST);
}
