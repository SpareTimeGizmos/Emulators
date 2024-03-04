//++
// DECfile8.hpp -> DEC Absolute Loader Paper Tape Routines
//
//   Copyright (C) 1999-2022 by Spare Time Gizmos.  All rights reserved.
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
//   The CDECfile8 class adds routines to load and save paper tape images in
// the DEC PDP-8 absolute loader format to the CGenericMemory class.  That's
// it - nothing more!
// 
//   Note that this class cannot be instantiated.  It simply contains a few
// static routines that you're expected to call when you need them.
//
// REVISION HISTORY:
//  1-AUG-22  RLA   Split from the Memory.hpp file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // CGenericMemory class declaration
using std::string;              // ...

// Standard extensions for DEC absolute loader files ...
#define DEFAULT_PAPERTAPE_FILE_TYPE ".ptp"

class CDECfile8 {
  //++
  // DEC PDP-8 Absolute Loader file support routines ...
  //--

  // No constructor or destructor here!

public:
  // Load or save memory in DEC PDP8 absolute loader format ...
  static int32_t LoadPaperTape (CGenericMemory *pMemory, string sFileName);
  static int32_t SavePaperTape (CGenericMemory *pMemory, string sFileName, address_t wBase=0, size_t cbBytes=0);
  // Load or save memory as two Intel .hex EPROM images ...
  static int32_t Load2Intel (CGenericMemory *pMemory, string sFileNameHigh, string sFileNameLow, address_t wBase=0, size_t cbLimit=0, address_t wOffset=0);
  static int32_t Save2Intel(CGenericMemory* pMemory, string sFileNameHigh, string sFileNameLow, address_t wBase=0, size_t cwLimit=0);

  // Private methods ...
private:
  static bool GetByte  (FILE *pFile, uint8_t &bData);
  static bool GetFrame (FILE *pFile, uint16_t &wFrame, uint16_t &wChecksum);
  static int32_t LoadSegment (FILE *pFile, CGenericMemory *pMemory, uint16_t &wFrame, uint16_t &wChecksum);
};
