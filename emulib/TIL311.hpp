//++
// TIL311.hpp -> Hexadecimal Display Emulation
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
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
//   This class emulates the TIL311 hexadecimal data display used on the ELF.
// This is output device #4 and is used by the Elf2K EPROM to display POST
// codes.
//
// REVISION HISTORY:
// 18-JAN-20  RLA   New file.
//--
#pragma once
#include <stdint.h>
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
using std::string;              // ...

class CTIL311 : public CDevice {
  //++
  //--

public:
  // Constructor and destructor...
  CTIL311 (address_t nPort);
  virtual ~CTIL311() {};

  //   This is a pretty trivial device - the only method we need to implement
  // is DevWrite().  The base class "do nothing" implementations will suffice
  // for all the rest...
public:
  // Write to a device register ...
  void DevWrite (address_t nPort, word_t bData) override;
  // Get the last POST code displayed ...
  uint8_t GetPOST() const {return m_bPOST;}
  // Dump the device state for the user ...
  void ShowDevice (ostringstream &ofs) const override;

  // Private member data...
protected:
  uint8_t m_bPOST;              // last byte sent to the display
};

