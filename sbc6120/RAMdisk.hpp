//++
// RAMdisk.hpp -> SBC6120 RAM disk emulation
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
// 21-Aug-22  RLA   New file.
//--
#pragma once
#include <assert.h>             // assert() ...
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "ImageFile.hpp"        // CDiskImageFile, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // generic CMemory definitions
using std::string;              // ...


class CRAMdisk : public CMemory {
  //++
  // SBC6120 style RAM disk emulation ...
  //--
  friend class CGenericMemory;

  // Constants and magic numbers ...
public:
  enum {
    RAM512_SIZE   = 524288UL, // size of 512K SRAM chip, in bytes
    RAM128_SIZE   = 131072UL, //  "   "  128K   "    "   "    "
    BANK_SIZE     = 4096,     // each RAM disk bank is 4K bytes
    RAM512_BANKS  =  128,     // number of 4K banks in a 512K chip
    RAM128_BANKS  =   32,     //   "     "  "   "    " " 128K   "
    NDRIVES       =    8,     // number of drives supported
  };

public:
  // Constructor and destructor ...
  CRAMdisk ();
  virtual ~CRAMdisk();
private:
  // Disallow copy and assignments!
  CRAMdisk (const CRAMdisk &) = delete;
  CRAMdisk& operator= (CRAMdisk const &) = delete;

  // CPU memory access functions ...
public:
  virtual word_t CPUread (address_t a) const override;
  virtual void CPUwrite (address_t a, word_t d) override;
  // Breakpoints in RAMdisk aren't implemented!
  virtual bool IsBreak (address_t a) const override {return false;}

  // Basic memory properties ...
public:
//inline size_t Size() const {return m_cwMemory;}
  // Return true if the backup battery is low ...
  bool IsBatteryLow() const {return false;}

  // Other RAM disk routines ...
public:
  // Load the RAM disk address register (LDAR IOT) ...
  void LoadDiskAddress (word_t wDAR) {m_bDAR = wDAR & 0177;}
  word_t GetDiskAddress() const {return m_bDAR;}
    // Attach the emulated drive to a disk image file ...
  bool Attach (uint8_t nUnit, const string &sFileName, uint32_t lCapacity);
  void Detach (uint8_t nUnit);
  void DetachAll();
  // Return TRUE if the drive is attached (online) ...
  bool IsAttached (uint8_t nUnit) const
    {assert(nUnit < NDRIVES);  return m_apImages[nUnit]->IsOpen();}
  // Return the external file that we're attached to ...
  string GetFileName (uint8_t nUnit) const
    {assert(nUnit < NDRIVES);  return IsAttached(nUnit) ? m_apImages[nUnit]->GetFileName() : "";}
  // Return the capacity of the drive, IN BANKS! ...
  uint32_t GetCapacity (uint8_t nUnit) const
    {assert(nUnit < NDRIVES);  return m_apImages[nUnit]->GetCapacity();}
  // Return TRUE if this unit is read only ...
  bool IsReadOnly (uint8_t nUnit) const
    {assert(nUnit < NDRIVES);  return m_apImages[nUnit]->IsReadOnly();}
  // Report RAM disk status and attached units ...
  void ShowStatus (ostringstream &ofs) const;

  // Local routines ...
private:
  // Calculate the memory address for a RAM disk access ...
  uint8_t *GetAddress (address_t a) const;
  // Read or write the RAM disk image from or to a file ...
  bool ReadImage (uint8_t nUnit);
  void WriteImage (uint8_t nUnit);

  // Members ...
private:
  CDiskImageFile *m_apImages[NDRIVES];    // RAM disk image file(s)
  uint8_t        *m_apBuffers[NDRIVES];   // RAM disk data buffers
  uint8_t         m_bDAR;                 // disk address register
};
