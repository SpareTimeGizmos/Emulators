//++
// MemoryMap.hpp -> CMemoryControl (SBC1802 memory control register) class
//                  CMemoryMap (SBC1802 memory mapping) class
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
//   This header defines two classes - CMemoryControl, which is a CDevice
// derived class that implements the SBC1802 memory control register, and
// also CMemoryMap, which is a CMemory derived class that implements the
// SBC1802 memory mapping hardware.
//
// REVISION HISTORY:
// 16-JUN-22  RLA   New file.
// 19-JUN-22  RLA   Split out CMemoryControl
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // generic memory definitions
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic I/O device definitions
using std::string;              // ...
class CCDP1877;                 // ...
class CCDP1879;                 // ...


class CMemoryControl : public CDevice {
  //++
  // SBC1802 Memory Control Register ...
  //--

public:
  enum {
    // Memory control register bits ...
    MCR_BOOT  = 0x00,   // boot time memory map
    MCR_ROM0  = 0x04,   // EPROM with RAM chip 0 mapped
    MCR_ROM1  = 0x05,   //   "     "   "    "  1    "
    MCR_MDOS  = 0x06,   // MicroDOS compatible memory map
    MCR_ELOS  = 0x07,   // ElfOS compatible memory map
    MCR_MASK  = 0x07,   // mask for all mapping bits
    //   The MCR contains a fourth writable bit which service as a master interrupt
    // enable. This is unfortunately necessary because the CDP1877 PIC doesn't have
    // a clear/reset input and it could be in any state after the SBC1802 is reset,
    // including with an active interrupt request.  Since the 1802 CPU also resets
    // to an interrupt enabled state, this is potentially disasterous.  The MCR
    // master interrupt enable is cleared by a reset and blocks CDP1877 interrupts
    // until the software sets it.  And yes, this has nothing to do with the memory
    // mapping - I just happened to have a spare bit in the MCR...
    MCR_MIEN  = 0x80,   // set to enable CDP1877 PIC interrupts
    //   The MCR can be read as well as written - this allows an ISR to save and
    // later restore the map in use!  The bus buffer used has six drivers, so in
    // addition to the three MCR bits and the MIEN bit, there are two extra bits 
    // present when the MCR is read.  One gives the status of the memory backup
    // battery and the other gives the current state of the 1802's interrupt
    // request input.  The latter allows the PIC to be tested without actually
    // needing to cause an interrupt.
    // only.
    MCR_BBOK  = 0x40,   // set when the backup battery is good
    MCR_PIRQ  = 0x08,   // CLEARED when the PIC is requesting an interrupt
  };

public:
  // Constructor and destructor...
  CMemoryControl (uint16_t nPort, CCDP1877 *pPIC);
  virtual ~CMemoryControl() {};
private:
  // Disallow copy and assignments!
  CMemoryControl (const CMemoryControl&) = delete;
  CMemoryControl& operator= (CMemoryControl const &) = delete;

  // Standard I/O device operations ...
public:
  virtual void ClearDevice() override;
  virtual word_t DevRead (address_t nPort) override;
  virtual void DevWrite (address_t nPort, word_t bData) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Other special MCR methods ...
public:
  inline uint8_t GetMap() const {return m_bMap;}
  static const char *MapToString (uint8_t bMap);

  // Private member data...
protected:
  uint8_t         m_bMap;     // current memory mapping mode selected
  CCDP1877       *m_pPIC;     // programmable interrupt controller
};


class CMemoryMap : public CMemory {
  //++
  // SBC1802 Memory Mapping Hardware ...
  //--

public:
  enum _CHIP_SELECT {
    // Chip select (memory space) names ...
    CS_RAM,             // the SRAM is selected
    CS_ROM,             //  "  EPROM "   "  "
    CS_RTC,             // the CDP1879 real time clock is selected
    CS_PIC,             //  "  CRP1877 interrupt controller "  "
    CS_MCR,             // the memory control register is selected
    CS_NONE             // unmapped addresses (do not use!)
  };
  typedef enum _CHIP_SELECT CHIP_SELECT;

public:
  // Constructor and destructor ...
  CMemoryMap (CGenericMemory *pRAM, CGenericMemory *pROM, CMemoryControl *pMCR, CCDP1879 *pRTC, CCDP1877 *pPIC);
  virtual ~CMemoryMap() {};
private:
  // Disallow copy and assignments!
  CMemoryMap (const CMemoryMap &) = delete;
  CMemoryMap& operator= (CMemoryMap const &) = delete;

public:
  // CPU memory access functions ...
  virtual word_t CPUread (address_t a) const;
  virtual void CPUwrite (address_t a, word_t d);
  virtual bool IsBreak (address_t a) const;
  virtual bool IsIO (address_t a) const;

public:
  // Public CMeoryMap methods ...
  static CHIP_SELECT ChipSelect (uint8_t bMap, address_t &a);
  static const char *ChipToString (CHIP_SELECT bChip);

  // SBC1802 Memory Map internal state ...
private:
  CCPU           *m_pCPU;     // the CPU that owns this memory map
  CGenericMemory *m_pRAM;     // a 64K SRAM space
  CGenericMemory *m_pROM;     // and a 32K EPROM space
  CMemoryControl* m_pMCR;     // memory control register
  CCDP1879       *m_pRTC;     // the CDP1879 real time clock
  CCDP1877       *m_pPIC;     // the CDP1877 programmable interrupt controller
};
