//++
// ElfDisk.hpp -> "Standard" Elf IDE disk emulation
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
//    This emulates the "standard" Elf IDE disk interface, as used in the PicoElf,
// the Elf2K (as part of the Disk/UART/RTC card) and the SBC1802.
//
// REVISION HISTORY:
// 20-JAN-20  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Device.hpp"           // generic device definitions
#include "IDE.hpp"              // IDE disk emulation
using std::string;              // ...

class CElfDisk : public CIDE {
  //++
  // Standard Elf disk emulation ...
  //--

  // Magic constants ...
public:
  enum {
    IDEPORTS            = 2,    // total number of ports required
    SELECT_PORT         = 0,    // IDE register selection port
    DATA_PORT           = 1,    // IDE data port
  };

  // Constructor and destructor ...
public:
  CElfDisk (uint8_t nPort, CEventQueue *pEvents)
    : CIDE("DISK", "IDE", "IDE Disk Interface", nPort, IDEPORTS, pEvents)
    {m_bSelect=0;}
  virtual ~CElfDisk() {};
private:
  // Disallow copy and assignments!
  CElfDisk (const CElfDisk &) = delete;
  CElfDisk& operator= (CElfDisk const &) = delete;

  // CElfDisk device methods ...
public:
  virtual void ClearDevice() override {m_bSelect = 0;  CIDE::ClearDevice();}
  virtual word_t DevRead (address_t nPort) override;
  virtual void DevWrite (address_t nPort, word_t bData) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Private member data...
protected:
  uint8_t    m_bSelect;         // last value written to the select register
};
