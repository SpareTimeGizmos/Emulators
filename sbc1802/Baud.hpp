//++
// Baud.hpp -> SBC1802 baud rate generator definitions
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the SBC1802 emulator project.  SBC1802 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBC1802 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBC1802.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This class emulates the COM8136 baud rate generator used in the SBC1802.
//
// REVISION HISTORY:
// 23-JUN-22  RLA   New file.
//--
#pragma once
#include <stdint.h>
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
using std::string;              // ...

class CBaud : public CDevice {
  //++
  // SBC1802 baud rate generator emulation ...
  //--

public:
  enum {
    //   Bits to baud rate mapping for the COM8136.  Note that these may appear
    // a bit strange if you read the 8116/8136 datasheet - that's because the
    // order of the bits is reversed on the SBC1802.
    BAUD_50       =  0,   //     50 bps
    BAUD_1800     =  1,   //  1,800 bps
    BAUD_150      =  2,   //    150 bps
    BAUD_4800     =  3,   //  4,800 bps
    BAUD_110      =  4,   //    110 bps
    BAUD_2400     =  5,   //  2,400 bps
    BAUD_600      =  6,   //    600 bps
    BAUD_9600     =  7,   //  9,600 bps
    BAUD_75       =  8,   //     75 bps
    BAUD_2000     =  9,   //  2,000 bps
    BAUD_300      = 10,   //    300 bps
    BAUD_7200     = 11,   //  7,200 bps
    BAUD_1345     = 12,   //    134.5 bps
    BAUD_3600     = 13,   //  3,600 bps
    BAUD_1200     = 14,   //  1,200 bps
    BAUD_19200    = 15,   // 19,200 bps
    BAUD_SLU0     = 0x0F, // mask for SLU0 baud setting
    BAUD_SLU1     = 0xF0, //  "    "  SLU1  "    "   "
  };

public:
  // Constructor and destructor...
  CBaud (address_t nPort);
  virtual ~CBaud() {};
private:
  // Disallow copy and assignments!
  CBaud (const CBaud &) = delete;
  CBaud& operator= (CBaud const &) = delete;

  // Public COM8136 properties ...
public:
  uint8_t GetBaud0() const {return m_bBaud0;}
  uint8_t GetBaud1() const {return m_bBaud1;}

  // CDevice methods for the baud rate generator ...
public:
  // Write to a device register ...
  void DevWrite (address_t nPort, word_t bData) override;
  // Dump the device state for the user ...
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Private methods ...
private:
  static string DecodeBaud (uint8_t bBaud);

  // Private member data...
protected:
  uint8_t m_bBaud0;             // current baud rate for SLU0
  uint8_t m_bBaud1;             //   "  "    "   "    "  SLU1
};
