//++
// POST.hpp -> SBC6120 three LED POST code display
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
// REVISION HISTORY:
// 21-AUG-22  RLA   New file.
//--
#pragma once
#include <stdint.h>
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
using std::string;              // ...

class CPOST : public CDevice
{
  //++
  // Three LED POST display emulation ...
  //--

public:
  // Constructor and destructor...
  CPOST (word_t nIOT);
  virtual ~CPOST() {};
private:
  // Disallow copy and assignments!
  CPOST (const CPOST &) = delete;
  CPOST& operator= (CPOST const &) = delete;

  // Public POST display properties
public:
  // Get the last POST code displayed ...
  uint8_t GetPOST() const {return m_bPOST;}

  // CPOST device methods from CDevice ...
public:
  virtual void ClearDevice() override {m_bPOST = 0;}
  virtual bool DevIOT (word_t wIR, word_t &AC, word_t &wPC) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Private member data...
protected:
  uint8_t m_bPOST;              // last byte sent to the display
};
