//++
// MemoryMap.cpp -> CMemoryControl (SBCT11 MEMC/NXMCS registers) 
//                  CMemoryMap (SBCT11 memory mapping)
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
//   This module implements the memory mapping hardware that's unique to the
// SBCT11.  The SBCT11 has a full 32KW (that's 64K bytes!) of RAM and also a
// full 32KW of EPROM/ROM.  The DCT11 has no MMU and the PDP11 address space
// is limited to 16 bits or 64K bytes.  That means either the RAM or the ROM
// alone would be enough to fill the entire address space, and without some
// kind of mapping scheme it would be impossible to use all of both.
//
//   The SBCT11 has a pretty trivial mapping scheme controlled by a single bit,
// the RAM bit in the MEMC register.  When set this bit enables the (mostly)
// RAM memory map and when cleared this bit enables the mostly EPROM memory map.
//
//	ADDRESS RANGE	RAM=1    RAM=0      SIZE
//	--------------	-------- -------- ---------
//	000000..001777	RAM	 RAM	   1K bytes
//	001000..167777	RAM	 EPROM	  59K bytes
//	170000..170377	NXM      NXM	  256 bytes
//	170400..175777	EPROM	 EPROM	   3K bytes
//	176000..176377	RAM	 RAM	  256 bytes
//	176400..177777	IOPAGE	 IOPAGE   768 bytes
//
//   Notice that the 59K byte block froom 001000 to 167777 is the only part
// that's affected by the RAM bit.  The first 1K bytes are always mapped to
// RAM for vectors and a temporary disk/TU58 buffer.  The section from
// 170000 to 175777 is always mapped to EPROM.  This is used for startup code
// and for subroutines that need to be accessible in either mapping mode.
//
//   The block of RAM from 176000 to 176377 is reserved specifically for use by
// the firmware as scratch space and, although it's always accessible, user 
// programs shouldn't mess with it.  And lastly, addresses from 176400 and up
// are reserved for I/O devices.
//
//  One last comment - THERE IS NO ADDRESS TRANSLATION HARDWARE in the SBCT11!
// RAM and EPROM are both addressed from 000000 to 177777 and that never changes.
// If the T11 outputs address 012345 then address 012345 is applied to both the
// RAM and EPROM chips.  The only thing that the RAM MAP signal changes is which
// memory chips are selected.
//    
// REVISION HISTORY:
//  7-JUL-22  RLA   New file.
// 18-JUL-22  RLA   Add ClearDevices() ...
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
#include "sbct11.hpp"           // global declarations for this project
#include "MemoryTypes.h"        // address_t and word_t data types
#include "DeviceMap.hpp"        // memory mapped I/O device mapping
#include "Memory.hpp"           // CMemory and CGenericMemory declarations
#include "Device.hpp"           // generic I/O device definitions
#include "CPU.hpp"              // generic CPU definitions
#include "DCT11.hpp"            // DCT11 specific definitions
#include "MemoryMap.hpp"        // declarations for this module


word_t CMemoryControl::DevRead (address_t nPort)
{
  //++
  //   Read the memory control (MEMC)or the NXM status (NXMS) registers.  Both
  // the RAM and NXE bits can be read back, as well as the NXM flag.  Note that
  // in the real SBCT11 only these two bits are driven, and the other bus bits
  // will all float to some random values.  Here we assume they'll read as 1s..
  //--
  assert(nPort >= GetBasePort());
  if ((nPort-GetBasePort()) == MEMC) {
    uint8_t bData = 0377;
    if (!m_fRAM) CLRBIT(bData, MEMC_RAM);
    return bData;
  } else if ((nPort-GetBasePort()) == NXMCS) {
    uint8_t bData = 0377;
    if (!m_fNXE) CLRBIT(bData, NXMCS_NXE);
    if (!m_fNXM) CLRBIT(bData, NXMCS_NXM);
    return bData;
  } else
    return 0377;
}

