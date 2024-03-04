//++
// DECfile11.hpp -> DEC Absolute Loader Paper Tape Routines
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
//   The CDECfile11 class adds routines to load and save paper tape images in
// the DEC PDP-11 absolute loader format to the CGenericMemory class.  That's
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

class CDECfile11 {
  //++
  // DEC PDP-11 Absolute Loader file support routines ...
  //--

  // No constructor or destructor here!

  // Load or save the memory in DEC PDP11 absolute loader format ...
public:
  static int32_t LoadPaperTape (CGenericMemory *pMemory, string sFileName);
  static int32_t SavePaperTape (CGenericMemory *pMemory, string sFileName, address_t wBase=0, size_t cbBytes=0);

  // Private methods ...
private:
  // Load or save segments of memory in DEC PDP11 absolute loader format ...
  static bool GetWord (FILE *pFile, uint16_t &wData, uint8_t &bChecksum);
  static void PutWord (FILE *pFile, uint16_t  wData, uint8_t &bChecksum);
  static int32_t LoadPaperTape (uint8_t *pData, string sFileName, size_t cbLimit);
  static int32_t SavePaperTape (const uint8_t *pData, string sFileName, size_t cbBytes=0, uint16_t wAddress=0);
};
