//++
// PPI11.cpp -> SBCT11 8255 PPI parallel port emulation
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
// 
// LICENSE:
//    This file is part of the SBCT11 emulator project.  SBCT11 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBCT11 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBCT11.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This class implements the 8255 PPI as it is used in the SBCT11.  Port A
// and the handshaking bits of port C are used to implement a general purpose
// bidirectional 8 bit parallel port, which could possibly be Centronics
// printer compatible.  The remaining bits in port B are used to control the
// POST display and the RUN LED.
//
// CENTRONICS PARALLEL PORT SIGNALS
//    port A is in mode 1, strobed output
//    PA0..7      -> D0..7
//    STBA (PC4)  -> BUSY
//    IBFA (PC5)  -> AUTO LF
//    ACKA (PC6)  -> ACK
//    OBFA (PC7)  -> STROBE
//    PC0         <- PAPER END
//    PC1         <- SELECT
//    PC2         <- ERROR
//    PB7         -> INIT
//    PB6         -> SELECT IN
// 
// OTHER SIGNALS
//    INTRA (PC3) -> PPI IRQ
//    PB5         -> unused
//    PB4         -> RUN LED
//    PB0..3      -> POST CODE
//
// REVISION HISTORY:
//  7-JUL-22  RLA   New file.
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
#include "Interrupt.hpp"        // CInterrupt definitions
#include "EventQueue.hpp"       // CEventQueue definitions
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "i8255-original.hpp"   // generic PPI base class
#include "PPI11.hpp"            // declarations for this module


CPPI11::CPPI11 (const char *pszName, address_t nBase, CEventQueue *pEvents)
  : C8255 (pszName, nBase, 2*C8255::REG_COUNT, pEvents)
{
  //++
  //--
  m_bPOST = 0;
}

void CPPI11::ClearDevice()
{
  //++
  //--
  C8255::ClearDevice();
}

uint8_t CPPI11::DevRead (address_t nPort)
{
  //++
  //   There's a small problem - the 8255 is an 8 bit device, however in the
  // SBCT11 the LSB of the address is ignored.  This maps the 8255 registers
  // to even addresses, which is much more convenient for the PDP11, but we
  // have to divide the register offset by two to get the real 8255 register.
  // 
  //   However, remember that the SBCT11 has a 16 bit data bus but here we're
  // emulating an 8 bit bus.  On the SBCT11 reading the upper byte from an
  // eight bit chip always returns zeros...
  //--
  assert(nPort >= GetBasePort());
  address_t nRegister = nPort-GetBasePort();
  assert(nRegister < C8255::REG_COUNT*2);
  if (ISODD(nRegister)) return 0;
  return C8255::DevRead(GetBasePort() + nRegister/2);
}

void CPPI11::DevWrite (address_t nPort, uint8_t bData)
{
  //++
  //   This handles the same situation as Read() - we need to cut the register
  // offset in half before calling the C8255::Write() method.  But once again,
  // remember that the real SBCT11 has a 16 bit data bus.  The upper byte is
  // ignored when writing to an 8 bit peripheral chip.
  //--
  assert(nPort >= GetBasePort());
  address_t nRegister = nPort-GetBasePort();
  assert(nRegister < C8255::REG_COUNT*2);
  if (ISODD(nRegister)) return;
  C8255::DevWrite(GetBasePort() + nRegister/2, bData);
}

void CPPI11::OutputB (uint8_t bData)
{
  //++
  //--
  if (bData == 0) return;
  m_bPOST = bData & 0x0F;
  LOGF(DEBUG, "POST %1X", m_bPOST);
  //if (m_bPOST == 0x16) LOGF(WARNING, "AUTOBAUD NOW");
}

void CPPI11::ShowDevice (ostringstream &ofs) const
{
  //++
  // Show the parallel port state for debugging ...
  //--
  ofs << FormatString("SBCT11 POST=%1X\n", m_bPOST);
  C8255::ShowDevice(ofs);
}