void CMemoryControl::DevWrite (address_t nPort, word_t bData)
{
  //++
  //   And this method handles writing to the MEMC or NXMC registers.  Writing
  // MEMC just updates the RAM bit; all other bits are ignored.  Writing the
  // NXMC register also just updates the NXE bit and all other bits are ignored,
  // HOWEVER, setting NXE to zero has the side effect of also clearing the NXM
  // bit, if set.  This is the ONLY way the NXM bit can be cleared!
  //--
  assert(nPort >= GetBasePort());
  if ((nPort-GetBasePort()) == MEMC) {
    m_fRAM = ISSET(bData, MEMC_RAM);
    LOGF(TRACE, "MCR %s mode", m_fRAM ? "RAM" : "ROM");
  } else if ((nPort-GetBasePort()) == NXMCS) {
    m_fNXE = ISSET(bData, NXMCS_NXE);
    if (!m_fNXE) m_fNXM = false;
  }
}

void CMemoryControl::ShowDevice (ostringstream &ofs) const
{
  //++
  // Show the current RAM, NXE and NXM status for debugging.
  //--
  ofs << FormatString("%s MODE, NXM HALT %s",
    (m_fRAM ? "RAM" : "ROM"), (m_fNXE ? "ENABLED" : "DISABLED"));
  if (m_fNXM) ofs << ", NXM DETECTED";
  ofs << std::endl;
}

CMemoryMap::CMemoryMap (CGenericMemory *pRAM, CGenericMemory *pROM,
                        CDeviceMap *pIOpage, CMemoryControl *pMCR)
{
  //++
  //   The constructor for CMemoryMap assembles and remembers all the
  // components required for the memory subsystem, including two separate
  // CGenericMemory spaces, one for RAM and one for EPROM.  It also needs
  // to know about the PDP11 I/O page, and the MEMC/NXMCS object.  The
  // latter can also be accessed thru the I/O page, but we need to access
  // it directly so we can query the RAM and NXE bits, as well as set the
  // NXM flag when rquired.
  //--
  assert((pRAM != NULL)  &&  (pROM != NULL));
  assert((pIOpage != NULL) && (pMCR != NULL));
  m_pRAM = pRAM;  m_pROM = pROM;  m_pCPU = NULL;
  m_pMCR = pMCR;  m_pIOpage = pIOpage;
}

CMemoryMap::CHIP_SELECT CMemoryMap::ChipSelect (address_t a, bool fRAM, bool fNXE)
{
  //++
  //   This routine will figure out what memory space - RAM, ROM, I/O, or
  // none of the above - should be selected by a given memory address and the
  // current memory mode selected by the RAM bit in the MEMC register.  This
  // is exactly the function of the memory GAL in the SBCT11 design, and the
  // ultimate gold standard for this behavior is the PLD source.
  // 
  //   Note that in the SBCT11 the memory address is NEVER modified regardless
  // of which device is selected!
  //
  //   It's worth sparing a moment to think about the expense, in CPU time,
  // of this function.  After all, this function will be called for EVERY
  // SINGLE MEMORY access performed by the CPU emulation!  That's a lot, and
  // you could undoubtedly find ways to optimize this considerably.  It's
  // not really necessary though, because this implementation seems to be
  // "fast enough."  It's a testament to how fast modern PCs have become that
  // this is so...
  //--
  if (a < ROM_BASE_0) 
    // Addresses 000000..001777 always select the RAM, regardless ...
    return CS_RAM;
  else if (a <= RAMTOP) 
    //   Addresses 002000..167777 are mapped to either RAM or EPROM, depending
    // on the state of the RAM bit in the MEMC register.
    return fRAM ? CS_RAM : CS_ROM;
  else if (a <= 0170377)
    //   In RAM mode, and only in RAM mode, addresses right above the top of
    // RAM, from 170000 thru 170377, cause a NXM trap.  This is so that PDP11
    // software that sizes memory by reading upward looking for a bus timeout
    // will work correctly.  In ROM mode, or if NXM trapping is disabled, then
    // this chunk maps to EPROM normally.
    return (fRAM && fNXE) ? CS_NXM : CS_ROM;
  else if (a <= ROMTOP)
    // Addresses 170400..175777 are always mapped to EPROM ...
    return CS_ROM;
  else if (a <= SCRATCH_TOP)
    // Addresses 176000..176377 are the scratchpad RAM area ...
    return CS_RAM;
  else
    // Everything from 176400 and up is the I/O area ...
    return CS_IOPAGE;
}

