//++
// Switches.hpp -> Switch Panel Emulation
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the ELF2K emulator project.  ELF2K is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    ELF2K is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with ELF2K.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This class emulates a panel of 8 toggle switches connected to an in;ut
// port.  Needless to say, it's pretty trivial ...
//
// REVISION HISTORY:
// 27-JAN-20  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
using std::string;              // ...

class CSwitches : public CDevice {
  //++
  //--

public:
  // Constructor and destructor...
  CSwitches (address_t nPort);
  virtual ~CSwitches() {};

  //   This is a pretty trivial device - the only method we need to implement
  // is DevRead().  The base class "do nothing" implementations will suffice
  // for all the rest...
public:
  // Read from a device register ...
  word_t DevRead (address_t nPort);
  // Set the switches (for the SET SWITCHES command ...) ...
  void SetSwitches (uint8_t bData) {m_bSwitches = bData;}
  uint8_t GetSwitches() const {return m_bSwitches;}
  // Dump the device state for the user ...
  void ShowDevice (ostringstream &ofs) const;

  // Private member data...
protected:
  uint8_t m_bSwitches;        // current switch register settings
};

