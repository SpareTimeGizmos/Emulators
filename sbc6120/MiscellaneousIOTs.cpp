//++
// MiscellaneousIOTs.hpp -> SBC6120 "miscellaneous" IOT implementation
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
//   This file implements a class for the SBC6120 641x IOTs, which perform
// miscellaneous functions - skip on battery low, load disk address register,
// select SLU secondary mode, etc.
// 
//   It also contains another class which implements the IOTs for the FP6120
// front panel.  That's not emulated here and the SBC6120 firmware doesn't
// require that it be present, but we still need to define all the associated
// IOTs as NOPs.
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
#include "HD6120opcodes.hpp"    // standard opcode definitions
#include "Device.hpp"           // basic I/O device emulation declarations
#include "CPU.hpp"              // generic CPU declarations
#include "HD6120.hpp"           // HD6120 specific declarations
#include "SLU.hpp"              // serial port declarations
#include "RAMdisk.hpp"          // RAM disk declarations
#include "IDEdisk.hpp"          // IDE disk declarations
#include "MiscellaneousIOTs.hpp"// definitions for this module


CIOT641x::CIOT641x (word_t nIOT, CSLU *pSLU, CRAMdisk *pRAMdisk, CIDEdisk *pIDEdisk)
  : CDevice("SBC6120", "SBC6120", "Miscellaneous IOTs", INOUT, nIOT)
{
  //++
  //--
  assert((pSLU != NULL) && (pIDEdisk != NULL) && (pIDEdisk != NULL));
  m_pSLU = pSLU;  m_pIDEdisk = pIDEdisk;  m_pRAMdisk = pRAMdisk;
}

bool CIOT641x::DevIOT (word_t wIR, word_t &wAC, word_t &wPC)
{
  //++
  //   The SBC6120 has several IOTs that perform miscellaneous, unrelated,
  // functions and these were all grouped together with one device IOT code.
  // That made the hardware design easier, but it makes this C++ version a
  // little less clean.
  //--
  switch (wIR & 7) {

    case (OP_LDAR & 7):
      // Load the RAM disk address register and clear the AC ...
      m_pRAMdisk->LoadDiskAddress(wAC);  wAC = 0;  return true;

    case (OP_SDASP & 7):
      // Skip on IDE drive active ...
      if (m_pIDEdisk->IsDASP()) wPC = INC12(wPC);
      return true;

    case (OP_PRISLU & 7):
    case (OP_SECSLU & 7):
      //   Select primary or secondary IOT assignments for the SLU.  Note that
      // on early versions of the SBC6120 these IOTs didn't exist and were
      // NOPs.  On later versions these IOTs exist AND they clear the AC.  The
      // firmware uses this AC clearing to determine which version of the
      // hardware we have!
      /*wAC = 0;*/  return true;

    case (OP_SBBLO & 7):
      // Skip on RAM disk backup battery low ...
      if (m_pRAMdisk->IsBatteryLow()) wPC = INC12(wPC);
      return true;

    default:
      // Everything else is unimplemented ...
      return false;
  }
}

void CIOT641x::ShowDevice (ostringstream &ofs) const
{
  //++
  // Show the status for debugging ...
  //--
  ofs << FormatString("Disk Address Register=%04o", m_pRAMdisk->GetDiskAddress());
  ofs << FormatString(", backup battery %s", m_pRAMdisk->IsBatteryLow() ? "FAIL" : "OK");
  ofs << std::endl;
  ofs << FormatString("IDE drive %s, ", m_pIDEdisk->IsDASP() ? "BUSY" : "IDLE");
  ofs << FormatString("SLU PRIMARY (secondary not implemented)");
  ofs << std::endl;
}
