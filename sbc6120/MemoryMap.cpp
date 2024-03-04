//++
// MemoryMap.cpp -> SBC6120 memory mapping emulation
//
//   Copyright (C) 1999-2022 by Spare Time Gizmos.  All rights reserved.
//
// DESCRIPTION:
//   The SBC6120 has three memory subsystems - 64K words of twelve bit RAM, 32K
// words of 12 bit EPROM, and up to 4Mb of 8 bit of SRAM with a battery backup
// for a RAM disk.  The HD6120 on the other hand, has only two memory spaces -
// panel memory and main memory, and each of these is limited to 32K words. The
// EPROM is a problem because the PDP-8 instruction set makes it difficult, if
// not impossible, to get by without some read/write memory in every 4K field.
//
//   The SBC6120 implements a simple memory mapping scheme to allow all three
// memory subsystems to fit in the available address space.  The memory map in
// use is selected by four IOT instructions, MM0, MM1, MM2 and (what else ?)
// MM3.  Memory map changes take place immediately with the next instruction
// fetch - there's no delay until the next JMP the way there is with a CIF
// instruction.
//
//      IOT       Function
//      --------  ----------------------------------------------------------
//      MM0 6400  Select ROM/RAM  memory map (0)
//      MM1 6401  Select RAM/ROM  memory map (1)
//      MM2 6402  Select RAM only memory map (2)
//      MM3 6403  Select RAM disk memory map (3)
//
// The four memory maps implemented by the SBC6120 are-
//
//   * Map 0 uses the EPROM for all direct memory accesses, including opcode
//     fetch, and uses the RAM for all indirect memory accesses.  This is the
//     mapping mode selected by the hardware after power on or a reset.
//
//   * Map 1 uses the RAM for all direct memory accesses, including opcode
//     fetch, and uses the EPROM for all indirect memory references.  This mode
//     is the "complement" of map 0, and it's used by the ROM firmware startup
//     code to copy the EPROM contents to RAM.
//
//   * Map 2 uses the RAM for all memory accessesand the EPROM is not used.
//     This is the normal mapping mode used after the firmware initialization.
//
//   * Map 3 is the same as map 2, except that the RAM disk memory is enabled
//     for all indirect accesses.  RAM disk memory is only eight bits wide and
//     reads and writes to this memory space only store and return the lower
//     byte of a twelve bit word.  This mode is used only while we're accessing
//     the RAM disk.
//
//   The memory mapping mode affects only HD6120 control panel memory accesses.
// Main memory is always mapped to RAM regardless of the mapping mode selected.
//    
// REVISION HISTORY:
// 21-AUG-22  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string>               // C++ std::string class, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "LogFile.hpp"          // emulator library message logging facility
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // CMemory and CGenericMemory declarations
#include "CPU.hpp"              // generic CCPU class declarations
#include "HD6120.hpp"           // HD6120 specific CPU declarations
#include "MemoryMap.hpp"        // declarations for this module
using std::string;              // too lazy to type "std::string..."!


CMemoryMap::CMemoryMap (word_t nIOT, C6120 *pCPU, CMemory *pMainMemory,
                        CMemory *pPanelMemory, CMemory *pEPROM, CMemory *pRAMdisk)
  : CDevice("MMAP", "MMAP", "Memory Mapping Register", INOUT, nIOT)
{
  //++
  //--
  assert((pCPU != NULL) && (pMainMemory != NULL) && (pEPROM != NULL));
  assert((pPanelMemory != NULL) && (pRAMdisk != NULL));
  m_pCPU = pCPU;  m_pMainMemory = pMainMemory;  m_pEPROM = pEPROM;
  m_pPanelMemory = pPanelMemory;  m_pRAMdisk = pRAMdisk;
  CMemoryMap::MasterClear();
}

void CMemoryMap::MasterClear()
{
  //++
  //   A hardware reset reverts to map 0 - EPROM for direct accesses and
  // opcode fetches, and panel RAM for indirect accesses...
  //--
  m_Mode = MAP_ROMRAM;
  m_pCPU->SetPanelDirect(m_pEPROM);
  m_pCPU->SetPanelIndirect(m_pPanelMemory);
}

void CMemoryMap::ClearDevice()
{
  //++
  //   ClearDevice(), which is invoked by a CAF instruction, does nothing to
  // memory mapping hardware.  It had better not - otherwise we'd crash any
  // time any program executed a CAF!
  //--
}

bool CMemoryMap::DevIOT (word_t wIR, word_t &wAC, word_t &wPC)
{
  //++
  //   This routine is called for any of the four memory map IOTs.  It simply
  // installs pointers to the correct address spaces into the panel direct and
  // panel indirect pointers in the CPU ...
  //--
  uint8_t nMap = wIR & 7;
  if (nMap > 3) return false;

  m_Mode = (MAPPING_MODE) nMap;
  switch (m_Mode) {
    case MAP_ROMRAM:
      // Select EPROM for direct addressing, panel RAM for indirect ...
      LOGS(TRACE, "ROM/RAM mapping selected");
      m_pCPU->SetPanelDirect(m_pEPROM);
      m_pCPU->SetPanelIndirect(m_pPanelMemory);
      break;

    case MAP_RAMROM:
      // Select panel RAM for direct addressing, EPROM for indirect ...
      LOGS(TRACE, "RAM/ROM mapping selected");
      m_pCPU->SetPanelDirect(m_pPanelMemory);
      m_pCPU->SetPanelIndirect(m_pEPROM);
      break;

    case MAP_RAMONLY:
      // Select panel RAM for all addressing ...
      LOGS(TRACE, "RAM only mapping selected");
      m_pCPU->SetPanelDirect(m_pPanelMemory);
      m_pCPU->SetPanelIndirect(m_pPanelMemory);
      break;

    case MAP_RAMDISK:
      // Select panel RAM for direct addressing, and RAM disk for indirect ...
      LOGS(TRACE, "RAM disk mapping selected");
      m_pCPU->SetPanelDirect(m_pPanelMemory);
      m_pCPU->SetPanelIndirect(m_pRAMdisk);
      break;
  }
  return true;
}

void CMemoryMap::ShowDevice (ostringstream& ofs) const
{
  //++
  // Show the memory mapping status for debugging ...
  //--
  ofs << FormatString("Mapping mode %d - ", m_Mode);
  switch (m_Mode) {
    case MAP_ROMRAM:  ofs << "panel direct EPROM, panel indirect RAM";    break;
    case MAP_RAMROM:  ofs << "panel direct RAM, panel indirect EPROM";    break;
    case MAP_RAMONLY: ofs << "RAM only";                                  break;
    case MAP_RAMDISK: ofs << "panel direct RAM, panel indirect RAM disk"; break;
  }
  ofs << std::endl;
}