const char *CMemoryMap::GetChipName (CHIP_SELECT nSelect)
{
  //++
  //   Return the name associated with a given CHIP_SELECT.  This is used
  // by the UserInterface to print out a memory map.
  //--
  switch (nSelect) {
    case CS_NXM:    return "NXM HALT";
    case CS_RAM:    return "SRAM";
    case CS_ROM:    return "EPROM";
    case CS_IOPAGE: return "IOPAGE";
    default:        return "???";
  }
}

void CMemoryMap::NXMtrap (address_t wAddress) const
{
  //++
  //   This routine will request that the DCT11 CPU halt if NXM trapping is
  // enabled.  The T11 doesn't actually halt at all, but instead traps to
  // the restart address.  The firmware then figures out what happened and
  // simulates a bus timeout trap.
  //--
  if (m_pMCR->IsNXE() && !m_pMCR->IsNXM()) {
    m_pMCR->SetNXM();
    if (m_pCPU != NULL) {
      LOGF(TRACE, "NXM address %06o at PC %06o", wAddress, m_pCPU->GetPC());
      m_pCPU->HaltRequest();
    }
  }
}

word_t CMemoryMap::CPUread (address_t a) const
{
  //++
  //   This method is called for _every_ CPU memory read operation (yikes!).
  // It runs the memory mapping algorithm to figure out which address space
  // and chip should be selected, and then delegates the request to the
  // corresponding object.
  //--
  CDevice *pDevice;
  switch (ChipSelect(a, g_pMCR->IsRAM(), g_pMCR->IsNXE())) {
    case CS_ROM: return m_pROM->CPUread(a);
    case CS_RAM: return m_pRAM->CPUread(a);
    case CS_IOPAGE:
      if ((pDevice = m_pIOpage->Find(a)) != NULL)
        return pDevice->DevRead(a);
      else
        NXMtrap(a);
      return WORD_MAX;
    default:
      NXMtrap(a);  return WORD_MAX;
  }
}

void CMemoryMap::CPUwrite (address_t a, word_t d)
{
  //++
  // The same idea as CPUread(), except this time write to a location...
  //--
  CDevice *pDevice;
  switch (ChipSelect(a, g_pMCR->IsRAM(), g_pMCR->IsNXE())) {
    case CS_ROM: /* can't write to ROM! */  break;
    case CS_RAM: m_pRAM->CPUwrite(a, d);    break;
    case CS_IOPAGE:
      if ((pDevice = m_pIOpage->Find(a)) != NULL)
        pDevice->DevWrite(a, d);
      else
        NXMtrap(a);
      break;
    default:
      NXMtrap(a);  break;
  }
}

void CMemoryMap::ClearDevices()
{
  //++
  //   This method will clear all I/O devices, including the memory control
  // itself.  It's the equivalent of a PDP-11 bus clear (BCLR) operation and
  // is invoked by the RESET instruction ...
  //--
  m_pIOpage->ClearAll();
}

bool CMemoryMap::IsBreak (address_t a) const
{
  //++
  //   Return TRUE if a breakpoint is set on the specified memory address.
  // This works only for RAM and EPROM, however it needs to be careful to
  // figure out which one of those two is currently selected first.  Break
  // points are not supported, and we always return FALSE, for I/O devices.
  //--
  switch (ChipSelect(a, g_pMCR->IsRAM(), g_pMCR->IsNXE())) {
    case CS_ROM: return m_pROM->IsBreak(a);
    case CS_RAM: return m_pRAM->IsBreak(a);
    default: return false;
  }
}

bool CMemoryMap::IsIO (address_t a) const
{
  //++
  //   Return true if the specified (and mapped) address is an I/O device and
  // false if it is either RAM or EPROM.  This is pretty easy to figure out.
  //--
  return ChipSelect(a, g_pMCR->IsRAM(), g_pMCR->IsNXE()) == CS_IOPAGE;
}
