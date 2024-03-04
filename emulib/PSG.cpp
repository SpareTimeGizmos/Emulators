//++
// PSG.cpp -> AY-3-891x Programmable Sound Generator emulation
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
//   This module implements a simple emulation of the General Instruments
// ANY-3-891x programmable sound generator chips.  There are several chips in
// this family - the 8910, 8912 and 8913 being most popular - and that are all
// essentially the same as far as the programmer interface goes.  These chips
// were the staple of early 80s personal computers, video games, arcade games
// and even slot machines.
// 
//   This current implementation is trivial and does just enough to pass the
// POST in the SBC1802 firmware.  It doesn't do any sound generation at all!
// We're fortunate because even though all 16 of the PSG's internal registers
// are read/write, reading them only reads back what was last written there
// (with the exception of the two I/O registers).  There are no status bits,
// flags, or anything else that changes state during the operation of the PSG.
// 
//   The two I/O registers, ports A and B, are a slight exception.  Each I/O
// port can be programmed as either input or output (and the setting is port
// wide, not bit-by-bit) by bits in R7.  Reading an output port does read back
// whatever was last written there, but reading an input port reads the current
// state of the pins.  AFAIK, writing to an input port is ignored.
// 
// AMBITIONS:
//    The simulator already keeps track of the exact simulated time, and from
// that it should be possible to figure out how many cycles each PSG oscillator
// has generated and also the volume modulation produced by the envelope
// generator.  And if you can do that, then you should be able to figure out the
// exact waveforms generated and output those to a WAV file.  AY-3-891x simulations
// that do this have already been done several times, possibly by people smarter
// than me.  Just Google and you'll find them.
//    
// REVISION HISTORY:
// 27-FEB-24  RLA   New file.
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
#include "Device.hpp"           // generic device definitions
#include "PSG.hpp"              // declarations for this module


CPSG::CPSG (const char *pszName, const char *pszType, const char *pszDescription, address_t nPort, address_t nPorts, CEventQueue *pEvents)
  : CDevice(pszName, pszType, pszDescription, INOUT, nPort, nPorts, pEvents)
{
  //++
  //--
  memset(m_abRegisters, 0, sizeof(m_abRegisters));
  m_bAddress = 0;
}

CPSG::~CPSG()
{
  //++
  //--
}

void CPSG::ClearDevice()
{
  //++
  //--
  memset(m_abRegisters, 0, sizeof(m_abRegisters));
  m_bAddress = 0;
}

word_t CPSG::DevRead (address_t nRegister)
{
  //++
  //   Read a PSG register.  Remember that, as far as the CPU knows, the
  // PSG has only two I/O ports - an address register and a data register.
  // Only the data register is read/write; attempting to read the address
  // just returns $FF.  Reading the data port returns the contents of
  // the last selected internal PSG register.
  // 
  //   All PSG registers just read back whatever was last written to them,
  // EXCEPT for R16 and R17 which are the I/O port A and B.  Those also
  // read back whatever was last written, UNLESS they're programmed as
  // inputs in which case they read $FF.
  //--
  assert((nRegister >= GetBasePort()) && ((nRegister-GetBasePort()) < MAXPORT));
  nRegister = (nRegister-GetBasePort());
  if (nRegister == 0) {
    // Read the address register ...
    return 0xFF;
  } else {
    // Read from an internal register ...
    assert(m_bAddress <= MAXREG);
    if (m_bAddress <= R15)
      return m_abRegisters[m_bAddress];
    else if (m_bAddress == R16)
      return IsOutputA() ? m_abRegisters[R16] : 0xFF;
    else
      return IsOutputB() ? m_abRegisters[R17] : 0xFF;
  }
}

void CPSG::DevWrite (address_t nRegister, word_t bData)
{
  //++
  //   Write to a PSG register ...
  //--
  assert((nRegister >= GetBasePort()) && ((nRegister-GetBasePort()) < MAXPORT));
  nRegister = (nRegister-GetBasePort());
  if (nRegister == 0) {
    // Write the address register ...
    m_bAddress = LOBYTE(bData);
  } else {
    // Write to an internal register ...
    assert(m_bAddress <= MAXREG);
    m_abRegisters[m_bAddress] = LOBYTE(bData);
  }
}

void CPSG::ShowDevice (ostringstream &ofs) const
{
  //++
  //   This routine will dump the state of the internal PSG registers.
  // This is used by the user interface SHOW DEVICE command ...
  //--
  ofs << FormatString("LastAddress = 03o", m_bAddress) << std::endl;
  for (uint8_t i = 0; i < MAXREG; ++i) {
    ofs << FormatString("R%02o=0x%02X ", i, m_abRegisters[i]);
    if ((i%8) == 7) ofs << std::endl;
  }
}
