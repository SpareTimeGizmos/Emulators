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
// REVISION HISTORY:
// 29-OCT-24  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
//#include "EMULIB.hpp"           // emulator library definitions
//#include "MemoryTypes.h"        // address_t and word_t data types
//#include "EventQueue.hpp"       // CEventQueue declarations
#include "Device.hpp"           // generic device definitions
//#include "PSG.hpp"              // single AY-3-8912 PSG emulation
using std::string;              // ...


class CTwoPSGs : public CDevice
{
  //++
  // Special SBC1802 emulation for two PSG chips ...
  //--

  // Constructor and destructor ...
public:
  CTwoPSGs (class CPSG *pPSG1, class CPSG *pPSG2, class CEventQueue *pEvents);
  virtual ~CTwoPSGs() {};
private:
  // Disallow copy and assignments!
  CTwoPSGs (const CTwoPSGs&) = delete;
  CTwoPSGs& operator= (CTwoPSGs const&) = delete;

  // CPSG device methods from CDevice ...
public:
  virtual void ClearDevice() override;
  virtual void EventCallback(intptr_t lParam) override {};
  virtual word_t DevRead (address_t nRegister) override;
  virtual void DevWrite (address_t nRegister, word_t bData) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Private methods ...
protected:

  // Private member data...
protected:
  class CPSG *m_pPSG1;        // pointer to the first PSG chip 
  class CPSG *m_pPSG2;        //  ... and the second
  address_t   m_nPSG1base;    // base port address for PSG1
  address_t   m_nPSG2base;    //  "    "    "   "   "  PSG2
  address_t   m_nLastN;       // last I/O port address
};
