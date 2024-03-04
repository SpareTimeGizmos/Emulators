//++
// MemoryMap.hpp -> CMemoryControl (SBCT11 memory control register) class
//                  CMemoryMap (SBCT11 memory mapping) class
//
// LICENSE:
//    This file is part of the emulator library project.  SBCT11 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBCT11 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBCT11.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This header defines two classes - CMemoryControl, which is a CDevice
// derived class that implements the SBCT11 memory control (MEMC) and the
// non-existent memory trapping/control (NXMCS) registers.
// 
//   This header also defines the CMemoryMap class, which is derived from
// CMemory and implements the SBCT11 memory mapping hardware.  This takes
// a raw address from the T11 chip and decides whether RAM, ROM, or some
// I/O device should be selected.
//
// REVISION HISTORY:
//  6-JUL-22  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // generic memory definitions
#include "Device.hpp"           // generic I/O device definitions
class CDeviceMap;               // ...
class CDCT11;                   // ...

class CMemoryControl : public CDevice {
  //++
  // SBCT11 Memory Control (MEMC) and NXM control/status (NXMCS) Registers ...
  //--

public:
  enum {
    // MEMC and NXMCS offsets ...
    MEMC      = 0,      // MEMC is first, at offset zero
    NXMCS     = 2,      // NXMCS is second, at offset two
    PORTS     = 4,      // total of 4 bytes of address space used
    // Memory control register bits ...
    MEMC_RAM  = 0100,   // set to enable RAM mapping mode
    // NXM control/status bits ...
    NXMCS_NXM = 0200,   // set when a non-existent memory reference occurs
    NXMCS_NXE = 0100,   // set to enable NXM trap via HALT
  };

public:
  // Constructor and destructor...
  CMemoryControl (address_t nPort)
    : CDevice("MCR", "MCR11", "Memory Control Registers", INOUT, nPort, PORTS)
    {PowerOn();}
  virtual ~CMemoryControl() {};
private:
  // Disallow copy and assignments!
  CMemoryControl (const CMemoryControl&) = delete;
  CMemoryControl& operator= (CMemoryControl const &) = delete;

  // Standard I/O device operations ...
  //   Note that BCLR (INIT) does NOT AFFECT either the MEMC nor the NXMCS
  // registers.  They are only cleared by power up.  Also note that this
  // device has no sense inputs, flag outputs, and doesn't use events.
public:
  virtual word_t DevRead (address_t nPort) override;
  virtual void DevWrite (address_t nPort, word_t bData) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Special MCR methods ...
public:
  // Simulate a power up clear!
  void PowerOn() {m_fRAM = m_fNXE = m_fNXM = false;}
  // Return true if RAM or ROM mode is selected ...
  bool IsRAM() const {return  m_fRAM;}
  bool IsROM() const {return !m_fRAM;}
  // Return true if NXM trapping is enabled ...
  bool IsNXE() const {return m_fNXE;}
  //   SET the NXM error bit.  Note that this bit can only be cleared by
  // writing to the NXMCS register!
  void SetNXM() {m_fNXM = true;}
  bool IsNXM() const {return m_fNXM;}

  // Private member data...
protected:
  //   Notice that, although the MEMC and NXMCS are technically 8 bit registers,
  // only these three bits are actually implemented!
  bool    m_fRAM;       // TRUE if RAM mode is selected
  bool    m_fNXE;       // TRUE if NXM trapping is enabled
  bool    m_fNXM;       // TRUE if a NXM error has occurred
};


class CMemoryMap : public CMemory {
  //++
  // SBCT11 Memory Mapping logic ...
  //--

public:
  enum _CHIP_SELECT {
    // Chip select (memory space) names ...
    CS_NXM        = 0,  // unmapped address
    CS_RAM        = 1,  // the SRAM is selected
    CS_ROM        = 2,  //  "  EPROM "   "  "
    CS_IOPAGE     = 3,  // some I/O device is selected
  };
  typedef enum _CHIP_SELECT CHIP_SELECT;

public:
  // Constructor and destructor ...
  CMemoryMap (CGenericMemory *pRAM, CGenericMemory *pROM, CDeviceMap *pIOpage, CMemoryControl *pMCR);
  virtual ~CMemoryMap() {};
private:
  // Disallow copy and assignments!
  CMemoryMap (const CMemoryMap &) = delete;
  CMemoryMap& operator= (CMemoryMap const &) = delete;

public:
  // Set the CPU object (for NXM trapping) ...
  void SetCPU (class CDCT11 *pCPU) {m_pCPU = pCPU;}
  // CPU memory access functions ...
  virtual word_t CPUread (address_t a) const override;
  virtual void CPUwrite (address_t a, word_t d) override;
  virtual bool IsBreak (address_t a) const override;
  virtual bool IsIO (address_t a) const;
  // Figure out what address space should be selected ...
  static CHIP_SELECT ChipSelect(address_t a, bool fRAM, bool fNXE);
  // Return the name of the memory associated with a CHIP_SELECT ...
  static const char *GetChipName (CHIP_SELECT nSelect);
  // Clear all I/O devices (equivalent to a PDP11 BCLR!) ...
  void ClearDevices();

  // Local methods ...
private:
  // Call the DCT11's external halt request, if NXM trapping is enabled.
  void NXMtrap (address_t wAddress) const;

  // SBC1802 Memory Map internal state ...
private:
  CDCT11         *m_pCPU;     // the CPU that owns this memory map
  CGenericMemory *m_pRAM;     // a 64K SRAM space
  CGenericMemory *m_pROM;     // and a 64K EPROM space
  CDeviceMap     *m_pIOpage;  // memory mapped I/O devices
  CMemoryControl *m_pMCR;     // memory control register
};
