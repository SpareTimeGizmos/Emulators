//++
// MemoryMap.hpp -> SBC6120 memory mapping class
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
// 21-aug-22  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // generic memory definitions
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic I/O device definitions
using std::string;              // ...
class CHD6120;                  // ...


class CMemoryMap : public CDevice
{
  //++
  // SBC6120 Memory Mapping Register ...
  //--

public:
  enum _MAPPING_MODE {
    // Memory mapping modes ...
    MAP_ROMRAM    = 0,        // ROM direct, RAM indirect
    MAP_RAMROM    = 1,        // RAM direct, ROM indirect
    MAP_RAMONLY   = 2,        // RAM for everything
    MAP_RAMDISK   = 3,        // RAM direct, RAM disk indirect
  };
  typedef enum _MAPPING_MODE MAPPING_MODE;

public:
  // Constructor and destructor...
  CMemoryMap (word_t nIOT, C6120 *pCPU, CMemory *pMainMemory, CMemory *pPanelMemory, CMemory *pEPROM, CMemory *pRAMdisk);
  virtual ~CMemoryMap() {};
private:
  // Disallow copy and assignments!
  CMemoryMap (const CMemoryMap &) = delete;
  CMemoryMap& operator= (CMemoryMap const &) = delete;

  // Standard I/O device operations ...
public:
  virtual void ClearDevice() override;
  virtual bool DevIOT (word_t wIR, word_t &wAC, word_t &wPC) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Other CMemoryMap methods ...
public:
  void MasterClear();

  // Private member data...
protected:
  C6120     *m_pCPU;          // CPU object (so we can change memory mapping)
  CMemory   *m_pMainMemory;   // pointer to the main memory space
  CMemory   *m_pPanelMemory;  //  "   "  "   "  control panel memory
  CMemory   *m_pEPROM;        //  "   "  "   "  EPROM memory space
  CMemory   *m_pRAMdisk;      //  "   "  "   "  RAM disk
  MAPPING_MODE m_Mode;        // memory mapping mode
};
