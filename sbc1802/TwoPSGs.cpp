//++
// TwoPSGs.hpp -> SBC1802 Specific Dual PSG Emulation
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
//   The SBC1802 has two AY-3-8192 programmable sound generator chips and,
// because of the way the PSG addressing works, they require some unique
// handling to coordinate the two chips.
//
//   The 8912 has an A8 address input which must be 1 to select the chip.In
// the SBC1802, which has two 8912 PSG chips, A8 is wired up to NOT N2 for
// PSG#1, and N2 for PSG#2.  The funky thing about the AY-3-891x chips is that
// this A8 input is LATCHED WHEN THE REGISTER ADDRESS IS LOADED!  A8 is totally
// ignored when reading or writing the PSG data port.  This means that the
// following sequence doesn't work as you might expect -
//
//	SEX PC
//	OUT PSG1ADR\.BYTE PSGR17
//	OUT PSG2ADR\.BYTE PSGR16
//	OUT PSG1DATA\.BYTE $55
//	OUT PSG2DATA\.BYTE $AA
//
//   In this case both writes to the PSG data port will write to PSG#2 because
// it was the last address loaded.  PSG#1 will be unaffected by this sequence!
// The correct way would be to do this instead -
//
//	OUT PSG1ADR\.BYTE PSGR17
//	OUT PSGDATA\.BYTE $55
//	OUT PSG2ADR\.BYTE PSGR16
//	OUT PSGDATA\.BYTE $AA
//
//  The bottom line is that you should always load the PSG address register
// before reading or writing the data port.  This also why there are two 1802
// PSG I/O addresses defined, but only one PSG data port.Awkward, but we can
// program around it...
//
// REVISION HISTORY:
// 29-OCT-24  RLA   New file.
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
#include "COSMAC.hpp"           // CDP1802 COSMAC specific defintions
#include "Device.hpp"           // generic device definitions
#include "PSG.hpp"              // programmable sound generator
#include "TwoPSGs.hpp"          // declarations for this module


CTwoPSGs::CTwoPSGs (CPSG *pPSG1, CPSG *pPSG2, CEventQueue *pEvents)
  : CDevice("TwoPSGs", "TwoPSGs", "Two PSGs", INOUT, 1, CCOSMAC::MAXDEVICE, pEvents)
{
  //++
  //--
  assert((pPSG1 != NULL) && (pPSG2 != NULL));
  m_pPSG1 = pPSG1;  m_pPSG2 = pPSG2;
  m_nPSG1base = pPSG1->GetBasePort();
  m_nPSG2base = pPSG2->GetBasePort();
  assert((m_nPSG1base^m_nPSG2base) == 4);
  m_nLastN = 0;
}

void CTwoPSGs::ClearDevice()
{
  //++
  // Reset both PSGs ...
  //--
  m_pPSG1->ClearDevice();
  m_pPSG2->ClearDevice();
}

word_t CTwoPSGs::DevRead (address_t nRegister)
{
  //++
  //   Read a PSG register from the last selected PSG device ...  Note that the
  // address register is write only (so you can't read it back!), and reading
  // from any other address just returns all ones...
  //--
  if (ISODD(nRegister)) {
    if ((m_nLastN & 4) == 0)
      return m_pPSG1->DevRead(m_nPSG1base+1);
    else
      return m_pPSG2->DevRead(m_nPSG2base+1);
  }
  return 0xFF;
}

void CTwoPSGs::DevWrite (address_t nRegister, word_t bData)
{
  //++
  //   Write to a PSG register ...   Writing to the address register actually
  // loads the address in BOTH PSGs, but writing to the data register writes
  // to the last selected PSG ONLY!
  //--
  if (!ISODD(nRegister)) {
    // Write the address register of BOTH PSGs ...
    m_pPSG1->DevWrite(m_nPSG1base, bData);
    m_pPSG2->DevWrite(m_nPSG2base, bData);
    m_nLastN = nRegister;
  } else {
    // Write data to the last selected PSG ...
    if ((m_nLastN & 4) == 0)
      m_pPSG1->DevWrite(m_nPSG1base+1, bData);
    else
      m_pPSG2->DevWrite(m_nPSG2base+1, bData);
  }
}

void CTwoPSGs::ShowDevice (ostringstream &ofs) const
{
  //++
  //   Dump the state of the internal PSG registers.  This function
  // never gets called - the UI will call the PSG1 or PSG2 ShowDevice()
  // routines directly!
  //--
}
